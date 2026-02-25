// SPDX-License-Identifier: MIT
// entry.rs --- MIDI text stream to Lilypond-style chord sequencer
// Copyright (c) 2026 Jakob Kastelic

use std::collections::{HashMap, HashSet};
use std::env;
use std::fs;
use std::io::{self, BufRead, Write};
use std::path::Path;

fn get_note_value(note: &str) -> i32 {
    match note {
        "c" => 0, "cis" | "des" => 1, "d" => 2, "dis" | "ees" => 3,
        "e" => 4, "f" => 5, "fis" | "ges" => 6, "g" => 7,
        "gis" | "aes" => 8, "a" => 9, "ais" | "bes" => 10, "b" => 11,
        _ => 0,
    }
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
    let mut outdir: Option<String> = None;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--key" => { key = args[i+1].clone(); i += 2; }
            "--time" => { time = args[i+1].clone(); i += 2; }
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
        let mut p = format!("{}/{}.txt", dir, n);
        while Path::new(&p).exists() {
            n += 1;
            p = format!("{}/{}.txt", dir, n);
        }
        Some(p)
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
                    write_data(fs::File::create(path)?, &key, &time, &title, &history)?;
                }
            }
        }
    }

    if outdir.is_none() {
        write_data(io::stdout(), &key, &time, &title, &history)?;
    }
    Ok(())
}

fn write_data<W: Write>(mut w: W, key: &str, time: &str, title: &str, hist: &Vec<Vec<String>>) -> io::Result<()> {
    // Add \n after time: to create the empty line
    writeln!(w, "title: {}\nkey: {}\ntime: {}\n", title, key, time)?;

    let mut v: HashMap<String, Vec<String>> = HashMap::new();
    let names = vec!["bassline", "melody", "figures"];
    for n in &names { v.insert(n.to_string(), Vec::new()); }

    for chord in hist {
        let sc: Vec<String> = chord.iter().map(|n| spell_note(n, key)).collect();
        if sc.len() == 1 {
            v.get_mut("bassline").unwrap().push(sc[0].clone());
            v.get_mut("melody").unwrap().push("r".to_string());
        } else {
            v.get_mut("bassline").unwrap().push(sc[0].clone());
            v.get_mut("melody").unwrap().push(sc.last().unwrap().clone());
            for (idx, note) in sc.iter().enumerate().skip(1).take(sc.len() - 2) {
                v.entry(format!("melody_{}", idx + 1)).or_default().push(note.clone());
            }
        }
        v.get_mut("figures").unwrap().push("0".to_string());
    }

    let mut ks: Vec<_> = v.keys().collect();
    ks.sort_by(|a, b| match (a.as_str(), b.as_str()) {
        ("bassline", _) => std::cmp::Ordering::Less,
        (_, "bassline") => std::cmp::Ordering::Greater,
        ("figures", _) => std::cmp::Ordering::Less,
        (_, "figures") => std::cmp::Ordering::Greater,
        _ => a.cmp(b),
    });

    for k in ks {
        // Add \n after the closing brace to create space between blocks
        writeln!(w, "{} = {{\n  {}\n}}\n", k, v.get(k).unwrap().join(" "))?;
    }
    Ok(())
}
