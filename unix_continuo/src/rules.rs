// SPDX-License-Identifier: MIT
// rules.rs --- elegant voice-leading validation for figured bass
// Copyright (c) 2026 Jakob Kastelic

use std::collections::HashSet;
use std::io::{self, BufRead};

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

/// A key signature represented as the set of pitch classes (0–11) in its scale.
#[derive(Debug, Clone, Default)]
struct KeySig {
    /// The seven pitch classes of the diatonic scale, in ascending order.
    scale_pcs: Vec<i32>,
}

/// A single figured-bass figure: a diatonic interval number plus a chromatic
/// accidental adjustment (0 = diatonic, +1 = raised, -1 = lowered, +2/-2 double).
#[derive(Debug, Clone)]
struct Figure {
    deg: i32, // 1-based diatonic degree, e.g. 3, 5, 6, 7
    acc: i32, // semitone adjustment on top of the diatonic pitch class
}

#[derive(Debug, Clone)]
struct Group {
    id: usize,
    passing: bool,
    bass: i32,
    figures: Vec<Figure>,
    inner: Vec<i32>,  // semitone pitches of realization (inner voices)
    melody: Vec<i32>, // semitone pitches of melody
}

/// Everything a rule needs: the sliding window plus the current key.
struct Context<'a> {
    window: &'a [Group],
    key: &'a KeySig,
}

type RuleFn = fn(&Context) -> Result<(), String>;

// ---------------------------------------------------------------------------
// Pitch helpers
// ---------------------------------------------------------------------------

fn to_semitone(p: &str) -> Option<i32> {
    let p = p.trim_end_matches(|c: char| c.is_ascii_digit() || c == '.');
    if p.is_empty() {
        return None;
    }

    let first = p.chars().next()?.to_ascii_lowercase();
    let rest = &p[1..];

    let (acc, oct_part) = if rest.starts_with("isis") {
        (2, &rest[4..])
    } else if rest.starts_with("eses") {
        (-2, &rest[4..])
    } else if rest.starts_with("is") {
        (1, &rest[2..])
    } else if rest.starts_with("es") || rest.starts_with("as") {
        (-1, &rest[2..])
    } else {
        (0, rest)
    };

    let octaves: i32 = oct_part
        .chars()
        .map(|c| match c {
            '\'' => 12,
            ',' => -12,
            _ => 0,
        })
        .sum();

    let base = match first {
        'c' => 0,
        'd' => 2,
        'e' => 4,
        'f' => 5,
        'g' => 7,
        'a' => 9,
        'b' => 11,
        _ => return None,
    };

    Some(base + acc + octaves + 48) // c' = 60
}

/// Pitch-class name for error messages.
fn pc_name(semitone: i32) -> &'static str {
    match semitone.rem_euclid(12) {
        0 => "c",
        1 => "cis",
        2 => "d",
        3 => "dis",
        4 => "e",
        5 => "f",
        6 => "fis",
        7 => "g",
        8 => "gis",
        9 => "a",
        10 => "ais",
        11 => "b",
        _ => "?",
    }
}

// ---------------------------------------------------------------------------
// Key signature parsing
// ---------------------------------------------------------------------------

const MAJOR_STEPS: [i32; 7] = [0, 2, 4, 5, 7, 9, 11];
const MINOR_STEPS: [i32; 7] = [0, 2, 3, 5, 7, 8, 10];

fn parse_key(token: &str) -> KeySig {
    let mut chars = token.chars();
    let first = match chars.next() {
        Some(c) => c,
        None => return KeySig::default(),
    };

    let is_minor = first.is_lowercase();
    let root_base: i32 = match first.to_ascii_uppercase() {
        'C' => 0,
        'D' => 2,
        'E' => 4,
        'F' => 5,
        'G' => 7,
        'A' => 9,
        'B' => 11,
        _ => return KeySig::default(),
    };

    let rest: String = chars.collect();
    let acc: i32 = match rest.as_str() {
        "#" | "is" => 1,
        "##" | "isis" => 2,
        "b" | "es" | "as" => -1,
        "bb" | "eses" => -2,
        _ => 0,
    };

    let root = (root_base + acc).rem_euclid(12);
    let steps = if is_minor { &MINOR_STEPS } else { &MAJOR_STEPS };
    let scale_pcs: Vec<i32> = steps.iter().map(|&s| (root + s) % 12).collect();

    KeySig { scale_pcs }
}

// ---------------------------------------------------------------------------
// Figure parsing
// ---------------------------------------------------------------------------

fn parse_one_figure(t: &str) -> Option<Figure> {
    let t = t.trim();
    if t.is_empty() {
        return None;
    }

    let (acc, rest) = if t.starts_with("##") {
        (2, &t[2..])
    } else if t.starts_with("bb") {
        (-2, &t[2..])
    } else if t.starts_with('#') {
        (1, &t[1..])
    } else if t.starts_with('b')
        && (t.len() == 1 || t.chars().nth(1).map_or(false, |c| c.is_ascii_digit()))
    {
        (-1, &t[1..])
    } else if t.starts_with('n') {
        (0, &t[1..])
    } else if t.starts_with("isis") {
        (2, &t[4..])
    } else if t.starts_with("eses") {
        (-2, &t[4..])
    } else if t.starts_with("is") {
        (1, &t[2..])
    } else if t.starts_with("es") || t.starts_with("as") {
        (-1, &t[2..])
    } else {
        (0, t)
    };

    // If rest is empty (e.g. just "#"), default degree to 3.
    let deg: i32 = if rest.is_empty() {
        3
    } else {
        rest.parse().ok()?
    };
    Some(Figure { deg, acc })
}

fn parse_figures(s: &str) -> Vec<Figure> {
    let s = s.trim();
    if s.is_empty() || s == "0" {
        return vec![
            Figure { deg: 3, acc: 0 },
            Figure { deg: 5, acc: 0 },
            Figure { deg: 8, acc: 0 },
        ];
    }

    let mut figs: Vec<Figure> = s.split(['/', ',']).filter_map(parse_one_figure).collect();

    if figs.is_empty() {
        return vec![
            Figure { deg: 3, acc: 0 },
            Figure { deg: 5, acc: 0 },
            Figure { deg: 8, acc: 0 },
        ];
    }

    // If only the 3rd is specified (e.g. "#" or "b3"), imply the rest of the triad.
    if figs.len() == 1 && figs[0].deg == 3 {
        figs.push(Figure { deg: 5, acc: 0 });
        figs.push(Figure { deg: 8, acc: 0 });
    }

    // If only the 4th is specified, imply a 4/5/8 chord.
    if figs.len() == 1 && figs[0].deg == 4 {
        figs.push(Figure { deg: 5, acc: 0 });
        figs.push(Figure { deg: 8, acc: 0 });
    }

    if figs.len() == 1 && figs[0].deg == 6 {
        figs.insert(0, Figure { deg: 3, acc: 0 });
    }

    figs.sort_by_key(|f| f.deg);
    figs.dedup_by_key(|f| f.deg);
    figs
}

fn figure_to_pc(fig: &Figure, bass_pc: i32, key: &KeySig) -> Option<i32> {
    let deg = ((fig.deg - 1) % 7) + 1;
    let bass_deg = key.scale_pcs.iter().position(|&pc| pc == bass_pc)?;
    let target_deg = (bass_deg + (deg as usize) - 1) % 7;
    let diatonic_pc = key.scale_pcs[target_deg];
    Some((diatonic_pc + fig.acc).rem_euclid(12))
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------

fn rule_no_parallels(ctx: &Context) -> Result<(), String> {
    let window = ctx.window;
    if window.len() < 2 {
        return Ok(());
    }
    let (prev, curr) = (&window[window.len() - 2], &window[window.len() - 1]);
    if curr.passing || prev.passing {
        return Ok(());
    }

    let flatten = |g: &Group| {
        let mut v = vec![g.bass];
        let mel = g.melody.first().copied();
        let b_pc = g.bass.rem_euclid(12);
        let m_pc = mel.map(|m| m.rem_euclid(12));

        for &n in &g.inner {
            let n_pc = n.rem_euclid(12);
            // Exclude notes that double the melody OR the bass pitch class
            if Some(n_pc) != m_pc && n_pc != b_pc {
                v.push(n);
            }
        }
        if let Some(m) = mel {
            v.push(m);
        }
        v
    };

    let p_voices = flatten(prev);
    let c_voices = flatten(curr);

    for i in 0..p_voices.len() {
        for j in (i + 1)..p_voices.len() {
            if j >= c_voices.len() || j >= p_voices.len() {
                continue;
            }

            let p_int = (p_voices[j] - p_voices[i]).abs() % 12;
            let c_int = (c_voices[j] - c_voices[i]).abs() % 12;

            if (p_int == 7 && c_int == 7) || (p_int == 0 && c_int == 0) {
                let motion_i = c_voices[i] - p_voices[i];
                let motion_j = c_voices[j] - p_voices[j];

                if motion_i.signum() == motion_j.signum() && motion_i != 0 {
                    return Err(format!(
                        "Parallel fifths/octaves between voice {} and {}",
                        i, j
                    ));
                }
            }
        }
    }
    Ok(())
}

fn rule_bass_leap(ctx: &Context) -> Result<(), String> {
    let window = ctx.window;
    if window.len() < 2 {
        return Ok(());
    }
    let (p, c) = (&window[window.len() - 2], &window[window.len() - 1]);
    if (c.bass - p.bass).abs() % 12 == 6 {
        return Err("Tritone leap in bass".into());
    }
    Ok(())
}

fn rule_check_realization(ctx: &Context) -> Result<(), String> {
    let g = &ctx.window[ctx.window.len() - 1];
    if g.passing {
        return Ok(());
    }
    let key = ctx.key;
    let bass_pc = g.bass.rem_euclid(12);

    let mut allowed: HashSet<i32> = HashSet::new();
    allowed.insert(bass_pc);

    for fig in &g.figures {
        match figure_to_pc(fig, bass_pc, key) {
            Some(pc) => {
                allowed.insert(pc);
            }
            None => {
                return Ok(());
            }
        }
    }

    // If the bass is held from the previous group (suspension resolution),
    // voices not re-specified by the figures may be carried over unchanged.
    // Admit any pitch class that was present in the previous group.
    if ctx.window.len() >= 2 {
        let prev = &ctx.window[ctx.window.len() - 2];
        if prev.bass.rem_euclid(12) == bass_pc {
            for &n in prev.inner.iter().chain(prev.melody.iter()) {
                allowed.insert(n.rem_euclid(12));
            }
            allowed.insert(prev.bass.rem_euclid(12));
        }
    }

    let fig_label = g
        .figures
        .iter()
        .map(|f| {
            let acc_str = match f.acc {
                2 => "##",
                1 => "#",
                -1 => "b",
                -2 => "bb",
                _ => "",
            };
            format!("{}{}", acc_str, f.deg)
        })
        .collect::<Vec<_>>()
        .join("/");

    for &note in g.inner.iter() {
        let pc = note.rem_euclid(12);
        if !allowed.contains(&pc) {
            return Err(format!(
                "Incorrect realization of figure {}: {}",
                fig_label,
                pc_name(note),
            ));
        }
    }

    Ok(())
}

fn rule_bass_in_key(ctx: &Context) -> Result<(), String> {
    let g = &ctx.window[ctx.window.len() - 1];
    if g.passing {
        return Ok(());
    }
    let bass_pc = g.bass.rem_euclid(12);
    if !ctx.key.scale_pcs.contains(&bass_pc) {
        return Err(format!(
            "Bass note {} is not diatonic to the key",
            pc_name(g.bass),
        ));
    }
    Ok(())
}

const RULES: &[RuleFn] = &[
    rule_no_parallels,
    rule_bass_leap,
    rule_check_realization,
    rule_bass_in_key,
];

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

fn parse_lesson_key(line: &str) -> Option<KeySig> {
    let tokens: Vec<&str> = line.split_whitespace().collect();
    tokens.get(2).map(|k| parse_key(k))
}

fn parse_group(line: &str) -> Option<Group> {
    if !line.starts_with("GROUP") {
        return None;
    }

    let mut id = 0usize;
    let passing = line.contains("passing");
    let mut bass = 0i32;
    let mut figures = parse_figures("0");
    let mut inner: Vec<i32> = Vec::new();
    let mut melody: Vec<i32> = Vec::new();

    let tokens: Vec<&str> = line.split_whitespace().collect();
    let mut i = 0;
    while i < tokens.len() {
        let t = tokens[i];
        if let Some(v) = t.strip_prefix("ID:") {
            id = v.parse().unwrap_or(0);
        } else if let Some(v) = t.strip_prefix("BASS_ACTUAL:") {
            bass = to_semitone(v).unwrap_or(0);
        } else if let Some(v) = t.strip_prefix("FIGURES:") {
            figures = parse_figures(v);
        } else if let Some(v) = t.strip_prefix("REALIZATION:") {
            inner = v.split('/').filter_map(to_semitone).collect();
            inner.sort();
        } else if let Some(v) = t.strip_prefix("MELODY:") {
            if !v.is_empty() {
                melody.push(to_semitone(v).unwrap_or(0));
            }
            while i + 1 < tokens.len() && !tokens[i + 1].contains(':') {
                i += 1;
                melody.push(to_semitone(tokens[i]).unwrap_or(0));
            }
        }
        i += 1;
    }

    Some(Group {
        id,
        passing,
        bass,
        figures,
        inner,
        melody,
    })
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

fn main() {
    let mut window: Vec<Group> = Vec::with_capacity(4);
    let mut key = KeySig::default();
    let stdin = io::stdin();

    for line in stdin.lock().lines().map_while(Result::ok) {
        println!("{}", line);

        if line.starts_with("LESSON") {
            if let Some(k) = parse_lesson_key(&line) {
                key = k;
                window.clear();
            }
            continue;
        }

        if let Some(g) = parse_group(&line) {
            let id = g.id;
            window.push(g);
            if window.len() > 4 {
                window.remove(0);
            }

            let ctx = Context {
                window: &window,
                key: &key,
            };

            match RULES.iter().try_for_each(|rule| rule(&ctx)) {
                Ok(_) => println!("RESULT {} \x1b[32mOK\x1b[0m", id),
                Err(e) => println!("RESULT {} \x1b[31mFAIL\x1b[0m {}", id, e),
            }
        }
    }
}
