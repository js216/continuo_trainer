// SPDX-License-Identifier: MIT
// load.rs --- load and normalize continuo lesson data
// Copyright (c) 2026 Jakob Kastelic

// DESCRIPTION
//     load reads lesson requests from standard input in a loop, loads the
//     corresponding lesson file from disk for each request, and writes a
//     normalized, fully expanded representation to standard output.  It
//     keeps running until EOF or a QUIT command; multiple lessons may be
//     loaded in a single session.
//
//     Error messages are printed in red to standard error via ANSI escape
//     codes and the program immediately exits with code 1.
//
// COMMANDS (stdin)
//     LOAD_LESSON <n>
//         Load and emit lesson number <n>.  <n> must be a non-negative
//         decimal integer.  Any unrecognized command (including a malformed
//         LOAD_LESSON line) is fatal.
//
//     QUIT
//         Exit cleanly.
//
//     Blank lines are silently ignored.
//
// FILES
//     For lesson number <n> the program reads seq/<n>.txt.  Failure to
//     open or read the file is fatal.
//
// LESSON FILE FORMAT
//     Plain text.  Metadata fields appear as "key: value" lines; section
//     headers introduce token blocks terminated by a line starting with '}'.
//
//     Metadata:
//         title: <text>     Lesson title (free text).
//         key:   <text>     Key signature.
//         time:  <text>     Time signature.
//
//     Sections (whitespace-separated tokens, one or more lines each):
//         bassline { ... }  Bass note tokens.
//         figures  { ... }  Figured-bass annotation tokens.
//         melody   { ... }  Melody note tokens.
//
//     The bass and figures sections must contain the same number of tokens;
//     a mismatch is fatal.
//
// PASSING NOTES
//     A bass token is marked passing by appending 'p' after its duration
//     and any dots, e.g. "fis2p" or "g4.p".  The 'p' is stripped before
//     output and "passing" is appended to the BASSNOTE line instead.
//
// DURATION HANDLING
//     Durations follow LilyPond numeric notation: 1 whole, 2 half, 4
//     quarter, 8 eighth, 16 sixteenth.  One or more trailing dots are
//     supported (each adds half the preceding value).  A token with no
//     duration digit inherits the most recent duration in its voice;
//     the initial default is a quarter note (4).
//
// MELODY GROUPING
//     Melody tokens are distributed across bass notes by comparing
//     cumulative durations.  All melody tokens whose rhythmic position
//     falls within a bass note's span are assigned to that group.  If a
//     bass note is covered entirely by a melody note begun in an earlier
//     group, its melody group is empty and "-" is emitted.
//
// OUTPUT FORMAT
//     LESSON <n> <key> <time> <title>
//
//     For each bass index i (0-based):
//         BASSNOTE i: <token> [passing]
//         FIGURES  i: <token>
//         MELODY   i: <tokens|-|>
//
// DIAGNOSTICS AND EXIT STATUS
//     On any error (unreadable file, unknown command, bass/figures length
//     mismatch, stdin read failure) a red-highlighted message is written
//     to standard error and the program exits with code 1.  On clean
//     termination (EOF or QUIT) it exits with code 0.

use std::fs;
use std::io::{self, BufRead};
use std::process;

/// Helper macro to print a formatted error message in red to stderr and exit with code 1.
macro_rules! die {
    ($($arg:tt)*) => {{
        eprint!("\x1b[31mError:\x1b[0m ");
        eprintln!($($arg)*);
        process::exit(1);
    }}
}

fn skip_pitch_name(s: &str) -> &str {
    let s = s.trim_start_matches(|c: char| c.is_ascii_alphabetic()); // note letter + is/es
    let s = s.trim_start_matches(|c: char| c == '\'' || c == ','); // octave marks
    s
}

fn parse_dots(s: &str) -> (&str, u32) {
    let dots = s.chars().take_while(|&c| c == '.').count() as u32;
    (&s[dots as usize..], dots)
}

fn base_duration_to_sixteenths(dur: u32) -> Option<u32> {
    match dur {
        1 => Some(16),
        2 => Some(8),
        4 => Some(4),
        8 => Some(2),
        16 => Some(1),
        _ => None,
    }
}

fn apply_dots(base: u32, dots: u32) -> u32 {
    let mut total = base;
    let mut add = base;
    for _ in 0..dots {
        add /= 2;
        total += add;
    }
    total
}

/// Returns the duration of a Lilypond note token in 16th-note units, or None
/// if the token carries no duration digit (caller handles inheritance).
fn parse_duration(token: &str) -> Option<u32> {
    let after_pitch = skip_pitch_name(token);
    let (digits, rest) = after_pitch.split_at(
        after_pitch
            .find(|c: char| !c.is_ascii_digit())
            .unwrap_or(after_pitch.len()),
    );
    if digits.is_empty() {
        return None;
    }
    let base_dur: u32 = digits.parse().ok()?;
    let rest = rest.trim_end_matches('p'); // strip our passing-note marker before dots
    let (_, dots) = parse_dots(rest);
    let base_sixteenths = base_duration_to_sixteenths(base_dur)?;
    Some(apply_dots(base_sixteenths, dots))
}

/// Returns true if this bass token is marked as a passing note (trailing 'p'
/// after the duration or dots, e.g. fis2p or g2.p).
fn is_passing(token: &str) -> bool {
    let b = token.as_bytes();
    b.last() == Some(&b'p')
        && b.len() > 1
        && (b[b.len() - 2].is_ascii_digit() || b[b.len() - 2] == b'.')
}

struct Lesson {
    title: String,
    key: String,
    time: String,
    bass: Vec<String>,
    figures: Vec<String>,
    melody: Vec<String>,
}

fn parse_lesson(content: &str) -> Lesson {
    let mut title = String::new();
    let mut key = String::new();
    let mut time = String::new();
    let mut bass = Vec::new();
    let mut figures = Vec::new();
    let mut melody = Vec::new();
    let mut mode = "";

    for line in content.lines() {
        let l = line.trim();
        if l.is_empty() {
            continue;
        }
        if l.starts_with("title:") {
            title = l[6..].trim().to_string();
            continue;
        }
        if l.starts_with("key:") {
            key = l[4..].trim().to_string();
            continue;
        }
        if l.starts_with("time:") {
            time = l[5..].trim().to_string();
            continue;
        }
        if l.starts_with("bassline") {
            mode = "bass";
            continue;
        }
        if l.starts_with("figures") {
            mode = "fig";
            continue;
        }
        if l.starts_with("melody") {
            mode = "mel";
            continue;
        }
        if l.starts_with('}') {
            mode = "";
            continue;
        }

        match mode {
            "bass" => bass.extend(l.split_whitespace().map(str::to_string)),
            "fig" => figures.extend(l.split_whitespace().map(str::to_string)),
            "mel" => melody.extend(l.split_whitespace().map(str::to_string)),
            _ => {}
        }
    }
    Lesson {
        title,
        key,
        time,
        bass,
        figures,
        melody,
    }
}

/// Groups melody tokens so that melody_groups[i] contains all melody tokens
/// that sound over bass[i]. An empty group means the bass note is covered by
/// a held note from the previous group.
fn group_melody(bass: &[String], melody: &[String]) -> Vec<String> {
    let mut last_dur = 4u32;
    let bass_durs: Vec<u32> = bass
        .iter()
        .map(|tok| match parse_duration(tok) {
            Some(d) => {
                last_dur = d;
                d
            }
            None => last_dur,
        })
        .collect();

    let mut groups: Vec<String> = vec![String::new(); bass.len()];
    let mut mel_idx = 0usize;
    let mut last_mel_dur = 4u32;
    let mut bass_total = 0u32;
    let mut mel_total = 0u32;

    for (bi, &bd) in bass_durs.iter().enumerate() {
        bass_total += bd;
        while mel_total < bass_total && mel_idx < melody.len() {
            let tok = &melody[mel_idx];
            let d = match parse_duration(tok) {
                Some(d) => {
                    last_mel_dur = d;
                    d
                }
                None => last_mel_dur,
            };
            if !groups[bi].is_empty() {
                groups[bi].push(' ');
            }
            groups[bi].push_str(tok);
            mel_total += d;
            mel_idx += 1;
        }
    }
    groups
}

fn emit(n: usize, lesson: &Lesson, melody_groups: &[String]) {
    println!(
        "LESSON {} {} {} {}",
        n, lesson.key, lesson.time, lesson.title
    );
    for i in 0..lesson.bass.len() {
        let (tok, passing) = if is_passing(&lesson.bass[i]) {
            (lesson.bass[i].trim_end_matches('p'), true)
        } else {
            (lesson.bass[i].as_str(), false)
        };
        if passing {
            println!("BASSNOTE {}: {} passing", i, tok);
        } else {
            println!("BASSNOTE {}: {}", i, tok);
        }
        println!("FIGURES {}: {}", i, lesson.figures[i]);
        println!(
            "MELODY {}: {}",
            i,
            if melody_groups[i].is_empty() {
                "-"
            } else {
                &melody_groups[i]
            }
        );
    }
}

fn load_and_emit(n: usize) {
    let file = format!("seq/{}.txt", n);
    let content = match fs::read_to_string(&file) {
        Ok(c) => c,
        Err(e) => {
            die!("Cannot read {}: {}", file, e);
        }
    };

    let lesson = parse_lesson(&content);
    if lesson.bass.len() != lesson.figures.len() {
        die!(
            "Length mismatch: bass={} figures={}",
            lesson.bass.len(),
            lesson.figures.len()
        );
    }

    let melody_groups = group_melody(&lesson.bass, &lesson.melody);
    emit(n, &lesson, &melody_groups);
}

fn main() {
    let stdin = io::stdin();
    let mut line = String::new();

    loop {
        line.clear();
        match stdin.lock().read_line(&mut line) {
            Ok(0) => break, // EOF
            Ok(_) => {}
            Err(e) => die!("stdin read error: {}", e),
        }

        let mut parts = line.split_whitespace();
        match parts.next() {
            None => continue, // blank line
            Some("LOAD_LESSON") => match parts.next().and_then(|x| x.parse::<usize>().ok()) {
                Some(n) => load_and_emit(n),
                None => die!("LOAD_LESSON requires a valid lesson number"),
            },
            Some("QUIT") => break,
            Some(cmd) => die!("Unknown command: {}", cmd),
        }
    }
}
