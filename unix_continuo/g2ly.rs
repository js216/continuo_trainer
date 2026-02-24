/* g2ly.rs --- convert grouper output into a four-staff LilyPond score
 *
 * Input: grouper output via stdin (LESSON header + GROUP lines)
 * Output: LilyPond document on stdout
 *
 * Usage:
 *   build/grouper < lessons/1.txt | build/g2ly > lessons/1.ly
 *   build/g2ly < groups/1.txt > lessons/1.ly
 *
 * Four staves, top to bottom:
 *   1. Melody      (treble): MELODY verbatim; "-"/empty -> spacer rest.
 *   2. Realization (treble): REALIZATION notes at/above middle C (MIDI >= 60).
 *   3. Realization (bass)  : BASS_ACTUAL (voice 1) + REALIZATION notes
 *                            below middle C (voice 2); all use BASS duration.
 *   4. Bass        (bass)  : BASS note (written/expected), one per group.
 *
 * Ties across passing groups:
 *   When group N+1 is passing, every REALIZATION note that appears at the
 *   exact same pitch in both groups is tied: group N's token gets "~".
 *   BASS_ACTUAL and BASS are never tied (passing bass is a new pitch).
 */

use std::io::{self, BufRead};

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

#[derive(Debug)]
#[allow(dead_code)]
struct Group {
    id:          usize,
    passing:     bool,
    bass:        String,      // written bass note + duration, e.g. "g2"
    figures:     String,
    melody:      String,      // "-" or multi-token, e.g. "b'4 cis''4"
    bass_actual: String,      // played bass pitch (no duration), e.g. "g"
    time_ms:     u64,
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

    let mut id          = 0usize;
    let mut passing     = false;
    let mut bass        = String::new();
    let mut figures     = String::new();
    let mut melody_toks: Vec<&str> = Vec::new();
    let mut bass_actual = String::new();
    let mut time_ms     = 0u64;
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
        id, passing, bass, figures,
        melody: melody_toks.join(" "),
        bass_actual, time_ms, realization,
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
    let _num  = parts.next()?;
    let key   = parts.next()?.to_string();
    let time  = parts.next()?.to_string();
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
    while i < b.len() && b[i].is_ascii_alphabetic() { i += 1; }
    while i < b.len() && (b[i] == b'\'' || b[i] == b',') { i += 1; }
    &token[..i]
}

/// Extract duration suffix: "g2" -> "2", "fis4." -> "4.".
fn duration_of(token: &str) -> &str {
    &token[pitch_only(token).len()..]
}

/// LilyPond pitch name -> MIDI note number (c' = middle C = 60).
fn pitch_to_midi(pitch: &str) -> Option<i32> {
    let end  = pitch.find(|c: char| c == '\'' || c == ',').unwrap_or(pitch.len());
    let name = &pitch[..end];
    let octs = &pitch[end..];
    let pc: i32 = match name {
        "c"  | "bis"  => 0,  "cis" | "des" => 1,
        "d"           => 2,  "dis" | "ees" => 3,
        "e"  | "fes"  => 4,  "f"   | "eis" => 5,
        "fis"| "ges"  => 6,  "g"           => 7,
        "gis"| "aes"  => 8,  "a"           => 9,
        "ais"| "bes"  => 10, "b"   | "ces" => 11,
        _ => return None,
    };
    let octave: i32 = 3 + octs.chars().map(|c| match c {
        '\'' => 1, ',' => -1, _ => 0,
    }).sum::<i32>();
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
        && notes.iter().all(|p| next_realization.iter().any(|q| q == *p))
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
    if s == "#" { return "_+".to_string(); }
    if s == "b" { return "_-".to_string(); }
    let (acc, num) = if s.starts_with('#') {
        ("+", &s[1..])
    } else if s.starts_with('b') {
        ("-", &s[1..])
    } else {
        ("", s)
    };
    if acc.is_empty() { num.to_string() } else { format!("{}{}", num, acc) }
}

/// Convert one figure token (e.g. "0", "#6", "2/#4/6") to a \figuremode group.
fn parse_figure(fig: &str) -> String {
    let fig = fig.trim();
    if fig == "0" { return "<_>".to_string(); }
    let parts: Vec<String> = fig.split('/').map(interval_to_ly).collect();
    format!("<{}>", parts.join(" "))
}

/// Build the \figuremode token stream for staff 4.
/// Each group contributes one token: passing groups get <_\\>, others
/// get parse_figure(g.figures).  The duration comes from g.bass.
fn build_figures(groups: &[Group]) -> String {
    groups.iter().map(|g| {
        let dur   = duration_of(&g.bass);
        let group = if g.passing {
            r"<_\\>".to_string()   // passing-note solidus (augmented-slash in figuremode)
        } else {
            parse_figure(&g.figures)
        };
        format!("{}{}", group, dur)
    }).collect::<Vec<_>>().join(" ")
}

// ---------------------------------------------------------------------------
// Voice builders
// ---------------------------------------------------------------------------

/// Staff 1: melody, verbatim pass-through. "-"/empty -> spacer rest.
fn build_melody(groups: &[Group]) -> String {
    groups.iter().map(|g| {
        if g.melody == "-" || g.melody.is_empty() {
            format!("s{}", duration_of(&g.bass))
        } else {
            g.melody.clone()
        }
    }).collect::<Vec<_>>().join(" ")
}

/// Staff 2: high realization notes (MIDI >= 60), with ties into passing groups.
fn build_real_treble(groups: &[Group]) -> String {
    groups.iter().enumerate().map(|(i, g)| {
        let dur = duration_of(&g.bass);
        let notes: Vec<&str> = g.realization.iter()
            .map(|s| s.as_str()).filter(|&p| is_treble(p)).collect();
        let tie = groups.get(i + 1).map_or(false, |next| {
            next.passing && all_continue(&notes, &next.realization)
        });
        realization_token(&notes, dur, tie)
    }).collect::<Vec<_>>().join(" ")
}

/// Staff 3, voice 1: BASS_ACTUAL note, one per group, no ties.
fn build_bass_actual(groups: &[Group]) -> String {
    groups.iter().map(|g| {
        format!("{}{}", g.bass_actual, duration_of(&g.bass))
    }).collect::<Vec<_>>().join(" ")
}

/// Staff 3, voice 2: low realization notes (MIDI < 60), with ties into passing groups.
fn build_real_bass(groups: &[Group]) -> String {
    groups.iter().enumerate().map(|(i, g)| {
        let dur = duration_of(&g.bass);
        let notes: Vec<&str> = g.realization.iter()
            .map(|s| s.as_str()).filter(|&p| !is_treble(p)).collect();
        let tie = groups.get(i + 1).map_or(false, |next| {
            next.passing && all_continue(&notes, &next.realization)
        });
        realization_token(&notes, dur, tie)
    }).collect::<Vec<_>>().join(" ")
}

/// Staff 4: BASS (written/expected) note, one per group, no ties.
fn build_bass_expected(groups: &[Group]) -> String {
    groups.iter().map(|g| {
        format!("{}{}", pitch_only(&g.bass), duration_of(&g.bass))
    }).collect::<Vec<_>>().join(" ")
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() {
    let stdin = io::stdin();
    let mut key   = String::from("c");
    let mut time  = String::from("4/4");
    let mut title = String::from("Untitled");
    let mut groups: Vec<Group> = Vec::new();

    for raw in stdin.lock().lines() {
        let line = raw.expect("read error");
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        if line.starts_with("LESSON ") {
            if let Some((k, t, ttl)) = parse_lesson(line) {
                key   = k.to_lowercase();
                time  = t;
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

    let melody        = build_melody(&groups);
    let real_treble   = build_real_treble(&groups);
    let bass_actual   = build_bass_actual(&groups);
    let real_bass     = build_real_bass(&groups);
    let bass_expected = build_bass_expected(&groups);
    let figures       = build_figures(&groups);

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
        title         = title,
        key           = key,
        time          = time,
        melody        = melody,
        real_treble   = real_treble,
        bass_actual   = bass_actual,
        real_bass     = real_bass,
        bass_expected = bass_expected,
        figures       = figures,
    );

    print!("{}", doc);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pitch_only() {
        assert_eq!(pitch_only("g2"),     "g");
        assert_eq!(pitch_only("fis2"),   "fis");
        assert_eq!(pitch_only("cis''4"), "cis''");
        assert_eq!(pitch_only("b'4"),    "b'");
        assert_eq!(pitch_only("g"),      "g");
    }

    #[test]
    fn test_duration_of() {
        assert_eq!(duration_of("g2"),    "2");
        assert_eq!(duration_of("fis2"),  "2");
        assert_eq!(duration_of("fis4."), "4.");
        assert_eq!(duration_of("b'4"),   "4");
        assert_eq!(duration_of("e2"),    "2");
    }

    #[test]
    fn test_pitch_to_midi() {
        assert_eq!(pitch_to_midi("c'"),  Some(60));
        assert_eq!(pitch_to_midi("g"),   Some(55));
        assert_eq!(pitch_to_midi("fis"), Some(54));
        assert_eq!(pitch_to_midi("b'"),  Some(71));
        assert_eq!(pitch_to_midi("d''"), Some(74));
        assert_eq!(pitch_to_midi("g'"),  Some(67));
    }

    #[test]
    fn test_is_treble() {
        assert!( is_treble("c'"));   // 60
        assert!( is_treble("g'"));   // 67
        assert!( is_treble("b'"));   // 71
        assert!(!is_treble("b"));    // 59
        assert!(!is_treble("g"));    // 55
        assert!(!is_treble("fis"));  // 54
    }

    #[test]
    fn test_parse_lesson() {
        let (k, t, ttl) = parse_lesson("LESSON 1 G 3/2 Purcell bars 48-52").unwrap();
        assert_eq!(k, "G");
        assert_eq!(t, "3/2");
        assert_eq!(ttl, "Purcell bars 48-52");
    }

    #[test]
    fn test_parse_group_normal() {
        let g = parse_group(
            "GROUP ID:0 BASS:g2 FIGURES:0 MELODY:g'2. BASS_ACTUAL:g TIME:4531 REALIZATION:b'/g'/d'"
        ).unwrap();
        assert_eq!(g.id, 0);
        assert!(!g.passing);
        assert_eq!(g.bass,        "g2");
        assert_eq!(g.melody,      "g'2.");
        assert_eq!(g.bass_actual, "g");
        assert_eq!(g.time_ms,     4531);
        assert_eq!(g.realization, vec!["b'", "g'", "d'"]);
    }

    #[test]
    fn test_parse_group_passing() {
        let g = parse_group(
            "GROUP ID:1 passing BASS:fis2 FIGURES:0 MELODY:b'4 BASS_ACTUAL:fis TIME:5475 REALIZATION:b'/g'/d'/g"
        ).unwrap();
        assert_eq!(g.id, 1);
        assert!(g.passing);
        assert_eq!(g.bass,        "fis2");
        assert_eq!(g.bass_actual, "fis");
        assert_eq!(g.realization, vec!["b'", "g'", "d'", "g"]);
    }

    #[test]
    fn test_parse_group_multi_melody() {
        let g = parse_group(
            "GROUP ID:2 BASS:e2 FIGURES:#6 MELODY:b'4 cis''4 BASS_ACTUAL:e TIME:9774 REALIZATION:g'/cis'"
        ).unwrap();
        assert_eq!(g.melody,      "b'4 cis''4");
        assert_eq!(g.realization, vec!["g'", "cis'"]);
    }

    fn sample_groups() -> Vec<Group> {
        vec![
            Group {
                id: 0, passing: false, time_ms: 0, figures: "0".into(),
                bass: "g2".into(), melody: "g'2.".into(), bass_actual: "g".into(),
                realization: vec!["b'".into(), "g'".into(), "d'".into()],
            },
            Group {
                id: 1, passing: true, time_ms: 0, figures: "0".into(),
                bass: "fis2".into(), melody: "b'4".into(), bass_actual: "fis".into(),
                realization: vec!["b'".into(), "g'".into(), "d'".into(), "g".into()],
            },
            Group {
                id: 2, passing: false, time_ms: 0, figures: "#6".into(),
                bass: "e2".into(), melody: "b'4 cis''4".into(), bass_actual: "e".into(),
                realization: vec!["g'".into(), "cis'".into()],
            },
        ]
    }

    #[test]
    fn test_build_bass_expected() {
        let groups = sample_groups();
        let v = build_bass_expected(&groups);
        // Should be the pitch-only parts of BASS with their durations
        assert_eq!(v, "g2 fis2 e2");
    }

    #[test]
    fn test_build_bass_actual() {
        let groups = sample_groups();
        let v = build_bass_actual(&groups);
        // BASS_ACTUAL strings plus duration inherited from BASS field
        assert_eq!(v, "g2 fis2 e2");
    }

    #[test]
    fn test_realization_split() {
        // Group with b'(71), g'(67), d'(62) -> treble; g(55) -> bass
        let groups = vec![Group {
            id: 0, passing: false, time_ms: 0, figures: "0".into(),
            bass: "g2".into(), melody: "-".into(), bass_actual: "g".into(),
            realization: vec!["b'".into(), "g'".into(), "d'".into(), "g".into()],
        }];
        let treble = build_real_treble(&groups);
        let bass   = build_real_bass(&groups);
        assert!(treble.contains("b'"), "treble: {}", treble);
        assert!(treble.contains("g'"), "treble: {}", treble);
        assert!(treble.contains("d'"), "treble: {}", treble);
        assert!(!treble.contains(" g"), "g should not be in treble: {}", treble);
        assert!(bass.contains("g"),    "bass: {}", bass);
    }

    #[test]
    fn test_ties_across_passing_group() {
        // Groups 0 and 1 from sample: group 1 is passing, shares b'/g'/d' with group 0.
        // -> group 0 treble chord should get "~"; group 0 has no low-realization so no bass tie.
        let groups = sample_groups();
        let treble = build_real_treble(&groups);

        let first = treble.split_whitespace().next().unwrap_or("");
        assert!(first.ends_with('~'),
            "group 0 treble chord should be tied; first token={:?}, full={}", first, treble);

        // Group 1 is the last-but-one and group 2 is not passing -> group 1 not tied
        let tokens: Vec<&str> = treble.split_whitespace().collect();
        assert!(!tokens[1].ends_with('~'),
            "group 1 should not be tied; token={:?}", tokens[1]);
    }

    #[test]
    fn test_no_tie_without_passing() {
        let groups = vec![
            Group {
                id: 0, passing: false, time_ms: 0, figures: "0".into(),
                bass: "g2".into(), melody: "g'2.".into(), bass_actual: "g".into(),
                realization: vec!["b'".into(), "g'".into()],
            },
            Group {
                id: 1, passing: false, time_ms: 0, figures: "6".into(),
                bass: "e2".into(), melody: "e'2.".into(), bass_actual: "e".into(),
                realization: vec!["b'".into(), "g'".into()],
            },
        ];
        let treble = build_real_treble(&groups);
        assert!(!treble.contains('~'),
            "no tie between non-passing groups: {}", treble);
    }

    #[test]
    fn test_melody_spacer() {
        let groups = vec![Group {
            id: 0, passing: false, time_ms: 0, figures: "0".into(),
            bass: "g2".into(), melody: "-".into(), bass_actual: "g".into(),
            realization: vec![],
        }];
        assert_eq!(build_melody(&groups), "s2");
    }

    #[test]
    fn test_interval_to_ly() {
        assert_eq!(interval_to_ly("6"),   "6");
        assert_eq!(interval_to_ly("#6"),  "6+");
        assert_eq!(interval_to_ly("b6"),  "6-");
        assert_eq!(interval_to_ly("#"),   "_+");
        assert_eq!(interval_to_ly("b"),   "_-");
        assert_eq!(interval_to_ly("4"),   "4");
    }

    #[test]
    fn test_parse_figure() {
        assert_eq!(parse_figure("0"),      "<_>");
        assert_eq!(parse_figure("6"),      "<6>");
        assert_eq!(parse_figure("#6"),     "<6+>");
        assert_eq!(parse_figure("#"),      "<_+>");
        assert_eq!(parse_figure("2/#4/6"), "<2 4+ 6>");
    }

    #[test]
    fn test_build_figures() {
        let groups = sample_groups();
        // group 0: figures="0", not passing  -> <_>2
        // group 1: passing                   -> solidus token
        // group 2: figures="#6", not passing -> <6+>2
        let f = build_figures(&groups);
        let tokens: Vec<&str> = f.split_whitespace().collect();
        assert_eq!(tokens[0], "<_>2",  "group 0: {}", tokens[0]);
        assert!(tokens[1].starts_with("<_"), "group 1 solidus: {}", tokens[1]);
        assert_eq!(tokens[2], "<6+>2", "group 2: {}", tokens[2]);
    }
}
