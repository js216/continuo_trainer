// SPDX-License-Identifier: MIT
// loader.rs --- load and normalize continuo lesson data
// Copyright (c) 2026 Jakob Kastelic

use std::fs;
use std::io;

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

fn read_lesson_number() -> Option<usize> {
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    let mut parts = input.split_whitespace();
    if parts.next() != Some("LOAD_LESSON") {
        eprintln!("Expected LOAD_LESSON <number>");
        return None;
    }
    match parts.next().and_then(|x| x.parse().ok()) {
        Some(n) => Some(n),
        None => {
            eprintln!("Invalid lesson number");
            None
        }
    }
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

fn main() {
    let n = match read_lesson_number() {
        Some(n) => n,
        None => return,
    };

    let file = format!("seq/{}.txt", n);
    let content = match fs::read_to_string(&file) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Cannot read {}: {}", file, e);
            return;
        }
    };

    let lesson = parse_lesson(&content);
    if lesson.bass.len() != lesson.figures.len() {
        eprintln!(
            "Length mismatch: bass={} figures={}",
            lesson.bass.len(),
            lesson.figures.len()
        );
        return;
    }

    let melody_groups = group_melody(&lesson.bass, &lesson.melody);
    emit(n, &lesson, &melody_groups);
}
