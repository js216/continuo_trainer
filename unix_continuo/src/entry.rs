// SPDX-License-Identifier: MIT
// entry.rs --- MIDI text stream to Lilypond-style chord sequencer
// Copyright (c) 2026 Jakob Kastelic

use std::collections::{HashMap, HashSet};
use std::env;
use std::fs;
use std::io::{self, BufRead, Write};
use std::path::Path;

fn get_note_value(note: &str) -> i32 {
    let base_name: String = note.chars().take_while(|c| c.is_alphabetic()).collect();
    let mut value = match base_name.as_str() {
        "c" => 0, "cis" | "des" => 1, "d" => 2, "dis" | "ees" => 3,
        "e" => 4, "f" => 5, "fis" | "ges" => 6, "g" => 7,
        "gis" | "aes" => 8, "a" => 9, "ais" | "bes" => 10, "b" => 11,
        _ => 0,
    };
    for c in note.chars() {
        if c == '\'' { value += 12; }
        if c == ',' { value -= 12; }
    }
    value
}

fn spell_note(note: &str, key: &str) -> String {
    if key == "F" {
        match note {
            "ais" => return "bes".to_string(),
            "dis" => return "ees".to_string(),
            _ => (),
        }
    }
    note.to_string()
}

fn main() -> io::Result<()> {
    let args: Vec<String> = env::args().collect();
    let mut key = "C".to_string();
    let mut time = "4/4".to_string();
    let mut title = "Untitled".to_string();
    let mut duration = "2".to_string();
    let mut outdir: Option<String> = None;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--key" => { key = args[i+1].clone(); i += 2; }
            "--time" => { time = args[i+1].clone(); i += 2; }
            "--duration" => { duration = args[i+1].clone(); i += 2; }
            "--outdir" => { outdir = Some(args[i+1].clone()); i += 2; }
            "--title" => {
                let mut t = Vec::new();
                i += 1;
                while i < args.len() && !args[i].starts_with("--") {
                    t.push(args[i].as_str());
                    i += 1;
                }
                title = t.join(" ");
            }
            _ => i += 1,
        }
    }

    let output_path = if let Some(ref dir) = outdir {
        let _ = fs::create_dir_all(dir);
        let mut n = 1;
        while Path::new(&format!("{}/{}.txt", dir, n)).exists() { n += 1; }
        Some(format!("{}/{}.txt", dir, n))
    } else {
        None
    };

    let stdin = io::stdin();
    let mut active_notes: HashSet<String> = HashSet::new();
    let mut current_chord: Vec<String> = Vec::new();
    let mut history: Vec<Vec<String>> = Vec::new();
    let mut pressed = false;

    for line in stdin.lock().lines() {
        let line = line?;
        if outdir.is_some() { println!("{}", line); }

        let p: Vec<&str> = line.split_whitespace().collect();
        if p.is_empty() { continue; }

        if p[0] == "NOTE_ON" {
            let note = p[1].to_string();
            active_notes.insert(note.clone());
            if !current_chord.contains(&note) { current_chord.push(note); }
            pressed = true;
        } else if p[0] == "NOTE_OFF" {
            active_notes.remove(p[1]);
            if active_notes.is_empty() && pressed {
                current_chord.sort_by_key(|n| get_note_value(n));
                history.push(current_chord.clone());
                current_chord.clear();
                pressed = false;
                if let Some(ref path) = output_path {
                    write_data(fs::File::create(path)?, &key, &time, &title, &history, &duration)?;
                }
            }
        }
    }

    if outdir.is_none() {
        write_data(io::stdout(), &key, &time, &title, &history, &duration)?;
    }
    Ok(())
}

fn write_data<W: Write>(
    mut w: W,
    key: &str,
    time: &str,
    title: &str,
    hist: &Vec<Vec<String>>,
    dur_str: &str,
) -> io::Result<()> {
    writeln!(w, "title: {}\nkey: {}\ntime: {}\n", title, key, time)?;

    let parts: Vec<&str> = time.split('/').collect();
    let beats_per_bar = parts[0].parse::<f32>().unwrap_or(4.0);
    let beat_unit = parts.get(1).and_then(|s| s.parse::<f32>().ok()).unwrap_or(4.0);
    let is_dotted = dur_str.contains('.');
    let dur_val = dur_str.trim_matches('.').parse::<f32>().unwrap_or(2.0);
    let dur_in_beats =
        (beat_unit / dur_val) * (if is_dotted { 1.5 } else { 1.0 });
    let n_per_bar = (beats_per_bar / dur_in_beats).round() as usize;

    let mut voices: HashMap<String, Vec<String>> = HashMap::new();
    for n in &["bassline", "melody", "figures"] {
        voices.insert(n.to_string(), Vec::new());
    }

    for (i, chord) in hist.iter().enumerate() {
        let sc: Vec<String> =
            chord.iter().map(|n| spell_note(n, key)).collect();
        let fmt = |s: &str| format!("{}{}", s, dur_str);

        let mut assign = vec![
            ("bassline".to_string(), fmt(&sc[0])),
            ("figures".to_string(), "0".to_string()), // ← FIXED
        ];

        if sc.len() == 1 {
            assign.push(("melody".to_string(), fmt("r")));
        } else {
            assign.push(("melody".to_string(), fmt(sc.last().unwrap())));
            for (idx, note) in sc.iter().enumerate().skip(1).take(sc.len() - 2) {
                assign.push((format!("melody_{}", idx + 1), fmt(note)));
            }
        }

        for (v_name, val) in assign {
            let v = voices.entry(v_name).or_default();
            v.push(val);
            if (i + 1) % n_per_bar == 0 && i != hist.len() - 1 {
                v.push("\n".to_string());
            }
        }
    }

    let mut ks: Vec<_> = voices.keys().collect();
    ks.sort_by(|a, b| match (a.as_str(), b.as_str()) {
        ("bassline", _) => std::cmp::Ordering::Less,
        (_, "bassline") => std::cmp::Ordering::Greater,
        ("figures", _) => std::cmp::Ordering::Less,
        (_, "figures") => std::cmp::Ordering::Greater,
        _ => a.cmp(b),
    });

    for k in ks {
        let raw = voices.get(k).unwrap().join(" ");
        let lines: Vec<&str> = raw.split('\n').collect();

        writeln!(w, "{} = {{", k)?;
        for line in lines {
            let trimmed = line.trim();
            if !trimmed.is_empty() {
                writeln!(w, "  {}", trimmed)?;
            }
        }
        writeln!(w, "}}\n")?;
    }

    Ok(())
}
