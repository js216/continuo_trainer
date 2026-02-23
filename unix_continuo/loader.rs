/* loader.c --- load lesson data and metadata from disk into pipeline
 *
 * Example input:
 *   LOAD_LESSON 3
 *
 * Example output:
 *   LESSON 3 G 3/4 Purcell bars 48-52
 *   BASSNOTE 0: C2
 *   FIGURES 0: 6/4
 *   MELODY 0: F4
 *   BASSNOTE 1: D2
 *   FIGURES 1: #3
 *   MELODY 1: F4
 *   ...
 */

use std::fs;
use std::io;

fn main() {
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    let mut parts = input.split_whitespace();

    if parts.next() != Some("LOAD_LESSON") {
        eprintln!("Expected LOAD_LESSON <number>");
        return;
    }
    let n: usize = match parts.next().and_then(|x| x.parse().ok()) {
        Some(x) => x,
        None => { eprintln!("Invalid lesson number"); return; }
    };

    let file = format!("lessons/{}.txt", n);
    let content = match fs::read_to_string(&file) {
        Ok(c) => c,
        Err(e) => { eprintln!("Cannot read {}: {}", file, e); return; }
    };

    let mut title = ""; let mut key = ""; let mut time = "";
    let mut bass = Vec::new(); let mut figures = Vec::new(); let mut melody = Vec::new();
    let mut mode = "";

    for line in content.lines() {
        let l = line.trim();
        if l.is_empty() { continue }
        if l.starts_with("title:") { title = &l[6..].trim(); continue }
        if l.starts_with("key:") { key = &l[4..].trim(); continue }
        if l.starts_with("time:") { time = &l[5..].trim(); continue }
        if l.starts_with("bassline") { mode="bass"; continue }
        if l.starts_with("figures") { mode="fig"; continue }
        if l.starts_with("melody") { mode="mel"; continue }
        if l.starts_with('}') { mode=""; continue }

        match mode {
            "bass" => bass.extend(l.split_whitespace().map(str::to_string)),
            "fig" => figures.extend(l.split_whitespace().map(str::to_string)),
            "mel" => melody.extend(l.split_whitespace().map(str::to_string)),
            _ => {}
        }
    }

    if bass.len() != figures.len() {
        eprintln!("Length mismatch: bass={} figures={}", bass.len(), figures.len());
        return;
    }

    println!("LESSON {} {} {} {}", n, key, time, title);
    for i in 0..bass.len() {
        println!("BASSNOTE {}: {}", i, bass[i]);
        println!("FIGURES {}: {}", i, figures[i]);
        if i < melody.len() { println!("MELODY {}: {}", i, melody[i]); }
    }
}
