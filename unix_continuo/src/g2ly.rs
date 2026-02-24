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

/// Parse a GROUP line into a Group struct.
/// Format: GROUP ID:<n> [passing] BASS:<note> FIGURES:<fig> MELODY:<mel>
///                BASS_ACTUAL:<note> TIME:<t> REALIZATION:<p1/p2/...>
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
            // MELODY may span multiple tokens until the next labeled field
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
        id,
        passing,
        bass,
        figures,
        melody: melody_toks.join(" "),
        bass_actual,
        time_ms,
        realization,
    })
}

/// True if a token starts with a known labeled-field prefix.
fn is_labeled(tok: &str) -> bool {
    matches!(
        tok.split(':').next().unwrap_or(""),
        "ID" | "passing" | "BASS" | "BASS_ACTUAL" | "FIGURES" | "MELODY" | "TIME" | "REALIZATION"
    )
}

/// Parse: LESSON <num> <key> <time_sig> <title...>
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

/// Strip duration suffix: "g2" -> "g", "cis''4" -> "cis''", "fis4." -> "fis".
fn pitch_only(token: &str) -> &str {
    let b = token.as_bytes();
    let mut i = 0;
    while i < b.len() && b[i].is_ascii_alphabetic() {
        i += 1;
    }
    while i < b.len() && (b[i] == b'\'' || b[i] == b',') {
        i += 1;
    }
    &token[..i]
}

/// Extract duration suffix: "g2" -> "2", "fis4." -> "4.".
fn duration_of(token: &str) -> &str {
    &token[pitch_only(token).len()..]
}

/// LilyPond pitch name -> MIDI note number (c' = middle C = 60).
fn pitch_to_midi(pitch: &str) -> Option<i32> {
    let end = pitch
        .find(|c: char| c == '\'' || c == ',')
        .unwrap_or(pitch.len());
    let name = &pitch[..end];
    let octs = &pitch[end..];
    let pc: i32 = match name {
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
    let octave: i32 = 3 + octs
        .chars()
        .map(|c| match c {
            '\'' => 1,
            ',' => -1,
            _ => 0,
        })
        .sum::<i32>();
    Some((octave + 1) * 12 + pc)
}

/// True if the pitch belongs on the treble staff (MIDI >= 60).
fn is_treble(pitch: &str) -> bool {
    pitch_to_midi(pitch).map_or(false, |m| m >= 60)
}

// ---------------------------------------------------------------------------
// Tie helpers
// ---------------------------------------------------------------------------

/// True if every pitch in `notes` also appears in `next_realization`
/// (exact string match) and `notes` is non-empty.
fn all_continue(notes: &[&str], next_realization: &[String]) -> bool {
    !notes.is_empty()
        && notes
            .iter()
            .all(|p| next_realization.iter().any(|q| q == *p))
}

/// Format a chord/note token for a realization voice, with optional tie.
/// Empty slice -> spacer rest (never tied).
fn realization_token(notes: &[&str], dur: &str, tie: bool) -> String {
    let t = if tie { "~" } else { "" };
    match notes.len() {
        0 => format!("s{}", dur),
        1 => format!("{}{}{}", notes[0], dur, t),
        _ => format!("<{}>{}{}", notes.join(" "), dur, t),
    }
}

// ---------------------------------------------------------------------------
// Figured bass (ported from l2ly, adapted for per-group data)
// ---------------------------------------------------------------------------

// Input figure tokens (g.figures field):
//   "0"        -> <_>       (no figure)
//   "6"        -> <6>
//   "#6"       -> <6+>      (raised sixth)
//   "b6"       -> <6->      (lowered sixth)
//   "#"        -> <_+>      (raised third, continuo shorthand)
//   "2/#4/6"   -> <2 4+ 6>  (stacked figures, slash-separated)
//
// Passing groups always emit <_\\> regardless of their figures field,
// matching the traditional passing-note solidus mark.

/// Convert a single interval string to a LilyPond figure interval.
fn interval_to_ly(s: &str) -> String {
    if s == "#" {
        return "_+".to_string();
    }
    if s == "b" {
        return "_-".to_string();
    }
    let (acc, num) = if s.starts_with('#') {
        ("+", &s[1..])
    } else if s.starts_with('b') {
        ("-", &s[1..])
    } else {
        ("", s)
    };
    if acc.is_empty() {
        num.to_string()
    } else {
        format!("{}{}", num, acc)
    }
}

/// Convert one figure token (e.g. "0", "#6", "2/#4/6") to a \figuremode group.
fn parse_figure(fig: &str) -> String {
    let fig = fig.trim();
    if fig == "0" {
        return "<_>".to_string();
    }
    let parts: Vec<String> = fig.split('/').map(interval_to_ly).collect();
    format!("<{}>", parts.join(" "))
}

/// Build the \figuremode token stream for staff 4.
/// Each group contributes one token: passing groups get <_\\>, others
/// get parse_figure(g.figures).  The duration comes from g.bass.
fn build_figures(groups: &[Group]) -> String {
    groups
        .iter()
        .map(|g| {
            let dur = duration_of(&g.bass);
            let group = if g.passing {
                r"<_\\>".to_string() // passing-note solidus (augmented-slash in figuremode)
            } else {
                parse_figure(&g.figures)
            };
            format!("{}{}", group, dur)
        })
        .collect::<Vec<_>>()
        .join(" ")
}

// ---------------------------------------------------------------------------
// Voice builders
// ---------------------------------------------------------------------------

/// Staff 1: melody, verbatim pass-through. "-"/empty -> spacer rest.
fn build_melody(groups: &[Group]) -> String {
    groups
        .iter()
        .map(|g| {
            if g.melody == "-" || g.melody.is_empty() {
                format!("s{}", duration_of(&g.bass))
            } else {
                g.melody.clone()
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

/// Staff 2: high realization notes (MIDI >= 60), with ties into passing groups.
fn build_real_treble(groups: &[Group]) -> String {
    groups
        .iter()
        .enumerate()
        .map(|(i, g)| {
            let dur = duration_of(&g.bass);
            let notes: Vec<&str> = g
                .realization
                .iter()
                .map(|s| s.as_str())
                .filter(|&p| is_treble(p))
                .collect();
            let tie = groups.get(i + 1).map_or(false, |next| {
                next.passing && all_continue(&notes, &next.realization)
            });
            realization_token(&notes, dur, tie)
        })
        .collect::<Vec<_>>()
        .join(" ")
}

/// Staff 3, voice 1: BASS_ACTUAL note, one per group, no ties.
fn build_bass_actual(groups: &[Group]) -> String {
    groups
        .iter()
        .map(|g| format!("{}{}", g.bass_actual, duration_of(&g.bass)))
        .collect::<Vec<_>>()
        .join(" ")
}

/// Staff 3, voice 2: low realization notes (MIDI < 60), with ties into passing groups.
fn build_real_bass(groups: &[Group]) -> String {
    groups
        .iter()
        .enumerate()
        .map(|(i, g)| {
            let dur = duration_of(&g.bass);
            let notes: Vec<&str> = g
                .realization
                .iter()
                .map(|s| s.as_str())
                .filter(|&p| !is_treble(p))
                .collect();
            let tie = groups.get(i + 1).map_or(false, |next| {
                next.passing && all_continue(&notes, &next.realization)
            });
            realization_token(&notes, dur, tie)
        })
        .collect::<Vec<_>>()
        .join(" ")
}

/// Staff 4: BASS (written/expected) note, one per group, no ties.
fn build_bass_expected(groups: &[Group]) -> String {
    groups
        .iter()
        .map(|g| format!("{}{}", pitch_only(&g.bass), duration_of(&g.bass)))
        .collect::<Vec<_>>()
        .join(" ")
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
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if line.starts_with("LESSON ") {
            if let Some((k, t, ttl)) = parse_lesson(line) {
                key = k.to_lowercase();
                time = t;
                title = ttl;
            }
        } else if line.starts_with("GROUP ") {
            if let Some(g) = parse_group(line) {
                groups.push(g);
            } else {
                eprintln!("Warning: could not parse GROUP line: {}", line);
            }
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
\header {{
  title = "{title}"
}}

%% Solidus glyph for passing notes in figured bass.
passingNoteSolidus = \markup
  \override #'(thickness . 1.4)
  \draw-line #'(0.55 . 1.4)

#(define-public (continuo-format-bass-figure figure event context)
   (let ((slash (eq? #t (ly:event-property event 'augmented-slash))))
     (if (and slash (not (number? figure)))
         passingNoteSolidus
         (format-bass-figure figure event context))))

\layout {{
  \context {{
    \Score
    figuredBassFormatter = #continuo-format-bass-figure
  }}
}}

\score {{
  <<
    %% 1. Melody (treble)
    \new Staff = "melody" {{
      \clef treble
      \key {key} \major
      \time {time}
      {melody}
    }}
    %% 2. High realization notes (treble)
    \new Staff = "real-treble" {{
      \clef treble
      \key {key} \major
      \time {time}
      {real_treble}
    }}
    %% 3. BASS_ACTUAL (voice 1) + low realization notes (voice 2)
    \new Staff = "real-bass" {{
      \clef bass
      \key {key} \major
      \time {time}
      <<
        \new Voice = "bass-actual" {{
          \voiceOne
          {bass_actual}
        }}
        \new Voice = "real-bass-notes" {{
          \voiceTwo
          {real_bass}
        }}
      >>
    }}
    %% 4. Written/expected bass note + figured bass below
    \new Staff = "bass" {{
      \clef bass
      \key {key} \major
      \time {time}
      <<
        \new Voice = "bass-expected" {{
          {bass_expected}
        }}
        \new FiguredBass {{
          \figuremode {{
            {figures}
          }}
        }}
      >>
    }}
  >>
}}"#,
        title = title,
        key = key,
        time = time,
        melody = melody,
        real_treble = real_treble,
        bass_actual = bass_actual,
        real_bass = real_bass,
        bass_expected = bass_expected,
        figures = figures,
    );

    print!("{}", doc);
}
