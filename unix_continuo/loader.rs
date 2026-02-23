/* loader.c --- load lesson data and metadata from disk into pipeline
 *
 * Reads lesson files (bassline, melody_1, melody_2, key, instructions)
 * plus any chunked lessons. Outputs lesson batch events (BEGIN_LESSON ...
 * END_LESSON) to stdout for GUI and grouper.
 *
 * Example input:
 *   LESSON_FILE lesson3.txt
 *
 * Example output:
 *   BEGIN_LESSON 3
 *     BASSLINE: C2 D2 E2 F2
 *     FIGURES: 0 4 # 6/4
 *     MELODY_1: G4 A4 B4 C5
 *     MELODY_2: E4 F4 G4 A4
 *   END_LESSON
 */

use std::fs::File;
use std::io::{self, BufRead};

struct Chord {
    bass: String,
    figures: Vec<String>,
    melody: Vec<String>,
}

struct Lesson {
    title: String,
    key: String,
    chords: Vec<Chord>,
}

fn parse_chord_line(line: &str) -> Option<Chord> {
    let parts: Vec<&str> = line.trim().split_whitespace().collect();
    if parts.is_empty() {
        return None;
    }

    let bass = parts[0].to_string();
    let figures = if parts.len() > 1 && !parts[1].is_empty() {
        parts[1].split(',').map(|s| s.to_string()).collect()
    } else {
        Vec::new()
    };

    let melody = if parts.len() > 2 {
        parts[2].split(',').map(|s| s.to_string()).collect()
    } else {
        Vec::new()
    };

    Some(Chord { bass, figures, melody })
}

fn load_lesson(filename: &str) -> io::Result<Lesson> {
    let file = File::open(filename)?;
    let reader = io::BufReader::new(file);

    let mut title = String::new();
    let mut key = String::new();
    let mut chords = Vec::new();

    for line in reader.lines() {
        let line = line?;
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        if let Some(rest) = line.strip_prefix("title:") {
            title = rest.trim().to_string();
        } else if let Some(rest) = line.strip_prefix("key:") {
            key = rest.trim().to_string();
        } else if let Some(chord) = parse_chord_line(line) {
            chords.push(chord);
        } else {
            eprintln!("Warning: unrecognized line: {}", line);
        }
    }

    Ok(Lesson { title, key, chords })
}

fn print_lesson_split(lesson: &Lesson, lesson_num: u32) {
    println!("BEGIN_LESSON {}", lesson_num);
    println!("  TITLE: {}", lesson.title);
    println!("  KEY: {}", lesson.key);

    // BASSLINE
    let bassline: Vec<String> = lesson.chords.iter().map(|c| c.bass.clone()).collect();
    println!("  BASSLINE: {}", bassline.join(" "));

    // FIGURES (print 0 if empty)
    let figures: Vec<String> = lesson.chords
        .iter()
        .map(|c| if c.figures.is_empty() { "0".to_string() } else { c.figures.join("/") })
        .collect();
    println!("  FIGURES: {}", figures.join(" "));

    // Find max number of melody voices
    let max_voices = lesson.chords.iter().map(|c| c.melody.len()).max().unwrap_or(0);

    // Print each melody voice
    for i in 0..max_voices {
        let melody_line: Vec<String> = lesson.chords.iter()
            .map(|c| c.melody.get(i).cloned().unwrap_or("0".to_string()))
            .collect();
        println!("  MELODY_{}: {}", i + 1, melody_line.join(" "));
    }

    println!("END_LESSON");
}

fn main() -> io::Result<()> {
    // Read lesson file name from stdin
    let mut input = String::new();
    io::stdin().read_line(&mut input)?;
    let input = input.trim();

    let lesson_file = if let Some(name) = input.strip_prefix("LESSON_FILE ") {
        name
    } else {
        eprintln!("Usage: LESSON_FILE <filename>");
        return Ok(());
    };

    // Extract lesson number, e.g., "1.txt" -> 1
    let lesson_num = lesson_file
        .rsplit('/').next()  // get last path component
        .and_then(|f| f.strip_suffix(".txt"))
        .and_then(|s| s.parse::<u32>().ok())
        .unwrap_or(0);

    let lesson = load_lesson(lesson_file)?;
    print_lesson_split(&lesson, lesson_num);

    Ok(())
}
