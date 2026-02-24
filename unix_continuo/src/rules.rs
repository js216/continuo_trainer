// SPDX-License-Identifier: MIT
// rules.rs --- evaluate correctness of chords using sliding-window rules
// Copyright (c) 2026 Jakob Kastelic

use std::io::{self, BufRead};

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
struct Group {
    id: usize,
    passing: bool,
    bass: String,
    figures: String,
    melody_notes: Vec<String>,
    bass_actual: String,
    time: u64,
    realization: Vec<String>,
}

// A rule function receives the window of recent groups (oldest first, current last)
// and returns Ok(()) or Err(explanation).
type RuleFn = fn(&[Group]) -> Result<(), String>;

// ---------------------------------------------------------------------------
// Pitch utilities
// ---------------------------------------------------------------------------

/// Convert a LilyPond pitch string (e.g. "g2", "cis''", "fis") to a MIDI-like
/// semitone integer so we can compute intervals.  Octave ticks (') raise by 12,
/// commas (,) lower by 12.  We ignore duration suffixes (digits at the end).
fn pitch_to_semitone(p: &str) -> Option<i32> {
    // Strip trailing duration digits (e.g. "g'2." -> "g'")
    let p = p.trim_end_matches(|c: char| c.is_ascii_digit() || c == '.');

    // Find the end of the note name + accidental
    let note_chars: &[char] = &['a','b','c','d','e','f','g'];
    if p.is_empty() { return None; }
    let first = p.chars().next()?.to_ascii_lowercase();
    if !note_chars.contains(&first) { return None; }

    // Collect accidental part (es/is/s/flat/sharp variants in LilyPond)
    let rest = &p[1..];
    let (acc_semitones, after_acc) = if rest.starts_with("isis") {
        (2i32, &rest[4..])
    } else if rest.starts_with("eses") {
        (-2i32, &rest[4..])
    } else if rest.starts_with("is") {
        (1i32, &rest[2..])
    } else if rest.starts_with("es") || rest.starts_with("as") {
        // "as" is enharmonic to "aes"
        (-1i32, &rest[2..])
    } else if rest.starts_with('s') && first != 'e' {
        // rare shorthand for -es, avoid matching 'e' + 's'
        (-1i32, &rest[1..])
    } else {
        (0i32, rest)
    };

    // Count octave modifiers
    let octave_delta: i32 = after_acc.chars().map(|c| match c {
        '\'' => 12,
        ','  => -12,
        _    => 0,
    }).sum();

    // Base semitone in octave 0 (c=0)
    let base = match first {
        'c' => 0,
        'd' => 2,
        'e' => 4,
        'f' => 5,
        'g' => 7,
        'a' => 9,
        'b' => 11,
        _   => return None,
    };

    // LilyPond's default octave for undecorated notes puts middle C (c') at MIDI 60.
    // A bare note letter without ' or , sits an octave below middle C (octave 4 in MIDI terms).
    // We add 48 so that "c" = 48, "c'" = 60, etc.
    Some(base + acc_semitones + octave_delta + 48)
}

/// Return the interval in semitones between two pitch strings (unsigned, mod 12 for quality check).
fn interval_semitones(a: &str, b: &str) -> Option<i32> {
    let sa = pitch_to_semitone(a)?;
    let sb = pitch_to_semitone(b)?;
    Some((sb - sa).abs())
}

/// Is the interval a perfect fifth (7 semitones) or perfect octave (12 / 0 mod 12)?
fn is_perfect_fifth(semitones: i32) -> bool {
    semitones % 12 == 7
}

fn is_perfect_unison_or_octave(semitones: i32) -> bool {
    semitones % 12 == 0
}

// ---------------------------------------------------------------------------
// Rule implementations
// ---------------------------------------------------------------------------

/// Rule: skip passing groups for most voice-leading checks.
/// Helper: returns true when the current (last) group is marked passing.
fn current_is_passing(window: &[Group]) -> bool {
    window.last().map(|g| g.passing).unwrap_or(false)
}

/// Parallel fifths / octaves between any two voice pairs across consecutive non-passing groups.
fn rule_no_parallel_fifths(window: &[Group]) -> Result<(), String> {
    if window.len() < 2 { return Ok(()); }
    let prev = &window[window.len() - 2];
    let curr = &window[window.len() - 1];
    if curr.passing || prev.passing { return Ok(()); }

    // Collect voices: bass_actual + realization (lowest to highest by semitone)
    let mut prev_voices: Vec<&str> = std::iter::once(prev.bass_actual.as_str())
        .chain(prev.realization.iter().map(|s| s.as_str()))
        .collect();
    let mut curr_voices: Vec<&str> = std::iter::once(curr.bass_actual.as_str())
        .chain(curr.realization.iter().map(|s| s.as_str()))
        .collect();

    // Pad shorter list with empty strings (voices that didn't move / are absent)
    let max_len = prev_voices.len().max(curr_voices.len());
    prev_voices.resize(max_len, "");
    curr_voices.resize(max_len, "");

    for i in 0..max_len {
        for j in (i + 1)..max_len {
            let pv_i = prev_voices[i];
            let pv_j = prev_voices[j];
            let cv_i = curr_voices[i];
            let cv_j = curr_voices[j];
            if pv_i.is_empty() || pv_j.is_empty() || cv_i.is_empty() || cv_j.is_empty() {
                continue;
            }
            let prev_int = match interval_semitones(pv_i, pv_j) { Some(v) => v, None => continue };
            let curr_int = match interval_semitones(cv_i, cv_j) { Some(v) => v, None => continue };

            // Both are perfect fifth or octave and voices move in the same direction → parallel
            let is_parallel_fifth  = is_perfect_fifth(prev_int) && is_perfect_fifth(curr_int);
            let is_parallel_octave = is_perfect_unison_or_octave(prev_int) && is_perfect_unison_or_octave(curr_int);

            if is_parallel_fifth || is_parallel_octave {
                // Check same direction of motion
                let psi = match pitch_to_semitone(pv_i) { Some(v) => v, None => continue };
                let csi = match pitch_to_semitone(cv_i) { Some(v) => v, None => continue };
                let psj = match pitch_to_semitone(pv_j) { Some(v) => v, None => continue };
                let csj = match pitch_to_semitone(cv_j) { Some(v) => v, None => continue };

                let dir_i = (csi - psi).signum();
                let dir_j = (csj - psj).signum();

                // Parallel motion into perfect consonance (same direction, non-zero)
                if dir_i == dir_j && dir_i != 0 {
                    let kind = if is_parallel_fifth { "fifths" } else { "octaves" };
                    return Err(format!(
                        "Parallel {} between voice {} ({}->{}) and voice {} ({}->{})",
                        kind, i, pv_i, cv_i, j, pv_j, cv_j
                    ));
                }
            }
        }
    }
    Ok(())
}

/// Rule: melody should not leap by more than a major sixth (9 semitones) in a single step.
fn rule_melody_no_large_leap(window: &[Group]) -> Result<(), String> {
    if window.len() < 2 { return Ok(()); }
    let prev = &window[window.len() - 2];
    let curr = &window[window.len() - 1];
    if curr.passing { return Ok(()); }

    // Take last note of previous melody and first note of current melody
    let prev_last = match prev.melody_notes.last() { Some(n) => n, None => return Ok(()) };
    let curr_first = match curr.melody_notes.first() { Some(n) => n, None => return Ok(()) };

    if let Some(iv) = interval_semitones(prev_last, curr_first) {
        if iv > 9 {
            return Err(format!(
                "Melody leap of {} semitones from {} to {} exceeds a major sixth",
                iv, prev_last, curr_first
            ));
        }
    }
    Ok(())
}

/// Rule: the bass should not move by a diminished interval (tritone = 6 semitones).
fn rule_no_tritone_bass_leap(window: &[Group]) -> Result<(), String> {
    if window.len() < 2 { return Ok(()); }
    let prev = &window[window.len() - 2];
    let curr = &window[window.len() - 1];
    if curr.passing { return Ok(()); }

    if let Some(iv) = interval_semitones(&prev.bass_actual, &curr.bass_actual) {
        if iv % 12 == 6 {
            return Err(format!(
                "Tritone leap in bass from {} to {}",
                prev.bass_actual, curr.bass_actual
            ));
        }
    }
    Ok(())
}

/// Rule: realization must not be empty (at least one inner voice required).
fn rule_realization_not_empty(window: &[Group]) -> Result<(), String> {
    if let Some(curr) = window.last() {
        if !curr.passing && curr.realization.is_empty() {
            return Err("Realization is empty; at least one inner voice is required".into());
        }
    }
    Ok(())
}

/// Rule: melody must not be absent.
fn rule_melody_present(window: &[Group]) -> Result<(), String> {
    if let Some(curr) = window.last() {
        if !curr.passing && curr.melody_notes.is_empty() {
            return Err("No melody notes present for this group".into());
        }
    }
    Ok(())
}

// ---------------------------------------------------------------------------
// Rule dispatch table
// ---------------------------------------------------------------------------

const RULES: &[RuleFn] = &[
    rule_realization_not_empty,
    rule_melody_present,
    rule_no_parallel_fifths,
    rule_melody_no_large_leap,
    rule_no_tritone_bass_leap,
];

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

fn parse_group(line: &str) -> Option<Group> {
    // GROUP ID:0 passing BASS:g2 FIGURES:0 MELODY:g'2. BASS_ACTUAL:g TIME:4531 REALIZATION:b'/g'/d'
    if !line.starts_with("GROUP ") { return None; }

    let mut id = 0usize;
    let mut passing = false;
    let mut bass = String::new();
    let mut figures = String::new();
    let mut melody_notes: Vec<String> = Vec::new();
    let mut bass_actual = String::new();
    let mut time = 0u64;
    let mut realization: Vec<String> = Vec::new();

    for token in line.split_whitespace() {
        if token == "passing" {
            passing = true;
        } else if let Some(v) = token.strip_prefix("ID:") {
            id = v.parse().unwrap_or(0);
        } else if let Some(v) = token.strip_prefix("BASS:") {
            bass = v.to_string();
        } else if let Some(v) = token.strip_prefix("FIGURES:") {
            figures = v.to_string();
        } else if let Some(v) = token.strip_prefix("MELODY:") {
            // Melody can be multiple space-separated tokens; here we grab what's in this token.
            // Multiple melody notes appear as separate whitespace-separated tokens all before
            // the next KEY: token.  We handle them as a single token for simplicity since
            // the format uses space separation and our split already handles that.
            melody_notes.extend(v.split_whitespace().map(|s| s.to_string()));
        } else if let Some(v) = token.strip_prefix("BASS_ACTUAL:") {
            bass_actual = v.to_string();
        } else if let Some(v) = token.strip_prefix("TIME:") {
            time = v.parse().unwrap_or(0);
        } else if let Some(v) = token.strip_prefix("REALIZATION:") {
            realization = v.split('/').map(|s| s.to_string()).collect();
        }
    }

    // Second pass: collect all melody tokens (they may span multiple whitespace-separated items
    // between MELODY: and the next KEY: marker).
    // Re-parse more carefully for MELODY which can have multiple space-separated notes.
    melody_notes.clear();
    let mut in_melody = false;
    for token in line.split_whitespace() {
        if token.starts_with("MELODY:") {
            in_melody = true;
            let after = &token["MELODY:".len()..];
            if !after.is_empty() { melody_notes.push(after.to_string()); }
        } else if in_melody {
            // Stop when we hit another KEY: token
            if token.contains(':') {
                in_melody = false;
            } else {
                melody_notes.push(token.to_string());
            }
        }
    }

    Some(Group { id, passing, bass, figures, melody_notes, bass_actual, time, realization })
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() {
    let stdin = io::stdin();
    let window_size = 4usize; // sliding window depth
    let mut window: Vec<Group> = Vec::with_capacity(window_size);

    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(e) => { eprintln!("Error reading input: {}", e); break; }
        };
        let line = line.trim();

        // Pass through comment / lesson header lines unchanged
        if line.is_empty() || line.starts_with("LESSON ") || line.starts_with("//") {
            println!("{}", line);
            continue;
        }

        let group = match parse_group(line) {
            Some(g) => g,
            None => {
                // Unknown line – echo it
                println!("{}", line);
                continue;
            }
        };

        let id = group.id;
        window.push(group);
        if window.len() > window_size {
            window.remove(0);
        }

        // Apply all rules
        let mut result: Result<(), String> = Ok(());
        for rule in RULES {
            if let Err(msg) = rule(&window) {
                result = Err(msg);
                break; // report first failure
            }
        }

        match result {
            Ok(())   => println!("RESULT {} OK", id),
            Err(msg) => println!("RESULT {} FAIL {}", id, msg),
        }
    }
}
