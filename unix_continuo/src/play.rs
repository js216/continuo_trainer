// play.rs
// Written by Jakob Kastelic (2026).

use std::env;
use std::io::{BufRead, BufReader, Write};
use std::process::{Command, Stdio};

const GREEN: &str = "\x1b[32m";
const RED: &str   = "\x1b[31m";
const RESET: &str = "\x1b[0m";

fn usage() -> !
{
    eprintln!("usage: play <lesson-number>");
    std::process::exit(1);
}

fn forward<R: BufRead>(
    mut reader: R,
    dst: &mut std::process::ChildStdin,
)
{
    let mut line = String::new();

    while reader.read_line(&mut line).unwrap() > 0
    {
        dst.write_all(line.as_bytes()).unwrap();
        dst.flush().unwrap();
        line.clear();
    }
}

fn run_lesson(lesson: &str)
{
    /* ---------- grouper | rules ---------- */

    let mut grouper = Command::new("bin/grouper")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();

    let mut rules = Command::new("bin/rules")
        .stdin(grouper.stdout.take().unwrap())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();

    let mut grouper_stdin =
        grouper.stdin.take().unwrap();

    /* ---------- LOADER ---------- */

    let mut loader = Command::new("bin/loader")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::inherit())   // show loader errors
        .spawn()
        .unwrap();

    {
        let mut loader_stdin =
            loader.stdin.take().unwrap();

        let cmd = format!("LOAD_LESSON {}\n", lesson);
        loader_stdin.write_all(cmd.as_bytes()).unwrap();
        drop(loader_stdin);
    }

    {
        let stdout = loader.stdout.take().unwrap();
        let reader = BufReader::new(stdout);
        forward(reader, &mut grouper_stdin);
    }

    let status = loader.wait().unwrap();

    if !status.success()
    {
        // Stop pipeline immediately
        drop(grouper_stdin);
        let _ = grouper.kill();
        let _ = rules.kill();
        std::process::exit(1);
    }

    /* ---------- MIDI ---------- */

    let mut midi = Command::new("bin/midi")
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();

    {
        let stdout = midi.stdout.take().unwrap();
        let reader = BufReader::new(stdout);
        forward(reader, &mut grouper_stdin);
    }

    midi.wait().unwrap();

    drop(grouper_stdin);

    /* ---------- CAPTURE RULES OUTPUT ---------- */

    let rules_stdout =
        rules.stdout.take().unwrap();

    let reader = BufReader::new(rules_stdout);

    let mut any_fail = false;

    for line in reader.lines()
    {
        let line = line.unwrap();
        println!("{}", line);

        if line.starts_with("RESULT")
            && line.contains("FAIL")
        {
            any_fail = true;
        }
    }

    grouper.wait().unwrap();
    rules.wait().unwrap();

    /* ---------- LESSON SUMMARY ---------- */

    if any_fail
    {
        println!("{RED}LESSON FAIL{RESET}");
    }
    else
    {
        println!("{GREEN}LESSON ALL OK{RESET}");
    }
}

fn main()
{
    let args: Vec<String> = env::args().collect();

    if args.len() != 2
    {
        usage();
    }

    let lesson = &args[1];

    loop
    {
        run_lesson(lesson);
    }
}
