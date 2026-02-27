// SPDX-License-Identifier: MIT
// run.rs --- execute a graph of interconnected processes
// Copyright (c) 2026 Jakob Kastelic

use std::collections::HashMap;
use std::env;
use std::io::{self, BufRead, BufReader, Write};
use std::process::{self, Child, Command, Stdio};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

// --------------------------------------------------------------------------
// helpers
// --------------------------------------------------------------------------

// Open a PTY pair. Returns (master_fd, slave_fd).
// We use this for child stdout so the child sees a TTY and uses
// line buffering rather than block buffering.
#[cfg_attr(target_os = "linux", link(name = "util"))]
extern "C" {}

#[cfg(unix)]
fn open_pty() -> Option<(std::os::fd::OwnedFd, std::os::fd::OwnedFd)> {
    use std::os::fd::FromRawFd;
    extern "C" {
        fn openpty(
            amaster: *mut i32,
            aslave: *mut i32,
            name: *mut i8,
            termp: *const u8,
            winp: *const u8,
        ) -> i32;
    }
    unsafe {
        let mut master: i32 = -1;
        let mut slave: i32 = -1;
        if openpty(
            &mut master,
            &mut slave,
            std::ptr::null_mut(),
            std::ptr::null(),
            std::ptr::null(),
        ) != 0
        {
            return None;
        }
        Some((
            std::os::fd::OwnedFd::from_raw_fd(master),
            std::os::fd::OwnedFd::from_raw_fd(slave),
        ))
    }
}

fn red_error(msg: &str) -> ! {
    eprintln!("\x1b[31mError:\x1b[0m {msg}");
    process::exit(1);
}

// --------------------------------------------------------------------------
// DOT subset parser
// --------------------------------------------------------------------------

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
enum Node {
    Stdin,
    Stdout,
    Prog(String),
}

impl Node {
    fn from_str(s: &str) -> Node {
        match s {
            "STDIN" => Node::Stdin,
            "STDOUT" => Node::Stdout,
            _ => Node::Prog(s.to_owned()),
        }
    }
}

fn strip_comments(src: &str) -> String {
    let mut out = String::with_capacity(src.len());
    let mut chars = src.chars().peekable();
    while let Some(c) = chars.next() {
        if c == '/' {
            match chars.peek() {
                Some('/') => {
                    chars.next();
                    for c2 in chars.by_ref() {
                        if c2 == '\n' {
                            out.push('\n');
                            break;
                        }
                    }
                }
                Some('*') => {
                    chars.next();
                    loop {
                        match chars.next() {
                            None => break,
                            Some('*') if chars.peek() == Some(&'/') => {
                                chars.next();
                                break;
                            }
                            Some('\n') => out.push('\n'),
                            _ => {}
                        }
                    }
                }
                _ => out.push(c),
            }
        } else {
            out.push(c);
        }
    }
    out
}

fn parse(src: &str) -> Vec<(Node, Node)> {
    let mut edges = Vec::new();
    let src = strip_comments(src);
    let flat = src.replace('\n', " ").replace('\r', " ");
    let raw_stmts: Vec<&str> = flat.split(';').collect();

    for (i, stmt) in raw_stmts.iter().enumerate() {
        let trimmed = stmt.trim();
        if trimmed.is_empty() {
            continue;
        }
        if i == raw_stmts.len() - 1 {
            red_error(&format!("statement not terminated with ';': `{trimmed}`"));
        }

        let parts = split_arrow(trimmed);
        if parts.len() < 2 {
            red_error(&format!(
                "expected at least two nodes separated by '->': `{trimmed}`"
            ));
        }
        for window in parts.windows(2) {
            let a = Node::from_str(window[0].trim());
            let b = Node::from_str(window[1].trim());
            edges.push((a, b));
        }
    }
    edges
}

fn split_arrow(s: &str) -> Vec<&str> {
    let bytes = s.as_bytes();
    let mut parts = Vec::new();
    let mut start = 0usize;
    let mut i = 0usize;
    let mut in_quote = false;
    while i < bytes.len() {
        if bytes[i] == b'"' {
            in_quote = !in_quote;
            i += 1;
        } else if !in_quote && i + 1 < bytes.len() && bytes[i] == b'-' && bytes[i + 1] == b'>' {
            parts.push(&s[start..i]);
            i += 2;
            start = i;
        } else {
            i += 1;
        }
    }
    parts.push(&s[start..]);
    parts
}

// --------------------------------------------------------------------------
// Line-oriented channel primitives
// --------------------------------------------------------------------------

fn make_pipe() -> (LineSender, LineReceiver) {
    let (tx, rx) = std::sync::mpsc::channel::<String>();
    (LineSender { tx }, LineReceiver { rx })
}

#[derive(Clone)]
struct LineSender {
    tx: std::sync::mpsc::Sender<String>,
}
struct LineReceiver {
    rx: std::sync::mpsc::Receiver<String>,
}

impl LineSender {
    fn send_line(&self, line: String) -> bool {
        self.tx.send(line).is_ok()
    }
}
impl LineReceiver {
    fn recv_line(&self) -> Option<String> {
        self.rx.recv().ok()
    }
}

// --------------------------------------------------------------------------
// argv parser
// --------------------------------------------------------------------------

fn parse_argv(cmd: &str) -> Vec<String> {
    let mut args = Vec::new();
    let mut current = String::new();
    let mut in_quote = false;
    for c in cmd.chars() {
        match c {
            '"' => in_quote = !in_quote,
            ' ' if !in_quote => {
                if !current.is_empty() {
                    args.push(current.clone());
                    current.clear();
                }
            }
            _ => current.push(c),
        }
    }
    if !current.is_empty() {
        args.push(current);
    }
    args
}

// --------------------------------------------------------------------------
// Shared state
// --------------------------------------------------------------------------

type ChildHandle = Arc<Mutex<Child>>;

struct Shared {
    error_flag: Arc<Mutex<bool>>,
    children: Arc<Mutex<Vec<ChildHandle>>>,
    stderr_lock: Arc<Mutex<()>>,
}

impl Shared {
    fn new() -> Arc<Self> {
        Arc::new(Shared {
            error_flag: Arc::new(Mutex::new(false)),
            children: Arc::new(Mutex::new(Vec::new())),
            stderr_lock: Arc::new(Mutex::new(())),
        })
    }

    fn is_failed(&self) -> bool {
        *self.error_flag.lock().unwrap()
    }

    /// Sets the error flag, kills all children, and immediately exits the process.
    fn fail(&self) -> ! {
        {
            let mut flag = self.error_flag.lock().unwrap();
            if *flag {
                // Someone else is already handling the exit
                thread::sleep(Duration::from_millis(100));
                process::exit(1);
            }
            *flag = true;
        }

        // Kill all registered child processes
        let children = self.children.lock().unwrap();
        for ch in children.iter() {
            // Use try_lock to avoid deadlocks with threads currently waiting on the child
            if let Ok(mut child) = ch.try_lock() {
                let _ = child.kill();
            }
        }

        // Final exit of the entire graph process
        process::exit(1);
    }

    fn print_stderr(&self, line: &str) {
        let _g = self.stderr_lock.lock().unwrap();
        let stderr = io::stderr();
        let mut err = stderr.lock();
        let _ = writeln!(err, "\x1b[31m[Process Error]:\x1b[0m {line}");
    }
}

// --------------------------------------------------------------------------
// Graph runner
// --------------------------------------------------------------------------

fn run(edges: &[(Node, Node)]) {
    let mut all_nodes: Vec<Node> = Vec::new();
    for (a, b) in edges {
        if !all_nodes.contains(a) {
            all_nodes.push(a.clone());
        }
        if !all_nodes.contains(b) {
            all_nodes.push(b.clone());
        }
    }

    let mut senders: Vec<LineSender> = Vec::new();
    let mut receivers: Vec<Option<LineReceiver>> = Vec::new();
    for _ in edges {
        let (s, r) = make_pipe();
        senders.push(s);
        receivers.push(Some(r));
    }

    let mut node_out: HashMap<Node, Vec<usize>> = HashMap::new();
    let mut node_in: HashMap<Node, Vec<usize>> = HashMap::new();
    for (i, (a, b)) in edges.iter().enumerate() {
        node_out.entry(a.clone()).or_default().push(i);
        node_in.entry(b.clone()).or_default().push(i);
    }

    let shared = Shared::new();
    let mut handles: Vec<thread::JoinHandle<()>> = Vec::new();

    // -- STDIN node ----------------------------------------------------------
    if let Some(out_indices) = node_out.get(&Node::Stdin) {
        let out_senders: Vec<LineSender> =
            out_indices.iter().map(|&i| senders[i].clone()).collect();
        let sh = Arc::clone(&shared);
        handles.push(thread::spawn(move || {
            let stdin = io::stdin();
            for line in stdin.lock().lines() {
                if sh.is_failed() {
                    return;
                }
                match line {
                    Ok(l) => {
                        let mut alive = false;
                        for s in &out_senders {
                            if s.send_line(l.clone()) {
                                alive = true;
                            }
                        }
                        if !alive {
                            return;
                        } // No more consumers
                    }
                    Err(_) => {
                        sh.fail();
                    }
                }
            }
        }));
    }

    // -- STDOUT node ---------------------------------------------------------
    if let Some(in_indices) = node_in.get(&Node::Stdout) {
        let stdout_lock = Arc::new(Mutex::<()>::new(()));
        for &ri in in_indices {
            let receiver = receivers[ri].take().expect("receiver consumed");
            let lock = stdout_lock.clone();
            let sh = Arc::clone(&shared);
            handles.push(thread::spawn(move || {
                let stdout = io::stdout();
                while let Some(line) = receiver.recv_line() {
                    if sh.is_failed() {
                        return;
                    }
                    let _guard = lock.lock().unwrap();
                    let mut out = stdout.lock();
                    if writeln!(out, "{line}").is_err() {
                        return;
                    }
                }
            }));
        }
    }

    // -- Program nodes -------------------------------------------------------
    for node in &all_nodes {
        let cmd_str = match node {
            Node::Prog(s) => s.clone(),
            _ => continue,
        };

        let argv = parse_argv(&cmd_str);
        if argv.is_empty() {
            red_error("empty command in graph");
        }

        let out_indices = node_out.get(node).cloned().unwrap_or_default();
        let in_indices = node_in.get(node).cloned().unwrap_or_default();
        let out_senders: Vec<LineSender> =
            out_indices.iter().map(|&i| senders[i].clone()).collect();
        let in_receivers: Vec<LineReceiver> = in_indices
            .iter()
            .map(|&ri| receivers[ri].take().expect("consumed"))
            .collect();

        let sh = Arc::clone(&shared);

        handles.push(thread::spawn(move || {
            #[cfg(unix)]
            use std::os::unix::io::{FromRawFd, IntoRawFd};

            let stdin_p = if in_receivers.is_empty() {
                Stdio::null()
            } else {
                Stdio::piped()
            };

            // Use a PTY for stdout so the child uses line buffering
            // (it thinks it's writing to a terminal).  Fall back to a pipe
            // if PTY allocation fails or no output is needed.
            let (pty_master, stdout_p) = if out_senders.is_empty() {
                (None, Stdio::null())
            } else {
                match open_pty() {
                    Some((master, slave)) => {
                        let stdio = unsafe { Stdio::from_raw_fd(slave.into_raw_fd()) };
                        (Some(master), stdio)
                    }
                    None => (None, Stdio::piped()),
                }
            };

            let mut child = match Command::new(&argv[0])
                .args(&argv[1..])
                .stdin(stdin_p)
                .stdout(stdout_p)
                .stderr(Stdio::piped())
                .spawn()
            {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("\x1b[31mError:\x1b[0m failed to spawn `{}`: {e}", argv[0]);
                    sh.fail();
                }
            };

            let child_in = child.stdin.take();
            // For PTY: read from master fd; for pipe: read from child.stdout
            let child_out: Option<Box<dyn std::io::Read + Send>> = if let Some(master) = pty_master
            {
                Some(Box::new(unsafe {
                    std::fs::File::from_raw_fd(master.into_raw_fd())
                }))
            } else {
                child
                    .stdout
                    .take()
                    .map(|s| Box::new(s) as Box<dyn std::io::Read + Send>)
            };
            let child_err = child.stderr.take();

            // Register child for killing on failure.  We wrap only for the
            // kill path; the wait is done in a dedicated thread below.
            let child_arc: ChildHandle = Arc::new(Mutex::new(child));
            sh.children.lock().unwrap().push(Arc::clone(&child_arc));

            // IO threads
            let sh_stdin = Arc::clone(&sh);
            let stdin_thread = child_in.map(|cin| {
                let cin = Arc::new(Mutex::new(cin));
                let mut subs = Vec::new();
                for rx in in_receivers {
                    let cin = Arc::clone(&cin);
                    let sh_sub = Arc::clone(&sh_stdin);
                    subs.push(thread::spawn(move || {
                        while let Some(line) = rx.recv_line() {
                            if sh_sub.is_failed() {
                                return;
                            }
                            let mut w = cin.lock().unwrap();
                            if writeln!(w, "{line}").is_err() {
                                return;
                            }
                        }
                    }));
                }
                thread::spawn(move || {
                    for h in subs {
                        let _ = h.join();
                    }
                    // All sub-threads done: drop cin so the child's stdin
                    // fd is closed (EOF) even if upstream is still open.
                    drop(cin);
                })
            });

            let sh_err = Arc::clone(&sh);
            let stderr_thread = child_err.map(|ce| {
                thread::spawn(move || {
                    let reader = BufReader::new(ce);
                    for line in reader.lines() {
                        if let Ok(l) = line {
                            sh_err.print_stderr(&l);
                            sh_err.fail(); // EXIT on any stderr
                        }
                    }
                })
            });

            let sh_out = Arc::clone(&sh);
            let stdout_thread = child_out.map(|co| {
                thread::spawn(move || {
                    let reader = BufReader::new(co);
                    for line in reader.lines() {
                        if sh_out.is_failed() {
                            break;
                        }
                        match line {
                            Ok(l) => {
                                for s in &out_senders {
                                    if !s.send_line(l.clone()) {
                                        return;
                                    }
                                }
                            }
                            // PTY master returns EIO when the slave side closes (child exited).
                            // Treat it as clean EOF rather than an error.
                            Err(e) if e.raw_os_error() == Some(5) => break, // EIO: PTY slave closed
                            Err(_) => break,
                        }
                    }
                })
            });

            // Blocking wait in a dedicated thread — no polling.
            let sh_wait = Arc::clone(&sh);
            let argv_wait = argv.clone();
            let wait_thread = thread::spawn(move || {
                let status = match child_arc.lock().unwrap().wait() {
                    Ok(s) => s,
                    Err(e) => {
                        eprintln!("\x1b[31mError:\x1b[0m `{}` wait error: {e}", argv_wait[0]);
                        sh_wait.fail();
                    }
                };
                if !status.success() {
                    eprintln!(
                        "\x1b[31mError:\x1b[0m `{}` exited status {}",
                        argv_wait[0],
                        status.code().unwrap_or(-1)
                    );
                    sh_wait.fail();
                }
            });

            // Drain output first, then wait, then tear down stdin.
            if let Some(t) = stdout_thread {
                let _ = t.join();
            }
            if let Some(t) = stderr_thread {
                let _ = t.join();
            }
            let _ = wait_thread.join();
            if let Some(t) = stdin_thread {
                let _ = t.join();
            }
        }));
    }

    // Drop original sender/receiver ends before joining.  Threads hold
    // only their own clones; releasing these lets every mpsc channel
    // reach EOF so all threads can finish.
    drop(senders);
    drop(receivers);

    for h in handles {
        let _ = h.join();
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <graph.dot>", args[0]);
        process::exit(1);
    }
    let src = std::fs::read_to_string(&args[1]).unwrap_or_else(|e| {
        red_error(&format!("cannot read `{}`: {e}", args[1]));
    });
    let edges = parse(&src);
    if !edges.is_empty() {
        run(&edges);
    }
}
