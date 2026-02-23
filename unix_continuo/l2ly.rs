/* l2ly.rs --- convert lesson file from stdin into minimal LilyPond grand staff
 *
 * Input: lesson file via stdin (title, key, time, bassline, figures, melody)
 * Output: LilyPond document on stdout (grand staff, figures above bassline)
 *
 * Usage:
 *   build/l2ly < lessons/1.txt > lessons/1.ly
 *
 * Features:
 *   - Melody in treble clef
 *   - Bassline in bass clef with figured bass below
 *   - Continuo figures converted to proper LilyPond \figuremode
 *   - Fully Unix-style: stdin/stdout
 */

use std::io::{self, Read};

fn main() {
    // Read entire lesson from stdin
    let mut content = String::new();
    io::stdin().read_to_string(&mut content)
        .expect("Cannot read lesson file from stdin");

    // Extract fields
    let title  = extract_field(&content, "title");
    let key    = extract_field(&content, "key");
    let time   = extract_field(&content, "time");
    let bassline = extract_block(&content, "bassline");
    let figures  = extract_block(&content, "figures");
    let melody   = extract_block(&content, "melody");

    // LilyPond requires lowercase pitch names for \key
    let key_ly = key.to_lowercase();

    // Generate LilyPond document
    let lilypond = format!(
        r#"\version "2.24.2"
\header {{
  title = "{title}"
}}

\score {{
  <<
    \new Staff = "melody" {{
      \clef treble
      \key {key_ly} \major
      \time {time}
      {melody}
    }}
    \new Staff = "continuo" {{
      \clef bass
      \key {key_ly} \major
      \time {time}
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
  \layout {{ }}
}}"#,
        title   = title,
        key_ly  = key_ly,
        time    = time,
        melody  = melody_to_ly(&melody),
        bassline = melody_to_ly(&bassline),
        figures  = figures_to_ly(&figures, &bassline),
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
    let start  = content.find(&marker).unwrap();
    let rest   = &content[start..];
    let start_brace = rest.find('{').unwrap() + 1;
    let end_brace   = rest.find('}').unwrap();
    rest[start_brace..end_brace].trim().replace('\n', " ")
}

// Naive pass-through for melody / bassline notation
fn melody_to_ly(input: &str) -> String {
    input
        .replace("~ ", "~ ")   // normalise ties (idempotent)
        .replace("~",  "~ ")
}

// Extract the duration suffix from a bassline note token (e.g. "g2" → "2",
// "fis4." → "4.", "r8" → "8").  Scans past the pitch/accidental/octave
// characters and returns whatever digit-and-dot tail remains.
fn note_duration(token: &str) -> &str {
    // Skip leading pitch letter (a-g or r for rest)
    let s = token.trim_start_matches(|c: char| c.is_alphabetic());
    // Skip accidentals written as 'is'/'es' sequences (still alphabetic,
    // already consumed above).  Now skip octave marks ' and ,
    let s = s.trim_start_matches(|c: char| c == '\'' || c == ',');
    // What remains should be the duration digits and optional dot(s)
    s
}

// Convert the figures block into \figuremode tokens, attaching the duration
// of the corresponding bassline note to each figure group so that LilyPond
// can lay them out correctly even without a simultaneous voice to borrow from.
//
// Input figure tokens:
//   0          → <_>    (no figure on this note)
//   6          → <6>
//   #6         → <6+>   (raised sixth)
//   b6         → <6->   (lowered sixth)
//   #          → <_+>   (raised third, figured-bass convention)
//   2/#4/6     → <2 4+ 6>  (stacked, / as separator)
fn figures_to_ly(figures: &str, bassline: &str) -> String {
    // Collect bassline tokens that carry a duration (skip ties ~ and barlines |)
    let bass_tokens: Vec<&str> = bassline
        .split_whitespace()
        .filter(|t| !matches!(*t, "~" | "|"))
        .collect();

    figures
        .split_whitespace()
        .zip(bass_tokens.iter())
        .map(|(fig, bass)| {
            let dur = note_duration(bass);
            let group = parse_figure(fig);
            format!("{}{}", group, dur)
        })
        .collect::<Vec<_>>()
        .join(" ")
}

// Convert one figure token to a \figuremode group.
fn parse_figure(fig: &str) -> String {
    let fig = fig.trim();
    if fig == "0" {
        // No figure on this note; empty <_> renders nothing
        return "<_>".to_string();
    }

    // Handle stacked figures separated by '/'
    let parts: Vec<String> = fig.split('/').map(interval_to_ly).collect();
    format!("<{}>", parts.join(" "))
}

// Convert a single interval string (possibly with # or b prefix) to a
// LilyPond figure interval.  LilyPond uses + for sharp, - for flat on
// the interval number; a bare # means "raise the third" → _+.
fn interval_to_ly(s: &str) -> String {
    if s == "#" {
        // Bare accidental: raise the 3rd (common basso continuo shorthand)
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
