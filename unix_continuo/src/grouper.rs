// SPDX-License-Identifier: MIT
// grouper.rs --- align MIDI notes to lesson bassline, figures, and melody
// Copyright (c) 2026 Jakob Kastelic

use std::collections::{BTreeMap, HashMap};
use std::io::{self, BufRead};

// ---------------------------------------------------------------------------
// Note spelling
// ---------------------------------------------------------------------------

/// Accidentals used per pitch class for a given key signature.
/// Keys with sharps use sharp spellings; keys with flats use flat spellings.
/// We store 7 sharp keys and 7 flat keys (including C which is neutral).
fn spelling_for_key(key: &str) -> [&'static str; 12] {
    // Pitch classes 0..11 = C C#/Db D D#/Eb E F F#/Gb G G#/Ab A A#/Bb B
    let sharp_keys = ["C", "G", "D", "A", "E", "B", "F#", "C#"];
    let flat_keys = ["C", "F", "Bb", "Eb", "Ab", "Db", "Gb", "Cb"];

    let use_sharps = sharp_keys.contains(&key);
    // For ambiguous C we default to sharps
    if use_sharps || !flat_keys.contains(&key) {
        [
            "c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "ais", "b",
        ]
    } else {
        [
            "c", "des", "d", "ees", "e", "f", "ges", "g", "aes", "a", "bes", "b",
        ]
    }
}

/// Parse a LilyPond-style note name (e.g. "g,", "fis", "cis''", "d'") into a MIDI note number.
/// Octave modifiers: no modifier = octave 4 (middle C = c' = 60)
/// '  raises one octave,  ,  lowers one octave. Multiple allowed.
fn lilypond_to_midi(note: &str) -> Option<u8> {
    // Strip duration suffix (digits + dots at end) and velocity/time tags
    let note = note.split_whitespace().next().unwrap_or(note);
    // Find where pitch name ends and octave modifiers begin
    let name_end = note
        .find(|c: char| c == '\'' || c == ',')
        .unwrap_or(note.len());

    // Strip trailing duration digits/dots from the pitch portion
    let raw_name = &note[..name_end];
    let raw_name = raw_name.trim_end_matches(|c: char| c.is_ascii_digit() || c == '.');

    let octave_str = &note[name_end..];

    let pc: i32 = match raw_name {
        "c" | "bis" => 0,
        "cis" | "des" => 1,
        "d" => 2,
        "dis" | "ees" => 3,
        "e" | "fes" => 4,
        "f" | "eis" => 5,
        "fis" | "ges" => 6,
        "g" => 7,
        "gis" | "aes" => 8,
        "a" => 9,
        "ais" | "bes" => 10,
        "b" | "ces" => 11,
        _ => return None,
    };

    // Base octave in LilyPond unmodified = octave 3 (C3 = MIDI 48; c' = middle C = MIDI 60)
    let base_octave: i32 = 3;
    let octave_offset: i32 = octave_str
        .chars()
        .map(|c| match c {
            '\'' => 1,
            ',' => -1,
            _ => 0,
        })
        .sum();

    let midi = (base_octave + octave_offset + 1) * 12 + pc;
    if midi >= 0 && midi <= 127 {
        Some(midi as u8)
    } else {
        None
    }
}

/// Format a MIDI note number as a LilyPond note name, using the provided spelling table.
/// Octave: MIDI 60 = c' (middle C).
fn midi_to_lilypond(midi: u8, spelling: &[&'static str; 12]) -> String {
    let pc = (midi % 12) as usize;
    let octave = (midi as i32) / 12 - 1; // MIDI octave: 60/12 - 1 = 4
    let name = spelling[pc];

    // LilyPond octave: c (no modifier) = octave 3, c' = 4, c'' = 5, c, = 2
    let lily_octave = octave - 3;
    let modifier = if lily_octave >= 0 {
        "'".repeat(lily_octave as usize)
    } else {
        ",".repeat((-lily_octave) as usize)
    };

    format!("{}{}", name, modifier)
}

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
struct LessonEntry {
    bassnote: String, // e.g. "g2"
    figures: String,  // e.g. "0", "#6"
    melody: String,   // e.g. "g'2."
    passing: bool,
}

#[derive(Debug, Default)]
struct Grouper {
    key: String,
    entries: HashMap<usize, LessonEntry>,

    /// Notes currently held: midi -> count (to handle repeated note-ons)
    held: BTreeMap<u8, u32>,

    /// The group currently being collected
    current_group_id: usize,
    /// Bass note of the current group (lowest NOTE_ON received since last group boundary)
    current_bass: Option<u8>,
    /// MIDI timestamp when current_bass was first played
    current_bass_time: u64,
    /// All notes pressed in the current group (sorted highest→lowest for chord output)
    current_chord: Vec<u8>,

    /// Pending output for the previous group when we hit a passing note mid-flight
    pending_output: Option<String>,
}

impl Grouper {
    fn reset(&mut self, key: String) {
        self.key = key;
        self.entries = HashMap::new();
        self.held = BTreeMap::new();
        self.current_group_id = 0;
        self.current_bass = None;
        self.current_bass_time = 0;
        self.current_chord = Vec::new();
        self.pending_output = None;
    }

    fn set_bassnote(&mut self, id: usize, note: &str, passing: bool) {
        let entry = self.entries.entry(id).or_insert_with(|| LessonEntry {
            bassnote: String::new(),
            figures: String::new(),
            melody: String::new(),
            passing,
        });
        entry.bassnote = note.to_string();
        entry.passing = passing;
    }

    fn set_figures(&mut self, id: usize, figures: &str) {
        self.entries
            .entry(id)
            .or_insert_with(|| LessonEntry {
                bassnote: String::new(),
                figures: String::new(),
                melody: String::new(),
                passing: false,
            })
            .figures = figures.to_string();
    }

    fn set_melody(&mut self, id: usize, melody: &str) {
        self.entries
            .entry(id)
            .or_insert_with(|| LessonEntry {
                bassnote: String::new(),
                figures: String::new(),
                melody: String::new(),
                passing: false,
            })
            .melody = melody.to_string();
    }

    fn spelling(&self) -> [&'static str; 12] {
        spelling_for_key(&self.key)
    }

    /// Called when all held notes have been released; format and return a GROUP line.
    fn emit_group(&self, group_id: usize, bass_midi: u8, bass_time: u64, chord: &[u8]) -> String {
        let spelling = self.spelling();
        let entry = self.entries.get(&group_id);
        let expected_bass = entry.map(|e| e.bassnote.as_str()).unwrap_or("?");
        let figures = entry.map(|e| e.figures.as_str()).unwrap_or("?");
        let melody = entry.map(|e| e.melody.as_str()).unwrap_or("?");
        let is_passing = entry.map(|e| e.passing).unwrap_or(false);

        let actual_bass = midi_to_lilypond(bass_midi, &spelling);

        // Chord: upper voices only (bass excluded), highest to lowest, joined by '/'
        let mut sorted: Vec<u8> = chord.iter().copied().filter(|&m| m != bass_midi).collect();
        sorted.sort_unstable_by(|a, b| b.cmp(a));
        let realization: Vec<String> = sorted
            .iter()
            .map(|&m| midi_to_lilypond(m, &spelling))
            .collect();

        if is_passing {
            format!(
                "GROUP ID:{} passing BASS:{} FIGURES:{} MELODY:{} BASS_ACTUAL:{} TIME:{} REALIZATION:{}",
                group_id, expected_bass, figures, melody,
                actual_bass, bass_time, realization.join("/")
            )
        } else {
            format!(
                "GROUP ID:{} BASS:{} FIGURES:{} MELODY:{} BASS_ACTUAL:{} TIME:{} REALIZATION:{}",
                group_id,
                expected_bass,
                figures,
                melody,
                actual_bass,
                bass_time,
                realization.join("/")
            )
        }
    }

    fn note_on(&mut self, midi: u8, time: u64) -> Option<String> {
        let mut output = None;

        // Check if this note corresponds to the *next* expected bass note, and that
        // next note is marked as passing — if so, emit the current group immediately.
        let next_id = self.current_group_id + 1;
        if let Some(next_entry) = self.entries.get(&next_id) {
            if next_entry.passing {
                if let Some(expected_midi) = lilypond_to_midi(&next_entry.bassnote) {
                    if midi == expected_midi {
                        // Emit current group now (if we have a bass note)
                        if let Some(bass) = self.current_bass {
                            let line = self.emit_group(
                                self.current_group_id,
                                bass,
                                self.current_bass_time,
                                &self.current_chord.clone(),
                            );
                            output = Some(line);
                        }
                        // Advance to the passing group.
                        // Inherit current_chord (same harmony still active); only reset bass.
                        self.current_group_id = next_id;
                        self.current_bass = None;
                        self.current_bass_time = 0;
                        // Do NOT clear current_chord — passing groups reuse the previous harmony.
                    }
                }
            }
        }

        // Track held notes
        *self.held.entry(midi).or_insert(0) += 1;

        // Update current bass (lowest note seen so far in this group)
        if self.current_bass.map(|b| midi < b).unwrap_or(true) {
            self.current_bass = Some(midi);
            self.current_bass_time = time;
        }

        // Add to chord if not already present
        if !self.current_chord.contains(&midi) {
            self.current_chord.push(midi);
        }

        output
    }

    fn note_off(&mut self, midi: u8) -> Option<String> {
        // Decrement hold count
        if let Some(count) = self.held.get_mut(&midi) {
            if *count > 1 {
                *count -= 1;
                return None;
            }
            self.held.remove(&midi);
        }

        // If there are still held notes, nothing to emit yet
        if !self.held.is_empty() {
            return None;
        }

        // All notes released — emit the current group
        if let Some(bass) = self.current_bass {
            let line = self.emit_group(
                self.current_group_id,
                bass,
                self.current_bass_time,
                &self.current_chord.clone(),
            );
            self.current_group_id += 1;
            self.current_bass = None;
            self.current_bass_time = 0;
            self.current_chord.clear();
            Some(line)
        } else {
            None
        }
    }
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------

/// Parse a NOTE_ON line: "NOTE_ON g, VELOCITY:79 TIME:6059" → (MIDI note number, time)
fn parse_note_on(line: &str) -> Option<(u8, u64)> {
    // Format: NOTE_ON <note> VELOCITY:<v> TIME:<t>
    let rest = line.strip_prefix("NOTE_ON ")?.trim();
    let mut tokens = rest.split_whitespace();
    let note_token = tokens.next()?;
    let midi = lilypond_to_midi(note_token)?;
    let time = tokens
        .find(|t| t.starts_with("TIME:"))?
        .strip_prefix("TIME:")?
        .parse::<u64>()
        .ok()?;
    Some((midi, time))
}

/// Parse a NOTE_OFF line: "NOTE_OFF g, TIME:22858" → MIDI note number
fn parse_note_off(line: &str) -> Option<u8> {
    let rest = line.strip_prefix("NOTE_OFF ")?.trim();
    let note_token = rest.split_whitespace().next()?;
    lilypond_to_midi(note_token)
}

/// Parse "BASSNOTE <id>: <note> [passing]" → (id, note_str, is_passing)
fn parse_bassnote(line: &str) -> Option<(usize, String, bool)> {
    let rest = line.strip_prefix("BASSNOTE ")?.trim();
    let (id_str, remainder) = rest.split_once(':')?;
    let id: usize = id_str.trim().parse().ok()?;
    let mut parts = remainder.split_whitespace();
    let note = parts.next()?.to_string();
    let passing = parts.any(|p| p == "passing");
    Some((id, note, passing))
}

/// Parse "FIGURES <id>: <figures>" → (id, figures_str)
fn parse_figures(line: &str) -> Option<(usize, String)> {
    let rest = line.strip_prefix("FIGURES ")?.trim();
    let (id_str, remainder) = rest.split_once(':')?;
    let id: usize = id_str.trim().parse().ok()?;
    Some((id, remainder.trim().to_string()))
}

/// Parse "MELODY <id>: <melody>" → (id, melody_str)
fn parse_melody(line: &str) -> Option<(usize, String)> {
    let rest = line.strip_prefix("MELODY ")?.trim();
    let (id_str, remainder) = rest.split_once(':')?;
    let id: usize = id_str.trim().parse().ok()?;
    Some((id, remainder.trim().to_string()))
}

/// Parse "LESSON <num> <key> <time_sig> <title...>" → key string
fn parse_lesson_key(line: &str) -> Option<String> {
    let rest = line.strip_prefix("LESSON ")?.trim();
    let mut parts = rest.split_whitespace();
    let _num = parts.next()?;
    let key = parts.next()?.to_string();
    Some(key)
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() {
    let stdin = io::stdin();
    let mut grouper = Grouper::default();

    for raw_line in stdin.lock().lines() {
        let line = match raw_line {
            Ok(l) => l,
            Err(e) => {
                eprintln!("Read error: {}", e);
                break;
            }
        };
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        if line.starts_with("LESSON ") {
            println!("{}", line);
            let key = parse_lesson_key(line).unwrap_or_else(|| "C".to_string());
            grouper.reset(key);
        } else if line.starts_with("BASSNOTE ") {
            if let Some((id, note, passing)) = parse_bassnote(line) {
                grouper.set_bassnote(id, &note, passing);
            } else {
                eprintln!("Warning: could not parse BASSNOTE line: {}", line);
            }
        } else if line.starts_with("FIGURES ") {
            if let Some((id, figures)) = parse_figures(line) {
                grouper.set_figures(id, &figures);
            } else {
                eprintln!("Warning: could not parse FIGURES line: {}", line);
            }
        } else if line.starts_with("MELODY ") {
            if let Some((id, melody)) = parse_melody(line) {
                grouper.set_melody(id, &melody);
            } else {
                eprintln!("Warning: could not parse MELODY line: {}", line);
            }
        } else if line.starts_with("NOTE_ON ") {
            if let Some((midi, time)) = parse_note_on(line) {
                if let Some(output) = grouper.note_on(midi, time) {
                    println!("{}", output);
                }
            } else {
                eprintln!("Warning: could not parse NOTE_ON line: {}", line);
            }
        } else if line.starts_with("NOTE_OFF ") {
            if let Some(midi) = parse_note_off(line) {
                if let Some(output) = grouper.note_off(midi) {
                    println!("{}", output);
                }
            } else {
                eprintln!("Warning: could not parse NOTE_OFF line: {}", line);
            }
        } else {
            // ignore all other commands
        }
    }
}
