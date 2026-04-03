use std::io::{self, BufRead, Write};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Mutex,
};
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
    beats_denominator: u32,
    melody: Vec<Note>,
}

// ── Helpers ──────────────────────────────────────────────────────────────────

fn parse_token(tok: &str, beats_denominator: u32) -> Option<Note> {
    let tok = tok.replace('~', "");
    if tok.is_empty() {
        return None;
    }

    let is_rest = tok.starts_with('r');
    let mut lily = None;
    let mut midi_note = None;
    let mut cursor;

    if !is_rest {
        let mut name_val = None;
        let mut name_len = 0;
        for &(name, val) in &[
            ("cis", 1),
            ("dis", 3),
            ("fis", 6),
            ("gis", 8),
            ("ais", 10),
            ("c", 0),
            ("d", 2),
            ("e", 4),
            ("f", 5),
            ("g", 7),
            ("a", 9),
            ("b", 11),
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
                '\'' => {
                    octave += 1;
                    cursor += 1;
                }
                ',' => {
                    octave -= 1;
                    cursor += 1;
                }
                _ => break,
            }
        }

        lily = Some(tok[..cursor].to_string());
        let m = (octave + 1) * 12 + val;
        if !(0..=127).contains(&m) {
            return None;
        }
        midi_note = Some(m as u8);
    } else {
        cursor = 1;
    }

    let remainder = &tok[cursor..];
    let dur_end = remainder
        .find(|c: char| !c.is_ascii_digit())
        .unwrap_or(remainder.len());
    let dur_num: f64 = remainder[..dur_end].parse().ok().unwrap_or(4.0);
    cursor += dur_end;

    let mut dots = 0;
    let chars: Vec<char> = tok.chars().collect();
    while cursor < chars.len() && chars[cursor] == '.' {
        dots += 1;
        cursor += 1;
    }

    let dot_factor = (f64::powi(2.0, dots + 1) - 1.0) / f64::powi(2.0, dots);
    let beats = (beats_denominator as f64 / dur_num) * dot_factor;

    Some(Note {
        lily,
        midi: midi_note,
        beats,
    })
}

// ── Main ─────────────────────────────────────────────────────────────────────

fn main() {
    let state = Arc::new(Mutex::new(SharedState {
        bpm: 120.0,
        beats_per_bar: 4,
        beats_denominator: 4,
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

                // Count-in: one click per beat in the time signature.
                // BPM is in denominator-beat units (60 BPM in 3/2 = 1 half note/s),
                // so the click interval is simply 60/bpm seconds.
                let beat_click_secs = 60.0 / bpm;
                let click_on = Duration::from_millis(30);
                let click_off = Duration::from_secs_f64((beat_click_secs - 0.030_f64).max(0.0));
                for _ in 0..beats_per_bar {
                    if stop_ref.load(Ordering::SeqCst) {
                        return;
                    }
                    println!("MIDI NOTE_ON b'' VELOCITY:64");
                    let _ = io::stdout().flush();
                    thread::sleep(click_on);
                    println!("MIDI NOTE_OFF b''");
                    let _ = io::stdout().flush();
                    thread::sleep(click_off);
                    // Count-in beats are not numbered for scoring
                }

                let mut stopped = false;
                for note in melody {
                    if stop_ref.load(Ordering::SeqCst) {
                        stopped = true;
                        break;
                    }

                    let total_secs = note.beats * 60.0 / bpm;
                    if let (Some(lily), Some(_midi)) = (&note.lily, note.midi) {
                        let play_dur = Duration::from_secs_f64(total_secs * 0.9);
                        let gap_dur = Duration::from_secs_f64(total_secs * 0.1);

                        println!("MIDI NOTE_ON {} VELOCITY:80", lily);
                        let _ = io::stdout().flush();

                        thread::sleep(play_dur);
                        if stop_ref.load(Ordering::SeqCst) {
                            stopped = true;
                            break;
                        }

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
        } else if line == "KARAOKE_OFF" || line == "KARAOKE_STOP" {
            stop_signal.store(true, Ordering::SeqCst);
            println!("MIDI PANIC");
            let _ = io::stdout().flush();
            if line == "KARAOKE_STOP" {
                println!("KARAOKE_ABORT");
                let _ = io::stdout().flush();
            }
        } else if line.starts_with("BPM ") {
            // BPM <value> — issued by stats.lua; overrides any previous BPM.
            if let Some(bpm_str) = line.split_whitespace().nth(1) {
                if let Ok(n) = bpm_str.parse::<f64>() {
                    if n > 0.0 {
                        state.lock().unwrap().bpm = n;
                    }
                }
            }
        } else if line.starts_with("LESSON ") {
            let mut s = state.lock().unwrap();
            s.melody.clear();
            // LESSON <hash> <key> <time> <bpm> <bar>
            // BPM is no longer read here; stats.lua issues a BPM command instead.
            let fields: Vec<&str> = line.split_whitespace().collect();
            if let Some(time_str) = fields.get(3) {
                let parts: Vec<&str> = time_str.split('/').collect();
                if let Some(num_str) = parts.first() {
                    if let Ok(n) = num_str.parse::<u32>() {
                        if n > 0 {
                            s.beats_per_bar = n;
                        }
                    }
                }
                if let Some(den_str) = parts.get(1) {
                    if let Ok(n) = den_str.parse::<u32>() {
                        if n > 0 {
                            s.beats_denominator = n;
                        }
                    }
                }
            }
            // REMOVED PASS THROUGH
        } else if line.starts_with("MELODY ") {
            if let Some((_, content)) = line.split_once(": ") {
                let mut s = state.lock().unwrap();
                let denom = s.beats_denominator;
                for tok in content.split_whitespace() {
                    if let Some(n) = parse_token(tok, denom) {
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
