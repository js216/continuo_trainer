// SPDX-License-Identifier: MIT
// rules.rs --- elegant voice-leading validation for figured bass
// Copyright (c) 2026 Jakob Kastelic

use std::io::{self, BufRead};

#[derive(Debug, Clone)]
struct Group {
    id: usize,
    passing: bool,
    bass: i32,
    inner: Vec<i32>,
    melody: Vec<i32>,
}

type RuleFn = fn(&[Group]) -> Result<(), String>;

// --- Pitch Logic ---

fn to_semitone(p: &str) -> Option<i32> {
    let p = p.trim_end_matches(|c: char| c.is_ascii_digit() || c == '.');
    if p.is_empty() {
        return None;
    }

    let first = p.chars().next()?.to_ascii_lowercase();
    let rest = &p[1..];

    let (acc, oct_part) = if rest.starts_with("isis") {
        (2, &rest[4..])
    } else if rest.starts_with("eses") {
        (-2, &rest[4..])
    } else if rest.starts_with("is") {
        (1, &rest[2..])
    } else if rest.starts_with("es") || rest.starts_with("as") {
        (-1, &rest[2..])
    } else {
        (0, rest)
    };

    let octaves: i32 = oct_part
        .chars()
        .map(|c| match c {
            '\'' => 12,
            ',' => -12,
            _ => 0,
        })
        .sum();

    let base = match first {
        'c' => 0,
        'd' => 2,
        'e' => 4,
        'f' => 5,
        'g' => 7,
        'a' => 9,
        'b' => 11,
        _ => return None,
    };

    Some(base + acc + octaves + 48) // c' = 60
}

// --- Rule Implementations ---

/// Rule: No parallel 5ths or 8ves between any two voices.
fn rule_no_parallels(window: &[Group]) -> Result<(), String> {
    if window.len() < 2 {
        return Ok(());
    }
    let (prev, curr) = (&window[window.len() - 2], &window[window.len() - 1]);
    if curr.passing || prev.passing {
        return Ok(());
    }

    let flatten = |g: &Group| {
        let mut v = vec![g.bass];
        v.extend(&g.inner);
        v.extend(&g.melody);
        v
    };

    let p_voices = flatten(prev);
    let c_voices = flatten(curr);

    for i in 0..p_voices.len() {
        for j in (i + 1)..p_voices.len() {
            if j >= c_voices.len() {
                continue;
            }

            let p_int = (p_voices[j] - p_voices[i]).abs() % 12;
            let c_int = (c_voices[j] - c_voices[i]).abs() % 12;

            if (p_int == 7 && c_int == 7) || (p_int == 0 && c_int == 0) {
                let motion_i = c_voices[i] - p_voices[i];
                let motion_j = c_voices[j] - p_voices[j];

                // Parallel if moving in the same non-zero direction.
                if motion_i.signum() == motion_j.signum() && motion_i != 0 {
                    return Err(format!(
                        "Parallel fifths/octaves between voice {} and {}",
                        i, j
                    ));
                }
            }
        }
    }
    Ok(())
}

/// Rule: Bass should not leap by a tritone (6 semitones).
fn rule_bass_leap(window: &[Group]) -> Result<(), String> {
    if window.len() < 2 {
        return Ok(());
    }
    let (p, c) = (&window[window.len() - 2], &window[window.len() - 1]);
    if (c.bass - p.bass).abs() % 12 == 6 {
        return Err("Tritone leap in bass".into());
    }
    Ok(())
}

const RULES: &[RuleFn] = &[rule_no_parallels, rule_bass_leap];

// --- Main Pipeline ---

fn parse_line(line: &str) -> Option<Group> {
    if !line.starts_with("GROUP") {
        return None;
    }

    let mut id = 0;
    let passing = line.contains("passing"); // mut removed to fix warning
    let (mut bass, mut inner, mut melody) = (0, Vec::new(), Vec::new());

    let tokens: Vec<&str> = line.split_whitespace().collect();
    let mut i = 0;
    while i < tokens.len() {
        let t = tokens[i];
        if let Some(v) = t.strip_prefix("ID:") {
            id = v.parse().unwrap_or(0);
        } else if let Some(v) = t.strip_prefix("BASS_ACTUAL:") {
            bass = to_semitone(v).unwrap_or(0);
        } else if let Some(v) = t.strip_prefix("REALIZATION:") {
            inner = v.split('/').filter_map(to_semitone).collect();
            inner.sort();
        } else if let Some(v) = t.strip_prefix("MELODY:") {
            if !v.is_empty() {
                melody.push(to_semitone(v).unwrap_or(0));
            }
            // Slurp up subsequent melody notes until we hit a KEY: token
            while i + 1 < tokens.len() && !tokens[i + 1].contains(':') {
                i += 1;
                melody.push(to_semitone(tokens[i]).unwrap_or(0));
            }
        }
        i += 1;
    }

    Some(Group {
        id,
        passing,
        bass,
        inner,
        melody,
    })
}

fn main() {
    let mut window: Vec<Group> = Vec::with_capacity(4);
    let stdin = io::stdin();

    for line in stdin.lock().lines().map_while(Result::ok) {
        if let Some(g) = parse_line(&line) {
            let id = g.id;
            window.push(g);
            if window.len() > 4 {
                window.remove(0);
            }

            match RULES.iter().try_for_each(|rule| rule(&window)) {
                Ok(_) => println!("RESULT {} OK", id),
                Err(e) => println!("RESULT {} FAIL {}", id, e),
            }
        } else {
            println!("{}", line); // Echo comments/headers
        }
    }
}
