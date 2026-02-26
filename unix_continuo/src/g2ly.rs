// SPDX-License-Identifier: MIT
// g2ly.rs --- convert grouper output into a four-staff LilyPond score
// Copyright (c) 2026 Jakob Kastelic

use std::io::{self, BufRead};

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

#[derive(Debug)]
#[allow(dead_code)]
struct Group {
    id: usize,
    passing: bool,
    bass: String, // written bass note + duration, e.g. "g2"
    figures: String,
    melody: String,      // "-" or multi-token, e.g. "b'4 cis''4"
    bass_actual: String, // played bass pitch (no duration), e.g. "g"
    time_ms: u64,
    realization: Vec<String>, // pitch names only, highest to lowest
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

fn parse_group(line: &str) -> Option<Group> {
    let rest = line.strip_prefix("GROUP ")?;
    let tokens: Vec<&str> = rest.split_whitespace().collect();

    let mut id = 0usize;
    let mut passing = false;
    let mut bass = String::new();
    let mut figures = String::new();
    let mut melody_toks: Vec<&str> = Vec::new();
    let mut bass_actual = String::new();
    let mut time_ms = 0u64;
    let mut realization: Vec<String> = Vec::new();

    let mut i = 0;
    while i < tokens.len() {
        let tok = tokens[i];
        if let Some(v) = tok.strip_prefix("ID:") {
            id = v.parse().unwrap_or(0);
            i += 1;
        } else if tok == "passing" {
            passing = true;
            i += 1;
        } else if let Some(v) = tok.strip_prefix("BASS:") {
            bass = v.to_string();
            i += 1;
        } else if let Some(v) = tok.strip_prefix("FIGURES:") {
            figures = v.to_string();
            i += 1;
        } else if tok.starts_with("MELODY:") {
            melody_toks.push(tok.strip_prefix("MELODY:").unwrap());
            i += 1;
            while i < tokens.len() && !is_labeled(tokens[i]) {
                melody_toks.push(tokens[i]);
                i += 1;
            }
        } else if let Some(v) = tok.strip_prefix("BASS_ACTUAL:") {
            bass_actual = v.to_string();
            i += 1;
        } else if let Some(v) = tok.strip_prefix("TIME:") {
            time_ms = v.parse().unwrap_or(0);
            i += 1;
        } else if let Some(v) = tok.strip_prefix("REALIZATION:") {
            realization = if v.is_empty() {
                Vec::new()
            } else {
                v.split('/').map(|s| s.to_string()).collect()
            };
            i += 1;
        } else {
            i += 1;
        }
    }

    Some(Group {
        id, passing, bass, figures, melody: melody_toks.join(" "),
        bass_actual, time_ms, realization,
    })
}

fn is_labeled(tok: &str) -> bool {
    matches!(
        tok.split(':').next().unwrap_or(""),
        "ID" | "passing" | "BASS" | "BASS_ACTUAL" | "FIGURES" | "MELODY" | "TIME" | "REALIZATION"
    )
}

fn parse_lesson(line: &str) -> Option<(String, String, String)> {
    let rest = line.strip_prefix("LESSON ")?;
    let mut parts = rest.splitn(4, ' ');
    let _num = parts.next()?;
    let key = parts.next()?.to_string();
    let time = parts.next()?.to_string();
    let title = parts.next().unwrap_or("").trim().to_string();
    Some((key, time, title))
}

// ---------------------------------------------------------------------------
// Pitch utilities
// ---------------------------------------------------------------------------

fn pitch_only(token: &str) -> &str {
    let b = token.as_bytes();
    let mut i = 0;
    while i < b.len() && b[i].is_ascii_alphabetic() { i += 1; }
    while i < b.len() && (b[i] == b'\'' || b[i] == b',') { i += 1; }
    &token[..i]
}

fn duration_of(token: &str) -> &str {
    &token[pitch_only(token).len()..]
}

fn pitch_to_midi(pitch: &str) -> Option<i32> {
    let end = pitch.find(|c: char| c == '\'' || c == ',').unwrap_or(pitch.len());
    let name = &pitch[..end];
    let octs = &pitch[end..];
    let pc: i32 = match name {
        "c" | "bis" => 0, "cis" | "des" => 1, "d" => 2, "dis" | "ees" => 3,
        "e" | "fes" => 4, "f" | "eis" => 5, "fis" | "ges" => 6, "g" => 7,
        "gis" | "aes" => 8, "a" => 9, "ais" | "bes" => 10, "b" | "ces" => 11,
        _ => return None,
    };
    let octave: i32 = 3 + octs.chars().map(|c| match c {
        '\'' => 1, ',' => -1, _ => 0,
    }).sum::<i32>();
    Some((octave + 1) * 12 + pc)
}

fn is_treble(pitch: &str) -> bool {
    pitch_to_midi(pitch).map_or(false, |m| m >= 60)
}

// ---------------------------------------------------------------------------
// Realization Formatting
// ---------------------------------------------------------------------------

fn all_continue(notes: &[&str], next_realization: &[String]) -> bool {
    !notes.is_empty() && notes.iter().all(|p| next_realization.iter().any(|q| q == *p))
}

/// Returns a note, chord, or an invisible spacer 's' to maintain group alignment.
fn realization_token(notes: &[&str], dur: &str, tie: bool) -> String {
    let t = if tie { "~" } else { "" };
    match notes.len() {
        0 => format!("s{}", dur), // Insert spacer to preserve group timing
        1 => format!("{}{}{}", notes[0], dur, t),
        _ => format!("<{}>{}{}", notes.join(" "), dur, t),
    }
}

// ---------------------------------------------------------------------------
// Figured bass
// ---------------------------------------------------------------------------

fn interval_to_ly(s: &str) -> String {
    if s == "#" { return "_+".to_string(); }
    if s == "b" { return "_-".to_string(); }
    let (acc, num) = if s.starts_with('#') { ("+", &s[1..]) }
                     else if s.starts_with('b') { ("-", &s[1..]) }
                     else { ("", s) };
    if acc.is_empty() { num.to_string() } else { format!("{}{}", num, acc) }
}

fn parse_figure(fig: &str) -> String {
    let fig = fig.trim();
    if fig == "0" { return "<_>".to_string(); }
    let parts: Vec<String> = fig.split('/').map(interval_to_ly).collect();
    format!("<{}>", parts.join(" "))
}

fn build_figures(groups: &[Group]) -> String {
    groups.iter().map(|g| {
        let dur = duration_of(&g.bass);
        let group = if g.passing { r"<_\\>".to_string() } else { parse_figure(&g.figures) };
        format!("{}{}", group, dur)
    }).collect::<Vec<_>>().join(" ")
}

// ---------------------------------------------------------------------------
// Voice builders
// ---------------------------------------------------------------------------

/// Staff 1: Skips "-" placeholders because melody durations are user-defined.
fn build_melody(groups: &[Group]) -> String {
    groups.iter().filter_map(|g| {
        if g.melody == "-" || g.melody.is_empty() { None } else { Some(g.melody.clone()) }
    }).collect::<Vec<_>>().join(" ")
}

/// Staff 2: Generates high realization notes, using spacers for alignment.
fn build_real_treble(groups: &[Group]) -> String {
    groups.iter().enumerate().map(|(i, g)| {
        let dur = duration_of(&g.bass);
        let notes: Vec<&str> = g.realization.iter().map(|s| s.as_str()).filter(|&p| is_treble(p)).collect();
        let tie = groups.get(i + 1).map_or(false, |next| {
            next.passing && all_continue(&notes, &next.realization)
        });
        realization_token(&notes, dur, tie)
    }).collect::<Vec<_>>().join(" ")
}

fn build_bass_actual(groups: &[Group]) -> String {
    groups.iter().map(|g| format!("{}{}", g.bass_actual, duration_of(&g.bass))).collect::<Vec<_>>().join(" ")
}

/// Staff 3: Generates low realization notes, using spacers for alignment.
fn build_real_bass(groups: &[Group]) -> String {
    groups.iter().enumerate().map(|(i, g)| {
        let dur = duration_of(&g.bass);
        let notes: Vec<&str> = g.realization.iter().map(|s| s.as_str()).filter(|&p| !is_treble(p)).collect();
        let tie = groups.get(i + 1).map_or(false, |next| {
            next.passing && all_continue(&notes, &next.realization)
        });
        realization_token(&notes, dur, tie)
    }).collect::<Vec<_>>().join(" ")
}

fn build_bass_expected(groups: &[Group]) -> String {
    groups.iter().map(|g| format!("{}{}", pitch_only(&g.bass), duration_of(&g.bass))).collect::<Vec<_>>().join(" ")
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() {
    let stdin = io::stdin();
    let mut key = String::from("c");
    let mut time = String::from("4/4");
    let mut title = String::from("Untitled");
    let mut groups: Vec<Group> = Vec::new();

    for raw in stdin.lock().lines() {
        let line = raw.expect("read error");
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        if line.starts_with("LESSON ") {
            if let Some((k, t, ttl)) = parse_lesson(line) {
                key = k.to_lowercase(); time = t; title = ttl;
            }
        } else if line.starts_with("GROUP ") {
            if let Some(g) = parse_group(line) { groups.push(g); }
        }
    }

    let melody = build_melody(&groups);
    let real_treble = build_real_treble(&groups);
    let bass_actual = build_bass_actual(&groups);
    let real_bass = build_real_bass(&groups);
    let bass_expected = build_bass_expected(&groups);
    let figures = build_figures(&groups);

    let doc = format!(
        r#"\version "2.24.2"
\header {{ title = "{title}" }}

passingNoteSolidus = \markup \override #'(thickness . 1.4) \draw-line #'(0.55 . 1.4)

#(define-public (continuo-format-bass-figure figure event context)
   (let ((slash (eq? #t (ly:event-property event 'augmented-slash))))
     (if (and slash (not (number? figure)))
         passingNoteSolidus
         (format-bass-figure figure event context))))

\layout {{ \context {{ \Score figuredBassFormatter = #continuo-format-bass-figure }} }}

\score {{
  <<
    \new Staff = "melody" {{ \clef treble \key {key} \major \time {time} {melody} }}
    \new Staff = "real-treble" {{ \clef treble \key {key} \major \time {time} {real_treble} }}
    \new Staff = "real-bass" {{
      \clef bass \key {key} \major \time {time}
      <<
        \new Voice = "bass-actual" {{ \voiceOne {bass_actual} }}
        \new Voice = "real-bass-notes" {{ \voiceTwo {real_bass} }}
      >>
    }}
    \new Staff = "bass" {{
      \clef bass \key {key} \major \time {time}
      <<
        \new Voice = "bass-expected" {{ {bass_expected} }}
        \new FiguredBass {{ \figuremode {{ {figures} }} }}
      >>
    }}
  >>
}}"#,
        title = title, key = key, time = time, melody = melody, real_treble = real_treble,
        bass_actual = bass_actual, real_bass = real_bass, bass_expected = bass_expected, figures = figures,
    );

    print!("{}", doc);
}
