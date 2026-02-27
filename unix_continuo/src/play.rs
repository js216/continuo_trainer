// SPDX-License-Identifier: MIT
// play.rs --- run programs and pipe data around
// Copyright (c) 2026 Jakob Kastelic

use std::env;
use std::io::{BufRead, BufReader, Write};
use std::process::{Command, Stdio};
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    // 1. Demand lesson number from command line
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <lesson_number>", args[0]);
        std::process::exit(1);
    }
    let lesson_num = args[1].clone();

    loop {
        println!("\n\x1b[37m--- Starting Lesson {} ---\x1b[0m", lesson_num);

        /* ---------- Process Setup ---------- */

        let mut group = Command::new("bin/group")
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
            .expect("Failed to start group");

        let mut rules = Command::new("bin/rules")
            .stdin(group.stdout.take().unwrap())
            .stdout(Stdio::piped())
            .spawn()
            .expect("Failed to start rules");

        let mut group_stdin = group.stdin.take().unwrap();
        let rules_stdout = rules.stdout.take().unwrap();

        /* ---------- LOAD ---------- */

        let mut load = Command::new("bin/load")
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
            .expect("Failed to start load");

        {
            let mut load_stdin = load.stdin.take().unwrap();
            let _ = writeln!(load_stdin, "LOAD_LESSON {}", lesson_num);
        } // EOF to load

        // Parse load output to find the end of the lesson (max_id)
        let mut max_id = -1;
        let mut load_reader = BufReader::new(load.stdout.take().unwrap());
        let mut line = String::new();
        while load_reader.read_line(&mut line).unwrap() > 0 {
            let _ = std::io::stdout().flush();

            if line.starts_with("BASSNOTE") {
                if let Some(id_part) = line.split_whitespace().nth(1) {
                    if let Ok(id) = id_part.trim_end_matches(':').parse::<i32>() {
                        if id > max_id { max_id = id; }
                    }
                }
            }
            let _ = group_stdin.write_all(line.as_bytes());
            let _ = group_stdin.flush();
            line.clear();
        }

        // 4. If load returns nonzero, quit immediately
        let load_status = load.wait().unwrap();
        if !load_status.success() {
            eprintln!("Load failed. Quitting.");
            std::process::exit(load_status.code().unwrap_or(1));
        }

        /* ---------- MIDI & MONITORING ---------- */

        let midi = Command::new("bin/midi")
            .stdout(Stdio::piped())
            .spawn()
            .expect("Failed to start midi");

        // Wrap midi in a Mutex so the rules thread can kill it when the lesson ends
        let midi_arc = Arc::new(Mutex::new(Some(midi)));
        let midi_arc_thread = Arc::clone(&midi_arc);
        let has_failed = Arc::new(Mutex::new(false));
        let has_failed_thread = Arc::clone(&has_failed);

        let rules_thread = thread::spawn(move || {
            let reader = BufReader::new(rules_stdout);
            let mut failed = false;
            let end_marker = format!("RESULT {}", max_id);

            for line_res in reader.lines() {
                if let Ok(l) = line_res {
                    // rules forwards group, so this catches all relevant output
                    println!("{}", l);

                    if l.contains("FAIL") {
                        failed = true;
                    }

                    // Check if we just processed the last note of the lesson
                    if l.contains(&end_marker) {
                        let mut lock = midi_arc_thread.lock().unwrap();
                        if let Some(mut m) = lock.take() {
                            let _ = m.kill(); // Stop midi input immediately
                        }
                    }
                }
            }
            if failed {
                *has_failed_thread.lock().unwrap() = true;
            }
        });

        // Forward midi output to group (blocks until midi is killed or finishes)
        let midi_stdout = {
            let mut lock = midi_arc.lock().unwrap();
            lock.as_mut().and_then(|m| m.stdout.take())
        };

        if let Some(stdout) = midi_stdout {
            let mut midi_reader = BufReader::new(stdout);
            let mut m_line = String::new();
            while midi_reader.read_line(&mut m_line).unwrap() > 0 {
                let _ = std::io::stdout().flush();
                let _ = group_stdin.write_all(m_line.as_bytes());
                let _ = group_stdin.flush();
                m_line.clear();
            }
        }

        /* ---------- CLEANUP & RESTART ---------- */

        drop(group_stdin); // Allow group and rules to finish
        let _ = rules_thread.join();
        let _ = rules.wait();
        let _ = group.wait();

        // 2. Print summary
        if *has_failed.lock().unwrap() {
            println!("\x1b[31m=== LESSON FAIL ===\x1b[0m");
        } else {
            println!("\x1b[32m=== LESSON ALL OK ===\x1b[0m");
        }
    }
}
