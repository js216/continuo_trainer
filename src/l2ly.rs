// SPDX-License-Identifier: MIT
// l2ly.rs --- convert lesson file into a minimal LilyPond grand staff
// Copyright (c) 2026 Jakob Kastelic
//
// DESCRIPTION
//     l2ly reads a lesson file from stdin and writes a two-staff LilyPond
//     (.ly) document to stdout: melody in treble clef, bassline with figured
//     bass in bass clef.
//
// INPUT FORMAT
//     A lesson file in seq/*.txt or chn/*.txt format.  Metadata as
//     "key: value" lines; sections as brace-delimited blocks:
//         title:   <text>       optional; absent in chunk files
//         key:     <text>       key letter, e.g. G
//         time:    <text>       time signature, e.g. 4/4; absent in chunks
//         bassline = { <tokens> }
//         figures  = { <tokens> }
//         melody   = { <tokens> }
//
//     Bass tokens with a trailing 'p' (e.g. fis2p) are passing notes:
//     the 'p' is stripped and the figure is replaced with a solidus (<_\>).
//
// OUTPUT FORMAT
//     A complete LilyPond document ready for lilypond(1).
//
// FIGURED BASS TRANSLATION
//     "0"        → <_>          no figure
//     "#" / "b"  → <_+> / <_->  raised/lowered third
//     "#6"/"b6"  → <6+> / <6->
//     "4+" / "+4" → <4\+>        plus sign (= raised); 4+ sets
//                                figuredBassPlusDirection = #RIGHT,
//                                +4 sets #LEFT (per-figure switching)
//     "6\/"      → <6/>         forward slash through figure (= raised)
//     "6\\"      → <6\\>        backward slash through figure (= raised)
//     "a/b/c"    → <a b c>      slash-separated stack (\/ and \\ escape /)
//
// DIAGNOSTICS
//     The program exits with an error message if the lesson file cannot be
//     read from standard input.

use std::io::{self, Read};

fn main() {
    // Read entire lesson from stdin
    let mut content = String::new();
    io::stdin()
        .read_to_string(&mut content)
        .expect("Cannot read lesson file from stdin");

    // Extract fields
    let title = extract_field(&content, "title");
    let composer = extract_field(&content, "composer");
    let key = extract_field(&content, "key");
    let time = extract_field(&content, "time");
    let bar: u32 = extract_field(&content, "bar").parse().unwrap_or(1);
    let bassline = extract_block(&content, "bassline");
    let figures = extract_block(&content, "figures");
    let melody = extract_block(&content, "melody");

    // LilyPond requires lowercase pitch names for \key
    let key_ly = key.to_lowercase();

    // Optional \header block: omitted entirely when neither title nor composer present.
    let header_block = if title.is_empty() && composer.is_empty() {
        String::new()
    } else {
        let mut h = String::from("\\header {\n");
        if !title.is_empty() {
            h.push_str(&format!("  title = \"{}\"\n", title));
        }
        if !composer.is_empty() {
            h.push_str(&format!("  composer = \"{}\"\n", composer));
        }
        h.push_str("}\n\n");
        h
    };

    // Optional time signature.  When present, also emit \partial if the chunk
    // starts mid-bar (partial: field holds remaining sixteenths in first bar).
    let partial: u32 = extract_field(&content, "partial").parse().unwrap_or(0);
    let time_directive = if time.is_empty() {
        r"\cadenzaOn".to_string()
    } else if partial > 0 {
        format!(
            "\\time {}\n      \\partial {}",
            time,
            sixteenths_to_duration(partial)
        )
    } else {
        format!(r"\time {}", time)
    };
    // Bar number directive: only meaningful when a time signature is present.
    // 1. Set the starting number.
    // 2. break-visibility ##(#f #f #t) shows numbers only at the start of lines.
    // 3. self-alignment-X = #0 (or #CENTER) centers the number over the clef/staff start.
    // 4. barNumberVisibility = #all-bar-numbers-visible forces bar 1 to show up.
    let bar_directive = if time.is_empty() {
        String::new()
    } else {
        format!(
            "\\set Score.currentBarNumber = #{}\n      \
             \\override Score.BarNumber.break-visibility = ##(#f #f #t)\n      \
             \\override Score.BarNumber.self-alignment-X = #CENTER\n      \
             \\set Score.barNumberVisibility = #all-bar-numbers-visible",
            bar
        )
    };

    // Generate LilyPond document
    let lilypond = format!(
        r#"\version "2.22.1"
{header_block}%% Solidus glyph for passing notes in figured bass.
%% A simple diagonal stroke, sized and sloped to match the figure column.
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
    \new Staff = "melody" {{
      \clef treble
      \key {key_ly} \major
      {time_directive}
      {bar_directive}
      {melody}
    }}
    \new Staff = "continuo" {{
      \clef bass
      \key {key_ly} \major
      {time_directive}
      <<
        \new Voice = "bassline" {{
          {bassline}
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
        header_block = header_block,
        key_ly = key_ly,
        time_directive = time_directive,
        bar_directive = bar_directive,
        melody = melody_to_ly(&melody),
        bassline = bassline_to_ly(&bassline),
        figures = figures_to_ly(&figures, &bassline),
    );

    // Write to stdout
    print!("{}", lilypond);
}

// Extract single-line fields  (e.g. "key: G")
fn extract_field(content: &str, field: &str) -> String {
    content
        .lines()
        .find(|l| l.to_lowercase().starts_with(&format!("{}:", field)))
        .map(|l| l.splitn(2, ':').nth(1).unwrap().trim().to_string())
        .unwrap_or_default()
}

// Extract the body of a named block:  name = { ... }
fn extract_block(content: &str, block_name: &str) -> String {
    let marker = format!("{} = {{", block_name);
    let start = content.find(&marker).unwrap();
    let rest = &content[start..];
    let start_brace = rest.find('{').unwrap() + 1;
    let end_brace = rest.find('}').unwrap();
    rest[start_brace..end_brace].trim().replace('\n', " ")
}

// Pass-through for melody notation.  Standalone "-" tokens (empty-group
// markers used in chunk files) are dropped; ties are normalised.
fn melody_to_ly(input: &str) -> String {
    let filtered: String = input
        .split_whitespace()
        .filter(|tok| *tok != "-")
        .collect::<Vec<_>>()
        .join(" ");
    filtered.replace("~ ", "~ ").replace("~", "~ ")
}

// Like melody_to_ly but also strips trailing 'p' passing-note markers from
// each token so LilyPond sees clean note names (e.g. "fis2p" → "fis2").
fn bassline_to_ly(input: &str) -> String {
    let cleaned: Vec<&str> = input
        .split_whitespace()
        .map(|tok| {
            if is_passing(tok) {
                tok.trim_end_matches('p')
            } else {
                tok
            }
        })
        .collect();
    cleaned.join(" ")
}

// Extract the duration suffix from a bassline note token (e.g. "g2" → "2",
// "fis4." → "4.", "fis2p" → "2").  Strips any trailing passing-note marker
// before scanning past pitch/accidental/octave characters.
fn note_duration(token: &str) -> &str {
    let token = token.trim_end_matches('p'); // strip passing-note marker if present
    let s = token.trim_start_matches(|c: char| c.is_alphabetic()); // pitch letter + is/es
    let s = s.trim_start_matches(|c: char| c == '\'' || c == ','); // octave marks
    s
}

fn is_passing(token: &str) -> bool {
    let b = token.as_bytes();
    b.last() == Some(&b'p')
        && b.len() > 1
        && (b[b.len() - 2].is_ascii_digit() || b[b.len() - 2] == b'.')
}

// Convert the figures block into \figuremode tokens, attaching the duration
// of the corresponding bassline note to each figure group so that LilyPond
// can lay them out correctly even without a simultaneous voice to borrow from.
//
// Input figure tokens:
//   0          → <_>      (no figure on this note)
//   6          → <6>
//   #6         → <6+>     (raised sixth)
//   b6         → <6->     (lowered sixth)
//   #          → <_+>     (raised third, figured-bass convention)
//   2/#4/6     → <2 4+ 6> (stacked, / as separator)
//
// Bassline notes marked with a trailing 'p' (e.g. fis2p) are passing notes:
// their figure is overridden with <_\\> (backward slash, traditional passing note mark)
// of what the figures block says for that beat.
fn figures_to_ly(figures: &str, bassline: &str) -> String {
    // Collect bassline tokens that carry a duration (skip ties ~ and barlines |)
    let bass_tokens: Vec<&str> = bassline
        .split_whitespace()
        .filter(|t| !matches!(*t, "~" | "|"))
        .collect();

    let mut plus_dir_is_right = false; // LilyPond default is LEFT
    let mut result = Vec::new();
    for (fig, bass) in figures.split_whitespace().zip(bass_tokens.iter()) {
        let dur = note_duration(bass);
        let (group, dir) = if is_passing(bass) {
            (r"<_\\>".to_string(), None)
        } else {
            parse_figure(fig)
        };
        if let Some(right) = dir {
            if right != plus_dir_is_right {
                plus_dir_is_right = right;
                let val = if right { "#RIGHT" } else { "#LEFT" };
                result.push(format!("\\set figuredBassPlusDirection = {}", val));
            }
        }
        result.push(format!("{}{}", group, dur));
    }
    result.join(" ")
}

// Convert one figure token to a \figuremode group.
// Returns (ly_string, plus_direction) where plus_direction is
// Some(true) for RIGHT, Some(false) for LEFT, None if no plus sign.
fn parse_figure(fig: &str) -> (String, Option<bool>) {
    let fig = fig.trim();
    if fig == "0" {
        return ("<_>".to_string(), None);
    }

    let tokens = split_figures(fig);
    let mut dir = None;
    for t in &tokens {
        if t.ends_with('+') {
            dir = Some(true); // RIGHT
        } else if t.starts_with('+') {
            dir = Some(false); // LEFT
        }
    }
    let parts: Vec<String> = tokens.iter().map(|s| interval_to_ly(s)).collect();
    (format!("<{}>", parts.join(" ")), dir)
}

// Split a figure string on '/' but treat '\/' and '\\' as escaped literals.
fn split_figures(s: &str) -> Vec<String> {
    let mut parts = Vec::new();
    let mut cur = String::new();
    let chars: Vec<char> = s.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        if chars[i] == '\\' && i + 1 < chars.len() && chars[i + 1] == '\\' {
            cur.push('\\');
            cur.push('\\');
            i += 2;
        } else if chars[i] == '\\' && i + 1 < chars.len() && chars[i + 1] == '/' {
            cur.push('\\');
            cur.push('/');
            i += 2;
        } else if chars[i] == '/' {
            parts.push(cur);
            cur = String::new();
            i += 1;
        } else {
            cur.push(chars[i]);
            i += 1;
        }
    }
    parts.push(cur);
    parts
}

// Convert a single interval token to a LilyPond figure interval.
//
// Input        LilyPond    Glyph
// #            _+          sharp on the 3rd (shorthand)
// b            _-          flat on the 3rd
// #6           6+          sharp accidental on 6
// b6           6-          flat accidental on 6
// 6+  / +6     6\+         plus sign on 6
// 6\/          6/          forward slash through 6
// 6\\          6\\         backward slash through 6
fn interval_to_ly(s: &str) -> String {
    if s == "#" {
        return "_+".to_string();
    }
    if s == "b" {
        return "_-".to_string();
    }

    // Backward slash through figure: 6\\ → 6\\
    if s.ends_with("\\\\") {
        let num = &s[..s.len() - 2];
        if num.is_empty() || !num.chars().all(|c| c.is_ascii_digit()) {
            eprintln!("error: invalid figure token: \"{}\"", s);
            std::process::exit(1);
        }
        return format!("{}\\\\", num);
    }

    // Forward slash through figure: 6\/ → 6/
    if s.ends_with("\\/") {
        let num = &s[..s.len() - 2];
        if num.is_empty() || !num.chars().all(|c| c.is_ascii_digit()) {
            eprintln!("error: invalid figure token: \"{}\"", s);
            std::process::exit(1);
        }
        return format!("{}/", num);
    }

    let (acc, num) = if s.starts_with('#') {
        ("+", &s[1..])
    } else if s.starts_with('b') {
        ("-", &s[1..])
    } else if s.ends_with('+') {
        ("\\+", &s[..s.len() - 1])
    } else if s.starts_with('+') {
        ("\\+", &s[1..])
    } else {
        ("", s)
    };

    if num.is_empty() || !num.chars().all(|c| c.is_ascii_digit()) {
        eprintln!("error: invalid figure token: \"{}\"", s);
        std::process::exit(1);
    }

    if acc.is_empty() {
        num.to_string()
    } else {
        format!("{}{}", num, acc)
    }
}

// Convert a duration expressed in sixteenth-note units to a LilyPond duration
// string (e.g. 16 → "1", 12 → "2.", 8 → "2").  Panics for values that cannot
// be expressed as a single note value with at most two dots.
fn sixteenths_to_duration(n: u32) -> String {
    match n {
        1 => "16",
        2 => "8",
        3 => "8.",
        4 => "4",
        6 => "4.",
        7 => "4..",
        8 => "2",
        12 => "2.",
        14 => "2..",
        16 => "1",
        24 => "1.",
        28 => "1..",
        _ => panic!(
            "partial bar of {} sixteenths cannot be expressed as a single LilyPond duration",
            n
        ),
    }
    .to_string()
}
