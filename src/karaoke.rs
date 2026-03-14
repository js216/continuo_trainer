use std::io::{self, BufRead, Write};
use std::sync::{Arc, Mutex, atomic::{AtomicBool, Ordering}};
use std::thread;
use std::time::Duration;

#[derive(Clone)]
struct Note {
    lily: Option<String>,
    midi: Option<u8>,
    beats: f64,
}

struct SharedState {
    bpm: f64,
    beats_per_bar: u32,
    melody: Vec<Note>,
}

// ── Helpers ──────────────────────────────────────────────────────────────────

fn parse_token(tok: &str) -> Option<Note> {
    let tok = tok.replace('~', "");
    if tok.is_empty() { return None; }

    let is_rest = tok.starts_with('r');
    let mut lily = None;
    let mut midi_note = None;
    let mut cursor;

    if !is_rest {
        let mut name_val = None;
        let mut name_len = 0;
        for &(name, val) in &[
            ("cis", 1), ("dis", 3), ("fis", 6), ("gis", 8), ("ais", 10),
            ("c", 0), ("d", 2), ("e", 4), ("f", 5), ("g", 7), ("a", 9), ("b", 11)
        ] {
            if tok.starts_with(name) {
                name_val = Some(val);
                name_len = name.len();
                break;
            }
        }
        let val = name_val?;
        cursor = name_len;

        let mut octave = 3i32;
        let chars: Vec<char> = tok.chars().collect();
        while cursor < chars.len() {
            match chars[cursor] {
                '\'' => { octave += 1; cursor += 1; }
                ',' => { octave -= 1; cursor += 1; }
                _ => break,
            }
        }

        lily = Some(tok[..cursor].to_string());
        let m = (octave + 1) * 12 + val;
        if !(0..=127).contains(&m) { return None; }
        midi_note = Some(m as u8);
    } else {
        cursor = 1;
    }

    let remainder = &tok[cursor..];
    let dur_end = remainder.find(|c: char| !c.is_ascii_digit()).unwrap_or(remainder.len());
    let dur_num: f64 = remainder[..dur_end].parse().ok().unwrap_or(4.0);
    cursor += dur_end;

    let mut dots = 0;
    let chars: Vec<char> = tok.chars().collect();
    while cursor < chars.len() && chars[cursor] == '.' {
        dots += 1;
        cursor += 1;
    }

    let dot_factor = (f64::powi(2.0, dots + 1) - 1.0) / f64::powi(2.0, dots);
    let beats = (4.0 / dur_num) * dot_factor;

    Some(Note { lily, midi: midi_note, beats })
}

// ── Main ─────────────────────────────────────────────────────────────────────

fn main() {
    let state = Arc::new(Mutex::new(SharedState {
        bpm: 120.0,
        beats_per_bar: 4,
        melody: Vec::new(),
    }));

    let stop_signal = Arc::new(AtomicBool::new(false));
    let mut last_handle: Option<thread::JoinHandle<()>> = None;

    let stdin = io::stdin();
    for line_result in stdin.lock().lines() {
        let line = match line_result {
            Ok(l) => l,
            Err(_) => break,
        };

        if line == "KARAOKE_ON" {
            stop_signal.store(true, Ordering::SeqCst);
            thread::sleep(Duration::from_millis(10));

            stop_signal.store(false, Ordering::SeqCst);
            let state_ref = Arc::clone(&state);
            let stop_ref = Arc::clone(&stop_signal);

            last_handle = Some(thread::spawn(move || {
                let (bpm, beats_per_bar, melody) = {
                    let s = state_ref.lock().unwrap();
                    (s.bpm, s.beats_per_bar, s.melody.clone())
                };

                // Count-in: one short click per beat in the bar
                let beat_secs = 60.0 / bpm;
                let click_on  = Duration::from_millis(30);
                let click_off = Duration::from_secs_f64((beat_secs - 0.030_f64).max(0.0));
                for _ in 0..beats_per_bar {
                    if stop_ref.load(Ordering::SeqCst) { return; }
                    println!("MIDI NOTE_ON b'' VELOCITY:64");
                    let _ = io::stdout().flush();
                    thread::sleep(click_on);
                    println!("MIDI NOTE_OFF b''");
                    let _ = io::stdout().flush();
                    thread::sleep(click_off);
                }

                let mut stopped = false;
                for note in melody {
                    if stop_ref.load(Ordering::SeqCst) { stopped = true; break; }

                    let total_secs = note.beats * 60.0 / bpm;
                    if let (Some(lily), Some(_midi)) = (&note.lily, note.midi) {
                        let play_dur = Duration::from_secs_f64(total_secs * 0.9);
                        let gap_dur = Duration::from_secs_f64(total_secs * 0.1);

                        println!("MIDI NOTE_ON {} VELOCITY:80", lily);
                        let _ = io::stdout().flush();

                        thread::sleep(play_dur);
                        if stop_ref.load(Ordering::SeqCst) { stopped = true; break; }

                        println!("MIDI NOTE_OFF {}", lily);
                        let _ = io::stdout().flush();

                        thread::sleep(gap_dur);
                    } else {
                        thread::sleep(Duration::from_secs_f64(total_secs));
                    }
                }

                if !stopped {
                    println!("KARAOKE_DONE");
                    let _ = io::stdout().flush();
                }
            }));
        } else if line == "KARAOKE_OFF" {
            stop_signal.store(true, Ordering::SeqCst);
            println!("MIDI PANIC");
            let _ = io::stdout().flush();
        } else if line.starts_with("LESSON ") {
            let mut s = state.lock().unwrap();
            s.melody.clear();
            // LESSON <id> <key> <time> <bpm> [<title>]
            let fields: Vec<&str> = line.split_whitespace().collect();
            if let Some(time_str) = fields.get(3) {
                if let Some(num_str) = time_str.split('/').next() {
                    if let Ok(n) = num_str.parse::<u32>() {
                        if n > 0 { s.beats_per_bar = n; }
                    }
                }
            }
            if let Some(bpm_str) = fields.get(4) {
                if let Ok(n) = bpm_str.parse::<f64>() {
                    if n > 0.0 { s.bpm = n; }
                }
            }
            // REMOVED PASS THROUGH
        } else if line.starts_with("MELODY ") {
            if let Some((_, content)) = line.split_once(": ") {
                let mut s = state.lock().unwrap();
                for tok in content.split_whitespace() {
                    if let Some(n) = parse_token(tok) {
                        s.melody.push(n);
                    }
                }
            }
            // REMOVED PASS THROUGH
        }
        // General pass-through block deleted.
    }
    if let Some(h) = last_handle {
        h.join().ok();
    }
}
