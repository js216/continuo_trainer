// SPDX-License-Identifier: MIT
// render.rs --- render the LilyPond subset produced by g2ly
// Copyright (c) 2026 Jakob Kastelic

use std::io::{self, Read};

// ============================================================
// Tokeniser
// ============================================================

/// Lex a LilyPond source string into a flat token list.
/// Strips % line-comments.  Brace pairs, angle brackets, and pipe are
/// individual tokens.  Backslash-words become single tokens including the
/// backslash.  Quoted strings are kept as one token including the quotes.
/// Scheme expressions beginning with # are absorbed as a single opaque token.
fn tokenise(src: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut chars = src.chars().peekable();

    while let Some(&ch) = chars.peek() {
        match ch {
            '%' => {
                // line comment
                while chars.next().map(|c| c != '\n').unwrap_or(false) {}
            }
            c if c.is_whitespace() => {
                chars.next();
            }
            '{' | '}' | '|' => {
                out.push(ch.to_string());
                chars.next();
            }
            // angle bracket: could be < in a chord/figure group or standalone >
            '<' | '>' => {
                out.push(ch.to_string());
                chars.next();
            }
            '\\' => {
                chars.next();
                if chars.peek() == Some(&'\\') {
                    // double backslash (augmented-slash sentinel)
                    chars.next();
                    out.push("\\\\".to_string());
                } else {
                    let mut w = String::from("\\");
                    while chars
                        .peek()
                        .map(|&c| c.is_alphanumeric() || c == '-' || c == '_')
                        .unwrap_or(false)
                    {
                        w.push(chars.next().unwrap());
                    }
                    out.push(w);
                }
            }
            '"' => {
                chars.next();
                let mut s = String::from("\"");
                for c in chars.by_ref() {
                    if c == '"' {
                        break;
                    }
                    s.push(c);
                }
                s.push('"');
                out.push(s);
            }
            '#' => {
                // Scheme expression: absorb to end of balanced parens or EOL
                chars.next();
                let mut tok = String::from("#");
                let mut depth = 0i32;
                loop {
                    match chars.peek() {
                        None => break,
                        Some(&'(') => {
                            depth += 1;
                            tok.push('(');
                            chars.next();
                        }
                        Some(&')') => {
                            tok.push(')');
                            chars.next();
                            depth -= 1;
                            if depth <= 0 {
                                break;
                            }
                        }
                        Some(&'\n') if depth == 0 => break,
                        Some(&c) => {
                            tok.push(c);
                            chars.next();
                        }
                    }
                }
                out.push(tok);
            }
            _ => {
                let mut w = String::new();
                while let Some(&c) = chars.peek() {
                    if c.is_whitespace() || "{}|%\"<>".contains(c) {
                        break;
                    }
                    if c == '\\' {
                        break;
                    }
                    w.push(c);
                    chars.next();
                }
                if !w.is_empty() {
                    out.push(w);
                }
            }
        }
    }
    out
}

// ============================================================
// Pitch / duration helpers
// ============================================================

/// Split a note token into (pitch_name, octave_marks, duration_str, dotted, tied).
/// E.g. "cis''4." -> ("cis", "''", "4", true, false)
///      "b'2~"   -> ("b",   "'",  "2", false, true)
///      "s2."    -> ("s",   "",   "2", true,  false)  — spacer
fn parse_note_token(tok: &str) -> Option<(&str, &str, &str, bool, bool)> {
    let bytes = tok.as_bytes();
    // pitch name: alphabetic
    let mut i = 0;
    while i < bytes.len() && bytes[i].is_ascii_alphabetic() {
        i += 1;
    }
    if i == 0 {
        return None;
    }
    let name = &tok[..i];
    // octave marks
    let oct_start = i;
    while i < bytes.len() && (bytes[i] == b'\'' || bytes[i] == b',') {
        i += 1;
    }
    let oct = &tok[oct_start..i];
    // duration digit
    let dur_start = i;
    while i < bytes.len() && bytes[i].is_ascii_digit() {
        i += 1;
    }
    let dur = &tok[dur_start..i];
    // dot
    let dotted = i < bytes.len() && bytes[i] == b'.';
    if dotted {
        i += 1;
    }
    // tie
    let tied = i < bytes.len() && bytes[i] == b'~';
    Some((name, oct, dur, dotted, tied))
}

/// Duration string → length in slots (where 1 slot = 1 denominator beat).
/// Returns None if the duration string is empty (inherit previous, not supported here).
fn dur_to_slots(dur: &str, dotted: bool) -> Option<f64> {
    let base: f64 = match dur {
        "1" => 4.0,
        "2" => 2.0,
        "4" => 1.0,
        "8" => 0.5,
        "16" => 0.25,
        _ => return None,
    };
    Some(if dotted { base * 1.5 } else { base })
}

/// Diatonic step index: c=0, d=1, e=2, f=3, g=4, a=5, b=6.
fn diatonic_index(name: &str) -> Option<i32> {
    // name may have is/es suffix; only first char matters for step
    match name.chars().next()? {
        'c' => Some(0),
        'd' => Some(1),
        'e' => Some(2),
        'f' => Some(3),
        'g' => Some(4),
        'a' => Some(5),
        'b' => Some(6),
        _ => None,
    }
}

/// True if the pitch name implies an accidental.
fn accidental_semitones(name: &str) -> i32 {
    if name.ends_with("is") {
        1
    } else if name.ends_with("es") || name.ends_with("as") {
        -1
    } else {
        0
    }
}

/// Parse octave marks ('', ', ,,) into an integer offset from LilyPond
/// "default" octave.  LilyPond starts at octave 3 for bare note names;
/// each ' adds 1, each , subtracts 1.
fn octave_offset(marks: &str) -> i32 {
    marks
        .chars()
        .map(|c| match c {
            '\'' => 1,
            ',' => -1,
            _ => 0,
        })
        .sum()
}

/// Convert a LilyPond pitch to an absolute MIDI-style octave number.
/// LilyPond: bare 'c' is C3 (MIDI 48), 'c' is C4 (MIDI 60).
/// We use octave number where C4 = octave 4.
fn ly_octave(oct_marks: &str) -> i32 {
    3 + octave_offset(oct_marks)
}

/// Staff position in half-spaces above the bottom line of the staff,
/// for a given clef.
///
/// Treble clef: bottom line = E4  (diatonic 2, octave 4)
///   half_sp = (octave - 4)*14 + (diatonic - 2)
///   E4=0, F4=1, G4=2, A4=3, B4=4(middle space), C5=5, D5=6, E5=7, F5=8(top line)
///
/// Bass clef: bottom line = G2  (diatonic 4, octave 2)
///   half_sp = (octave - 2)*14 + (diatonic - 4)
///   G2=0, A2=1, B2=2, C3=3, D3=4(middle), E3=5, F3=6, G3=7, A3=8(top line)
fn staff_halfsp(name: &str, oct_marks: &str, clef: &Clef) -> i32 {
    let d = diatonic_index(name).unwrap_or(0);
    let oct = ly_octave(oct_marks);
    match clef {
        Clef::Treble => (oct - 4) * 14 + (d - 2),
        Clef::Bass => (oct - 2) * 14 + (d - 4),
    }
}

/// Y coordinate (in staff-spaces from top of this staff) for a given
/// half-space position.  Bottom line = halfsp 0, top line = halfsp 8.
/// y=4 is the bottom line, y=0 is the top line.
fn halfsp_to_y(halfsp: i32) -> f64 {
    4.0 - halfsp as f64 * 0.5
}

// ============================================================
// Data model
// ============================================================

#[derive(Debug, Clone, PartialEq)]
enum Clef {
    Treble,
    Bass,
}

/// A parsed music event at a specific time position.
#[derive(Debug, Clone)]
struct Event {
    /// Beat position within the staff (in slots from the beginning)
    slot: f64,
    /// Duration in slots
    dur: f64,
    /// The event kind
    kind: EventKind,
}

#[derive(Debug, Clone)]
enum EventKind {
    Note {
        /// All pitches in the notehead (len > 1 = chord)
        pitches: Vec<Pitch>,
        filled: bool, // quarter/eighth = filled; half = open; whole handled separately
        whole: bool,
        tied: bool, // this note ties into the next
    },
    Rest,
    Spacer,
    Figure {
        /// Individual figure symbols to draw below the staff
        symbols: Vec<FigSym>,
    },
    Barline,
}

#[derive(Debug, Clone)]
struct Pitch {
    #[allow(dead_code)]
    name: String, // "g", "cis", "fis", ...
    #[allow(dead_code)]
    oct: String, // octave marks
    halfsp: i32, // precomputed staff half-space position
    acc: i32,    // accidental semitones: -1 flat, 0 natural, 1 sharp
}

#[derive(Debug, Clone)]
enum FigSym {
    Blank,        // _
    Slash,        // passing-note solidus
    Num(u8),      // plain digit
    NumSharp(u8), // digit+
    NumFlat(u8),  // digit-
}

/// One fully-parsed staff.
#[derive(Debug)]
struct Staff {
    clef: Clef,
    sharps: i32, // key signature: positive = sharps, negative = flats
    timesig: (u32, u32),
    /// Primary voice events
    voice1: Vec<Event>,
    /// Second voice events (only for "real-bass" staff)
    voice2: Vec<Event>,
    /// Figured bass events (only for "bass" staff)
    figures: Vec<Event>,
    /// Name as declared in the LilyPond source
    #[allow(dead_code)]
    name: String,
}

// ============================================================
// Parser
// ============================================================

struct Parser {
    tokens: Vec<String>,
    pos: usize,
}

impl Parser {
    fn new(tokens: Vec<String>) -> Self {
        Parser { tokens, pos: 0 }
    }

    fn peek(&self) -> Option<&str> {
        self.tokens.get(self.pos).map(|s| s.as_str())
    }

    fn next(&mut self) -> Option<&str> {
        let t = self.tokens.get(self.pos).map(|s| s.as_str());
        self.pos += 1;
        t
    }

    fn expect(&mut self, s: &str) {
        let t = self.next().unwrap_or("");
        if t != s {
            eprintln!("warn: expected {:?} got {:?}", s, t);
        }
    }

    /// Skip tokens until we find the matching `}`.  Handles nesting.
    fn skip_braced_block(&mut self) {
        self.expect("{");
        let mut depth = 1;
        while let Some(t) = self.next() {
            match t {
                "{" => depth += 1,
                "}" => {
                    depth -= 1;
                    if depth == 0 {
                        break;
                    }
                }
                _ => {}
            }
        }
    }

    /// Consume tokens up to and including the closing `}`, returning them.
    fn collect_braced_block(&mut self) -> Vec<String> {
        self.expect("{");
        let mut depth = 1;
        let mut out = Vec::new();
        while let Some(t) = self.next() {
            match t {
                "{" => {
                    depth += 1;
                    out.push("{".into());
                }
                "}" => {
                    depth -= 1;
                    if depth == 0 {
                        break;
                    }
                    out.push("}".into());
                }
                s => {
                    out.push(s.to_string());
                }
            }
        }
        out
    }

    /// Parse the top-level document; return a list of staves.
    fn parse_document(&mut self) -> (String, Vec<Staff>) {
        let mut title = String::from("Untitled");
        let mut staves = Vec::new();

        while let Some(tok) = self.peek() {
            match tok {
                "\\version" => {
                    self.next();
                    self.next(); /* quoted string */
                }
                "\\header" => {
                    title = self.parse_header();
                }
                // Skip Scheme lines and \layout entirely
                t if t.starts_with('#') => {
                    self.next();
                }
                "\\layout" => {
                    self.next();
                    self.skip_braced_block();
                }
                // passingNoteSolidus = \markup ... — skip bare identifiers before =
                "\\score" => {
                    self.next();
                    staves = self.parse_score();
                }
                _ => {
                    self.next(); /* skip unknown */
                }
            }
        }
        (title, staves)
    }

    fn parse_header(&mut self) -> String {
        self.next(); // \header
        let block = self.collect_braced_block();
        // Look for title = "..."
        for i in 0..block.len().saturating_sub(2) {
            if block[i] == "title" && block[i + 1] == "=" {
                let s = &block[i + 2];
                if s.starts_with('"') {
                    return s[1..s.len() - 1].to_string();
                }
            }
        }
        "Untitled".to_string()
    }

    fn parse_score(&mut self) -> Vec<Staff> {
        let block = self.collect_braced_block();
        let mut inner = Parser::new(block);
        // expect <<  ...  >>
        inner.skip_double_angle_open();
        let mut staves = Vec::new();
        while !inner.at_double_close() && inner.peek().is_some() {
            if inner.peek() == Some("\\new") {
                if let Some(st) = inner.parse_new_context() {
                    staves.push(st);
                }
            } else {
                inner.next();
            }
        }
        staves
    }

    /// Consume `<<` represented as two consecutive `<` tokens.
    fn skip_double_angle_open(&mut self) {
        if self.peek() == Some("<") {
            self.next();
        }
        if self.peek() == Some("<") {
            self.next();
        }
    }

    fn skip_double_angle_close(&mut self) {
        if self.peek() == Some(">") {
            self.next();
        }
        if self.peek() == Some(">") {
            self.next();
        }
    }

    fn at_double_close(&self) -> bool {
        self.tokens.get(self.pos) == Some(&">".to_string())
            && self.tokens.get(self.pos + 1) == Some(&">".to_string())
    }

    /// Parse a `\new Staff = "name" { ... }` or `\new Voice ...` etc.
    /// Returns a Staff only for top-level \new Staff contexts.
    fn parse_new_context(&mut self) -> Option<Staff> {
        self.next(); // \new
        let kind = self.next().unwrap_or("").to_string();
        let name = if self.peek() == Some("=") {
            self.next(); // =
            let n = self.next().unwrap_or("\"\"").to_string();
            n.trim_matches('"').to_string()
        } else {
            String::new()
        };

        match kind.as_str() {
            "Staff" => {
                let block = self.collect_braced_block();
                Some(self.parse_staff_block(name, block))
            }
            _ => {
                // Not a top-level staff — skip
                if self.peek() == Some("{") {
                    self.skip_braced_block();
                } else if self.peek() == Some("<") {
                    self.skip_double_angle_open();
                    while !self.at_double_close() && self.peek().is_some() {
                        self.next();
                    }
                    self.skip_double_angle_close();
                }
                None
            }
        }
    }

    /// Parse the body of a Staff block (already de-braced).
    fn parse_staff_block(&self, name: String, block: Vec<String>) -> Staff {
        let mut p = Parser::new(block);
        let mut clef = Clef::Treble;
        let mut sharps = 0i32;
        let mut timesig = (4u32, 4u32);
        let mut voice1 = Vec::new();
        let mut voice2 = Vec::new();
        let mut figures = Vec::new();

        while let Some(tok) = p.peek() {
            match tok {
                "\\clef" => {
                    p.next();
                    clef = match p.next().unwrap_or("treble") {
                        "bass" => Clef::Bass,
                        _ => Clef::Treble,
                    };
                }
                "\\key" => {
                    p.next();
                    let pitch = p.next().unwrap_or("c").to_string();
                    p.next(); // \major
                    sharps = key_sharps(&pitch);
                }
                "\\time" => {
                    p.next();
                    let ts = p.next().unwrap_or("4/4");
                    let mut parts = ts.splitn(2, '/');
                    let num = parts.next().unwrap_or("4").parse().unwrap_or(4);
                    let den = parts.next().unwrap_or("4").parse().unwrap_or(4);
                    timesig = (num, den);
                }
                "\\new" => {
                    p.next();
                    let kind = p.next().unwrap_or("").to_string();
                    // optional = "name"
                    let voice_name = if p.peek() == Some("=") {
                        p.next();
                        p.next().unwrap_or("\"\"").trim_matches('"').to_string()
                    } else {
                        String::new()
                    };

                    match kind.as_str() {
                        "Voice" => {
                            let vblock = p.collect_braced_block();
                            let events = parse_voice_block(vblock, &clef, timesig);
                            // voiceTwo goes to voice2; everything else to voice1
                            if voice_name.contains("v2") || voice_name.contains("real-bass-notes") {
                                voice2 = events;
                            } else {
                                voice1 = events;
                            }
                        }
                        "FiguredBass" => {
                            // \new FiguredBass { \figuremode { ... } }
                            let fblock = p.collect_braced_block();
                            figures = parse_figured_bass_block(fblock, timesig);
                        }
                        _ => {
                            if p.peek() == Some("{") {
                                p.skip_braced_block();
                            }
                        }
                    }
                }
                "<" => {
                    // Opening << : one '<' already consumed by the match.
                    // Consume the second '<' if present, then let the outer
                    // loop handle the inner \new Voice tokens.
                    if p.peek() == Some("<") {
                        p.next();
                    }
                }
                ">" => {
                    if p.peek() == Some(">") {
                        p.next();
                    }
                }
                _ => {
                    // bare music tokens at staff level (single-voice staff)
                    let music = collect_music_tokens(&mut p);
                    voice1 = parse_music_tokens(music, &clef, timesig);
                }
            }
        }

        Staff {
            clef,
            sharps,
            timesig,
            voice1,
            voice2,
            figures,
            name,
        }
    }
}

// ============================================================
// Music token parsing
// ============================================================

/// Drain music tokens from a Parser, stopping at structural keywords.
fn collect_music_tokens(p: &mut Parser) -> Vec<String> {
    let mut out = Vec::new();
    loop {
        match p.peek() {
            None | Some("\\new") | Some("\\clef") | Some("\\key") | Some("\\time") | Some("}") => {
                break
            }
            // Stop at << (two consecutive '<') or >> (two consecutive '>')
            Some(">") if p.at_double_close() => break,
            Some(t) if t.starts_with('\\') && !is_music_keyword(t) => break,
            _ => {
                out.push(p.next().unwrap().to_string());
            }
        }
    }
    out
}

fn is_music_keyword(s: &str) -> bool {
    matches!(
        s,
        "\\voiceOne" | "\\voiceTwo" | "\\stemUp" | "\\stemDown" | "\\stemNeutral"
    )
}

/// Parse a \new Voice block (already de-braced) into events.
fn parse_voice_block(block: Vec<String>, clef: &Clef, timesig: (u32, u32)) -> Vec<Event> {
    let mut p = Parser::new(block);
    // skip directives like \voiceOne, \voiceTwo, \stemUp, etc.
    while p.peek().map(|t| is_music_keyword(t)).unwrap_or(false) {
        p.next();
    }
    let music = collect_music_tokens(&mut p);
    parse_music_tokens(music, clef, timesig)
}

/// Parse a flat list of music tokens into timed events.
fn parse_music_tokens(tokens: Vec<String>, clef: &Clef, timesig: (u32, u32)) -> Vec<Event> {
    let measure_slots = timesig.0 as f64 * 4.0 / timesig.1 as f64;
    let mut events = Vec::new();
    let mut slot = 0.0f64;
    let mut last_dur = "4".to_string();
    let mut last_dotted = false;
    let mut i = 0;

    while i < tokens.len() {
        let tok = &tokens[i];
        i += 1;

        if tok == "|" {
            events.push(Event {
                slot,
                dur: 0.0,
                kind: EventKind::Barline,
            });
            continue;
        }
        if tok == "~" {
            continue;
        } // ties are handled at the note level

        // Chord: < pitch+ >dur
        if tok == "<" {
            let mut pitches_raw = Vec::new();
            while i < tokens.len() && tokens[i] != ">" {
                pitches_raw.push(tokens[i].clone());
                i += 1;
            }
            if i < tokens.len() {
                i += 1;
            } // consume >

            // duration comes right after >
            let (dur_s, dotted, tied) = if i < tokens.len() {
                let d = &tokens[i];
                if d.starts_with(|c: char| c.is_ascii_digit()) || d.ends_with('~') {
                    let (ds, dot, tie) = split_dur_dot_tie(d);
                    i += 1;
                    last_dur = ds.to_string();
                    last_dotted = dot;
                    (last_dur.clone(), dot, tie)
                } else {
                    (last_dur.clone(), last_dotted, false)
                }
            } else {
                (last_dur.clone(), last_dotted, false)
            };

            let dur = dur_to_slots(&dur_s, dotted).unwrap_or(1.0);
            let pitches = pitches_raw.iter().map(|p| parse_pitch(p, clef)).collect();
            let whole = dur_s == "1";
            let filled = dur_s == "4" || dur_s == "8";
            events.push(Event {
                slot,
                dur,
                kind: EventKind::Note {
                    pitches,
                    filled,
                    whole,
                    tied,
                },
            });
            slot += dur;
            continue;
        }

        // Note, rest, spacer
        if let Some((name, oct, dur_raw, dotted, tied)) = parse_note_token(tok) {
            let dur_s = if dur_raw.is_empty() {
                last_dur.clone()
            } else {
                last_dur = dur_raw.to_string();
                last_dotted = dotted;
                dur_raw.to_string()
            };
            let dot = if dur_raw.is_empty() {
                last_dotted
            } else {
                dotted
            };
            // check for trailing ~
            let tied = tied
                || (i < tokens.len() && tokens[i] == "~" && {
                    i += 1;
                    true
                });

            let dur = dur_to_slots(&dur_s, dot).unwrap_or(1.0);
            let kind = match name {
                "s" => EventKind::Spacer,
                "r" => EventKind::Rest,
                _ => {
                    let pitch = parse_pitch_parts(name, oct, clef);
                    let whole = dur_s == "1";
                    let filled = dur_s == "4" || dur_s == "8";
                    EventKind::Note {
                        pitches: vec![pitch],
                        filled,
                        whole,
                        tied,
                    }
                }
            };
            events.push(Event { slot, dur, kind });
            slot += dur;
        }
    }

    // Insert implicit barlines wherever beat count crosses a bar boundary
    insert_implicit_barlines(events, measure_slots)
}

/// Check and insert barlines at measure boundaries if none are present.
fn insert_implicit_barlines(mut events: Vec<Event>, measure: f64) -> Vec<Event> {
    // Collect existing barline positions
    let has_explicit = events.iter().any(|e| matches!(e.kind, EventKind::Barline));
    if has_explicit {
        return events;
    }
    if measure <= 0.0 {
        return events;
    }

    let total = events.last().map(|e| e.slot + e.dur).unwrap_or(0.0);
    let mut barlines = Vec::new();
    let mut bar_slot = measure;
    while bar_slot < total - 0.001 {
        barlines.push(Event {
            slot: bar_slot,
            dur: 0.0,
            kind: EventKind::Barline,
        });
        bar_slot += measure;
    }
    events.extend(barlines);
    events.sort_by(|a, b| a.slot.partial_cmp(&b.slot).unwrap());
    events
}

fn split_dur_dot_tie(s: &str) -> (&str, bool, bool) {
    let tied = s.ends_with('~');
    let s = if tied { &s[..s.len() - 1] } else { s };
    let dotted = s.ends_with('.');
    let s = if dotted { &s[..s.len() - 1] } else { s };
    (s, dotted, tied)
}

fn parse_pitch(tok: &str, clef: &Clef) -> Pitch {
    if let Some((name, oct, _, _, _)) = parse_note_token(tok) {
        parse_pitch_parts(name, oct, clef)
    } else {
        Pitch {
            name: "c".into(),
            oct: String::new(),
            halfsp: 0,
            acc: 0,
        }
    }
}

fn parse_pitch_parts(name: &str, oct: &str, clef: &Clef) -> Pitch {
    let halfsp = staff_halfsp(name, oct, clef);
    let acc = accidental_semitones(name);
    Pitch {
        name: name.to_string(),
        oct: oct.to_string(),
        halfsp,
        acc,
    }
}

// ============================================================
// Figured bass parsing
// ============================================================

fn parse_figured_bass_block(block: Vec<String>, timesig: (u32, u32)) -> Vec<Event> {
    let mut p = Parser::new(block);
    // skip \figuremode
    if p.peek() == Some("\\figuremode") {
        p.next();
    }
    let inner = p.collect_braced_block();
    parse_figure_tokens(inner, timesig)
}

fn parse_figure_tokens(tokens: Vec<String>, _timesig: (u32, u32)) -> Vec<Event> {
    let mut events = Vec::new();
    let mut slot = 0.0f64;
    let mut last_dur = "2".to_string();
    let mut last_dotted = false;
    let mut i = 0;

    while i < tokens.len() {
        let tok = &tokens[i];
        i += 1;

        if tok == "|" {
            continue;
        }
        if tok != "<" {
            continue;
        }

        // collect interval tokens until >
        let mut syms = Vec::new();
        while i < tokens.len() && tokens[i] != ">" {
            syms.push(parse_fig_sym(&tokens[i]));
            i += 1;
        }
        if i < tokens.len() {
            i += 1;
        } // >

        // duration
        let (dur_s, dotted) =
            if i < tokens.len() && tokens[i].starts_with(|c: char| c.is_ascii_digit()) {
                let d = &tokens[i];
                i += 1;
                let dot = d.ends_with('.');
                let s = if dot { &d[..d.len() - 1] } else { d.as_str() };
                last_dur = s.to_string();
                last_dotted = dot;
                (last_dur.clone(), dot)
            } else {
                (last_dur.clone(), last_dotted)
            };

        let dur = dur_to_slots(&dur_s, dotted).unwrap_or(1.0);
        events.push(Event {
            slot,
            dur,
            kind: EventKind::Figure { symbols: syms },
        });
        slot += dur;
    }
    events
}

fn parse_fig_sym(tok: &str) -> FigSym {
    match tok {
        "_" => FigSym::Blank,
        "\\\\" => FigSym::Slash,
        s if s.ends_with('+') => {
            let n = s[..s.len() - 1].parse().unwrap_or(0);
            if n == 0 {
                FigSym::Blank
            } else {
                FigSym::NumSharp(n)
            }
        }
        s if s.ends_with('-') => {
            let n = s[..s.len() - 1].parse().unwrap_or(0);
            if n == 0 {
                FigSym::Blank
            } else {
                FigSym::NumFlat(n)
            }
        }
        s => {
            if let Ok(n) = s.parse::<u8>() {
                FigSym::Num(n)
            } else {
                FigSym::Blank
            }
        }
    }
}

// ============================================================
// Key signature helper
// ============================================================

/// Return the number of sharps (positive) or flats (negative) for a major key.
fn key_sharps(pitch: &str) -> i32 {
    // pitch is the tonic note name as LilyPond writes it (without octave)
    match pitch {
        "c" => 0,
        "g" => 1,
        "d" => 2,
        "a" => 3,
        "e" => 4,
        "b" => 5,
        "fis" => 6,
        "cis" => 7,
        "f" => -1,
        "bes" => -2,
        "ees" => -3,
        "aes" => -4,
        "des" => -5,
        "ges" => -6,
        "ces" => -7,
        _ => 0,
    }
}

// ============================================================
// Layout  — assign x (slot) and y (staff-space) to every event
// ============================================================

/// Layout constants.
const CLEF_WIDTH: f64 = 2.0; // slots reserved for clef
const KEYSIG_WIDTH: f64 = 1.0; // per accidental
const TIMESIG_WIDTH: f64 = 1.0; // slots for time signature
const LEFT_MARGIN: f64 = 0.5; // before clef
const RIGHT_MARGIN: f64 = 1.0;
const STAFF_GAP: f64 = 7.0; // staff-spaces between top lines of adjacent staves
const NOTE_X_OFFSET: f64 = 0.0; // nudge notehead right from beat grid x

// ============================================================
// Primitive emission
// ============================================================

fn emit_line(x1: f64, y1: f64, x2: f64, y2: f64, t: f64) {
    println!("LINE {:.4} {:.4} {:.4} {:.4} {:.4}", x1, y1, x2, y2, t);
}

fn emit_notehead(x: f64, y: f64, filled: bool, halfsp: i32) {
    println!(
        "NOTEHEAD {:.4} {:.4} {} {}",
        x,
        y,
        if filled { 1 } else { 0 },
        halfsp
    );
}

fn emit_whole(x: f64, y: f64, halfsp: i32) {
    println!("WHOLE {:.4} {:.4} {}", x, y, halfsp);
}

fn emit_stem(x: f64, y_base: f64, y_tip: f64) {
    println!("STEM {:.4} {:.4} {:.4}", x, y_base, y_tip);
}

fn emit_ledger(x: f64, y: f64, hw: f64) {
    println!("LEDGER {:.4} {:.4} {:.4}", x, y, hw);
}

fn emit_dot(x: f64, y: f64) {
    println!("DOT {:.4} {:.4}", x, y);
}

fn emit_tie(x1: f64, y: f64, x2: f64, up: bool) {
    println!(
        "TIE {:.4} {:.4} {:.4} {}",
        x1,
        y,
        x2,
        if up { 1 } else { 0 }
    );
}

fn emit_rest(x: f64, y: f64, dur: u8) {
    println!("REST {:.4} {:.4} {}", x, y, dur);
}

fn emit_barline(x: f64, y_top: f64, y_bot: f64) {
    println!("BARLINE {:.4} {:.4} {:.4}", x, y_top, y_bot);
}

fn emit_clef(x: f64, y: f64, kind: &Clef) {
    println!(
        "CLEF {:.4} {:.4} {}",
        x,
        y,
        match kind {
            Clef::Treble => "treble",
            Clef::Bass => "bass",
        }
    );
}

fn emit_timesig(x: f64, y: f64, num: u32, den: u32) {
    println!("TIMESIG {:.4} {:.4} {} {}", x, y, num, den);
}

fn emit_keysig(x: f64, y: f64, sharps: i32) {
    println!("KEYSIG {:.4} {:.4} {}", x, y, sharps);
}

fn emit_figure(x: f64, y: f64, sym: &FigSym) {
    let text = match sym {
        FigSym::Blank => return, // nothing to draw
        FigSym::Slash => "/".to_string(),
        FigSym::Num(n) => format!("{}", n),
        FigSym::NumSharp(n) => format!("{}+", n),
        FigSym::NumFlat(n) => format!("{}-", n),
    };
    println!("FIGURE {:.4} {:.4} {}", x, y, text);
}

fn emit_title(x: f64, y: f64, title: &str) {
    // Replace spaces with underscores so the line stays one token
    let safe: String = title
        .chars()
        .map(|c| if c == ' ' { '_' } else { c })
        .collect();
    println!("TITLE {:.4} {:.4} \"{}\"", x, y, safe);
}

// ============================================================
// Per-staff rendering
// ============================================================

/// Render a single staff, given its Y origin (top line, in staff-spaces
/// from the system top) and a function mapping slot → x coordinate.
fn render_staff(
    staff: &Staff,
    staff_y: f64, // y of this staff's top line
    slot_to_x: impl Fn(f64) -> f64,
    voice: &[Event], // which voice to render (voice1 or voice2)
    v2: bool,        // is this voice 2 (affects stem direction)
) {
    // Draw the five staff lines (once per staff, on the primary voice call)
    if !v2 {
        let x0 = slot_to_x(0.0) - LEFT_MARGIN * 0.5;
        let last_slot = voice
            .iter()
            .chain(staff.voice2.iter())
            .chain(staff.figures.iter())
            .map(|e| e.slot + e.dur)
            .fold(0.0f64, f64::max);
        let x1 = slot_to_x(last_slot) + RIGHT_MARGIN * 0.5;
        for line in 0..5u32 {
            let y = staff_y + line as f64;
            emit_line(x0, y, x1, y, 0.08);
        }
    }

    // Pending ties: (pitch_halfsp, x_start, y, up)
    let mut pending_ties: Vec<(i32, f64, f64, bool)> = Vec::new();

    for event in voice {
        let x = slot_to_x(event.slot) + NOTE_X_OFFSET;

        match &event.kind {
            EventKind::Barline => {
                // Barlines are drawn system-wide in render(); skip here.
            }

            EventKind::Rest => {
                let y = staff_y + 2.0; // middle line
                let dur_tag = slots_to_dur_tag(event.dur);
                emit_rest(x, y, dur_tag);
            }

            EventKind::Spacer => { /* nothing */ }

            EventKind::Figure { .. } => { /* handled separately */ }

            EventKind::Note {
                pitches,
                filled,
                whole,
                tied,
            } => {
                for pitch in pitches {
                    let hy = staff_y + halfsp_to_y(pitch.halfsp);
                    let above_mid = pitch.halfsp >= 4;
                    // Stem direction: voice 2 always stems up; voice 1 uses pitch
                    let stem_up = if v2 { true } else { !above_mid };

                    // Resolve any pending tie for this pitch
                    pending_ties.retain(|(hsp, tx, ty, tup)| {
                        if *hsp == pitch.halfsp {
                            emit_tie(*tx, *ty, x, *tup);
                            false
                        } else {
                            true
                        }
                    });

                    // Draw accidental if needed
                    if pitch.acc != 0 {
                        let glyph = if pitch.acc > 0 { "sharp" } else { "flat" };
                        println!("GLYPH {:.4} {:.4} 0.8 {}", x - 0.6, hy, glyph);
                    }

                    // Draw notehead
                    if *whole {
                        emit_whole(x, hy, pitch.halfsp);
                    } else {
                        emit_notehead(x, hy, *filled, pitch.halfsp);
                        // Draw stem (not for whole notes)
                        let stem_x = if stem_up { x + 0.28 } else { x - 0.28 };
                        let stem_len = 3.5; // in half-spaces = 1.75 staff-spaces
                        let y_tip = if stem_up {
                            hy - stem_len * 0.5
                        } else {
                            hy + stem_len * 0.5
                        };
                        emit_stem(stem_x, hy, y_tip);
                    }

                    // Augmentation dot
                    if event.dur / slots_base(event.dur) > 1.2 {
                        // dotted: slot ratio > 1
                        emit_dot(x + 0.55, hy - 0.25);
                    }

                    // Ledger lines
                    // Below staff: halfsp < 0  (each 2 half-spaces = one staff space)
                    let mut lsp = -2i32;
                    while lsp >= pitch.halfsp {
                        emit_ledger(x, staff_y + halfsp_to_y(lsp), 0.6);
                        lsp -= 2;
                    }
                    // Above staff: halfsp > 8
                    let mut usp = 10i32;
                    while usp <= pitch.halfsp {
                        emit_ledger(x, staff_y + halfsp_to_y(usp), 0.6);
                        usp += 2;
                    }

                    // Queue tie if this note is tied
                    if *tied {
                        pending_ties.push((pitch.halfsp, x, hy, !stem_up));
                    }
                }
            }
        }
    }
}

/// Duration in slots → duration augmentation factor (1.0 for plain, 1.5 for dotted).
fn slots_base(slots: f64) -> f64 {
    // Find the nearest power-of-two division of 4
    for &base in &[4.0f64, 2.0, 1.0, 0.5, 0.25] {
        if (slots / base - 1.0).abs() < 0.01 || (slots / base - 1.5).abs() < 0.01 {
            return base;
        }
    }
    slots
}

/// Slots to a duration tag (1=whole, 2=half, 4=quarter, 8=eighth).
fn slots_to_dur_tag(slots: f64) -> u8 {
    let base = slots_base(slots);
    match () {
        _ if (base - 4.0).abs() < 0.01 => 1,
        _ if (base - 2.0).abs() < 0.01 => 2,
        _ if (base - 0.5).abs() < 0.01 => 8,
        _ => 4,
    }
}

/// Render figured bass for staff 4 at a given staff_y.
fn render_figures(staff: &Staff, staff_y: f64, slot_to_x: impl Fn(f64) -> f64) {
    // Figures appear below the bottom line of the staff (y = staff_y + 4.0)
    // Stack multiple figures: first symbol at y_fig, subsequent ones above it
    let y_fig = staff_y + 5.2;
    for event in &staff.figures {
        let x = slot_to_x(event.slot);
        if let EventKind::Figure { symbols } = &event.kind {
            let non_blank: Vec<&FigSym> = symbols
                .iter()
                .filter(|s| !matches!(s, FigSym::Blank))
                .collect();
            // Draw bottom-to-top (lowest interval first in continuo convention)
            for (i, sym) in non_blank.iter().enumerate() {
                emit_figure(x, y_fig - i as f64 * 1.0, sym);
            }
        }
    }
}

// ============================================================
// System-level rendering
// ============================================================

fn render(title: &str, staves: &[Staff]) {
    if staves.is_empty() {
        return;
    }

    let timesig = staves[0].timesig;
    let total_slots = staves
        .iter()
        .flat_map(|s| s.voice1.iter().chain(s.voice2.iter()))
        .map(|e| e.slot + e.dur)
        .fold(0.0f64, f64::max);

    // Header slots consumed by clef + keysig + timesig (use widest across staves)
    let max_sharps = staves.iter().map(|s| s.sharps.abs()).max().unwrap_or(0);
    let hdr = LEFT_MARGIN + CLEF_WIDTH + max_sharps as f64 * KEYSIG_WIDTH + TIMESIG_WIDTH;

    // x coordinate of a given slot
    let slot_to_x = |slot: f64| hdr + slot;

    let total_x = hdr + total_slots + RIGHT_MARGIN;
    let total_y = STAFF_GAP * (staves.len() as f64 - 1.0) + 4.0;

    // Score header line
    println!(
        "SCORE staves={} slots={:.4} timesig={}/{} title=\"{}\"",
        staves.len(),
        total_slots,
        timesig.0,
        timesig.1,
        title.replace(' ', "_")
    );
    println!("EXTENT {:.4} {:.4}", total_x, total_y);

    // Title above system
    emit_title(total_x * 0.5, -1.5, title);

    // Draw each staff
    for (si, staff) in staves.iter().enumerate() {
        let staff_y = si as f64 * STAFF_GAP;

        // Clef
        emit_clef(LEFT_MARGIN, staff_y, &staff.clef);

        // Key signature
        let keyx = LEFT_MARGIN + CLEF_WIDTH;
        emit_keysig(keyx, staff_y, staff.sharps);

        // Time signature
        let tsx = keyx + staff.sharps.abs() as f64 * KEYSIG_WIDTH;
        emit_timesig(tsx, staff_y, timesig.0, timesig.1);

        // Barlines that span just this staff (the global ones come later)
        // Voice 1
        render_staff(staff, staff_y, slot_to_x, &staff.voice1, false);
        // Voice 2 if present
        if !staff.voice2.is_empty() {
            render_staff(staff, staff_y, slot_to_x, &staff.voice2, true);
        }
        // Figured bass
        if !staff.figures.is_empty() {
            render_figures(staff, staff_y, slot_to_x);
        }
    }

    // Full-height barlines spanning all staves
    let y_top = 0.0;
    let y_bot = (staves.len() as f64 - 1.0) * STAFF_GAP + 4.0;
    // Collect barline x positions from voice1 of the first staff
    let barline_slots: Vec<f64> = staves[0]
        .voice1
        .iter()
        .filter(|e| matches!(e.kind, EventKind::Barline))
        .map(|e| e.slot)
        .collect();
    for bslot in barline_slots {
        emit_barline(slot_to_x(bslot), y_top, y_bot);
    }
    // Final barline
    emit_barline(slot_to_x(total_slots), y_top, y_bot);
}

// ============================================================
// Main
// ============================================================

fn main() {
    let mut src = String::new();
    io::stdin().read_to_string(&mut src).expect("read stdin");

    let tokens = tokenise(&src);
    let mut parser = Parser::new(tokens);
    let (title, staves) = parser.parse_document();

    render(&title, &staves);
}
