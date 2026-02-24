// SPDX-License-Identifier: MIT
// draw.rs --- translate render.rs output into Nuklear/Xlib drawing primitives
// Copyright (c) 2026 Jakob Kastelic

use std::io::{self, BufRead};

// ============================================================
// Scaling
// ============================================================

/// Pixels per slot (horizontal beat unit).
const PX_PER_SLOT: f64 = 60.0;

/// Pixels per staff-space (vertical unit; one staff = 4 staff-spaces).
const PX_PER_SP: f64 = 14.0;

/// Left margin in pixels before the first score element.
const MARGIN_X: f64 = 20.0;

/// Top margin in pixels above the top staff line.
const MARGIN_Y: f64 = 60.0;

// ============================================================
// Colour palette  (r g b)
// ============================================================
const BLACK: (u8, u8, u8) = (20, 20, 20);
const GREY: (u8, u8, u8) = (120, 120, 120);

// ============================================================
// Coordinate conversion
// ============================================================

fn px(slot: f64) -> i32 {
    (MARGIN_X + slot * PX_PER_SLOT).round() as i32
}
fn py(sp: f64) -> i32 {
    (MARGIN_Y + sp * PX_PER_SP).round() as i32
}

// Thickness in render.rs is in staff-spaces; convert to integer pixels (min 1).
fn pt(t: f64) -> i32 {
    (t * PX_PER_SP).round().max(1.0) as i32
}

// ============================================================
// Primitive emitters
// ============================================================

fn stroke_line(x0: i32, y0: i32, x1: i32, y1: i32, t: i32, c: (u8, u8, u8)) {
    println!(
        "stroke_line {} {} {} {} {} {} {} {}",
        x0, y0, x1, y1, t, c.0, c.1, c.2
    );
}

fn fill_circle(x: i32, y: i32, w: i32, h: i32, c: (u8, u8, u8)) {
    // x,y is top-left of bounding box (Nuklear convention)
    println!(
        "fill_circle {} {} {} {} {} {} {}",
        x, y, w, h, c.0, c.1, c.2
    );
}

fn stroke_circle(x: i32, y: i32, w: i32, h: i32, t: i32, c: (u8, u8, u8)) {
    println!(
        "stroke_circle {} {} {} {} {} {} {} {}",
        x, y, w, h, t, c.0, c.1, c.2
    );
}

// a_min and a_max are in degrees (0 = 3 o'clock, CCW positive like Nuklear/X11)
#[allow(dead_code)]
fn stroke_arc(cx: i32, cy: i32, r: i32, a_min: i32, a_max: i32, t: i32, c: (u8, u8, u8)) {
    println!(
        "stroke_arc {} {} {} {} {} {} {} {} {}",
        cx, cy, r, a_min, a_max, t, c.0, c.1, c.2
    );
}

fn fill_arc(cx: i32, cy: i32, r: i32, a_min: i32, a_max: i32, c: (u8, u8, u8)) {
    println!(
        "fill_arc {} {} {} {} {} {} {} {}",
        cx, cy, r, a_min, a_max, c.0, c.1, c.2
    );
}

#[allow(dead_code)]
fn fill_triangle(x0: i32, y0: i32, x1: i32, y1: i32, x2: i32, y2: i32, c: (u8, u8, u8)) {
    println!(
        "fill_triangle {} {} {} {} {} {} {} {} {}",
        x0, y0, x1, y1, x2, y2, c.0, c.1, c.2
    );
}

// Cubic bezier: p0 -> ctrl0 -> ctrl1 -> p1
fn stroke_curve(
    x0: i32,
    y0: i32,
    cx0: i32,
    cy0: i32,
    cx1: i32,
    cy1: i32,
    x1: i32,
    y1: i32,
    t: i32,
    c: (u8, u8, u8),
) {
    println!(
        "stroke_curve {} {} {} {} {} {} {} {} {} {} {} {}",
        x0, y0, cx0, cy0, cx1, cy1, x1, y1, t, c.0, c.1, c.2
    );
}

fn fill_rect(x: i32, y: i32, w: i32, h: i32, c: (u8, u8, u8)) {
    println!("fill_rect {} {} {} {} {} {} {}", x, y, w, h, c.0, c.1, c.2);
}

fn draw_text(x: i32, y: i32, text: &str, c: (u8, u8, u8)) {
    println!("draw_text {} {} \"{}\" {} {} {}", x, y, text, c.0, c.1, c.2);
}

// ============================================================
// Musical shape helpers
// ============================================================

/// Notehead ellipse dimensions in pixels.
/// Standard music engraving: heads are ~1.4 staff-spaces wide, 1.0 tall.
const HEAD_W: f64 = 1.4 * PX_PER_SP;
const HEAD_H: f64 = 1.0 * PX_PER_SP;

/// Whole note is wider and more circular.
const WHOLE_W: f64 = 1.7 * PX_PER_SP;
const WHOLE_H: f64 = 1.1 * PX_PER_SP;

/// Draw a filled notehead (quarter/eighth) centred at (cx, cy).
fn draw_notehead_filled(cx: i32, cy: i32) {
    let w = HEAD_W.round() as i32;
    let h = HEAD_H.round() as i32;
    fill_circle(cx - w / 2, cy - h / 2, w, h, BLACK);
}

/// Draw an open notehead (half note) centred at (cx, cy).
/// Rendered as a stroke_circle with a thin inner fill to leave a white centre.
fn draw_notehead_open(cx: i32, cy: i32) {
    let w = HEAD_W.round() as i32;
    let h = HEAD_H.round() as i32;
    let t = pt(0.12); // beam thickness ~12 % of a staff-space
    stroke_circle(cx - w / 2, cy - h / 2, w, h, t, BLACK);
    // white fill inside so ledger lines don't show through
    let iw = (HEAD_W * 0.6).round() as i32;
    let ih = (HEAD_H * 0.55).round() as i32;
    fill_circle(cx - iw / 2, cy - ih / 2, iw, ih, (255, 255, 255));
}

/// Draw a whole note centred at (cx, cy) — wider oval, open centre with hole.
fn draw_notehead_whole(cx: i32, cy: i32) {
    let w = WHOLE_W.round() as i32;
    let h = WHOLE_H.round() as i32;
    let t = pt(0.14);
    stroke_circle(cx - w / 2, cy - h / 2, w, h, t, BLACK);
    // white interior
    let iw = (WHOLE_W * 0.55).round() as i32;
    let ih = (WHOLE_H * 0.50).round() as i32;
    fill_circle(cx - iw / 2, cy - ih / 2, iw, ih, (255, 255, 255));
}

/// Whole-rest: a filled rectangle hanging from a staff line.
///   Conventional: a filled black box below the second-from-top line (y_top + sp).
fn draw_rest_whole(cx: i32, y_staff_top: i32) {
    // Hang from the line at staff position 3 (second from top, halfsp 6).
    // In staff-spaces: that line is at y_staff_top + (4 - 6*0.5) = y_staff_top + 1.
    let line_y = y_staff_top + py_sp(1.0);
    let rw = pt(1.6);
    let rh = pt(0.5);
    fill_rect(cx - rw / 2, line_y, rw, rh, BLACK);
}

/// Half-rest: a filled rectangle sitting on a staff line (one space lower than whole).
fn draw_rest_half(cx: i32, y_staff_top: i32) {
    // Sit on the middle line (halfsp 4, staff-space 2).
    let line_y = y_staff_top + py_sp(2.0);
    let rw = pt(1.6);
    let rh = pt(0.5);
    fill_rect(cx - rw / 2, line_y - rh, rw, rh, BLACK);
}

/// Quarter-rest: a rough z-shape built from three short strokes.
/// (A proper engraved glyph would need a font; we approximate with lines.)
fn draw_rest_quarter(cx: i32, y_staff_top: i32) {
    // Centred on the staff, spanning about 2 staff-spaces.
    let t = pt(0.10);
    let top = y_staff_top + py_sp(1.0);
    let mid = y_staff_top + py_sp(2.0);
    let bottom = y_staff_top + py_sp(3.0);
    let dx = pt(0.4);
    // Three diagonal strokes making a rough quarter-rest zigzag
    stroke_line(cx - dx, top, cx + dx, mid, t, BLACK);
    stroke_line(cx + dx, mid, cx - dx, mid, t, BLACK);
    stroke_line(cx - dx, mid, cx + dx, bottom, t, BLACK);
}

// Helper: convert a staff-space offset to pixels (no margin, just the scale).
fn py_sp(sp: f64) -> i32 {
    (sp * PX_PER_SP).round() as i32
}

/// Draw a treble clef at (x, y) in pixels using a text label.
/// A full engraved glyph requires a music font; we emit a text command
/// so the downstream renderer can substitute the proper glyph.
fn draw_clef_treble(x: i32, y: i32) {
    draw_text(x, y, "G", BLACK); // "G" clef; renderer substitutes music glyph
}

fn draw_clef_bass(x: i32, y: i32) {
    draw_text(x, y, "F", BLACK); // "F" clef
                                 // two dots to the right of the F clef
    let dot_r = pt(0.15);
    let dot_x = x + pt(1.0);
    fill_circle(dot_x, y + py_sp(0.5) - dot_r, dot_r * 2, dot_r * 2, BLACK);
    fill_circle(dot_x, y + py_sp(1.5) - dot_r, dot_r * 2, dot_r * 2, BLACK);
}

/// Draw a sharp sign (#) at pixel position (x, y).
/// Approximated with four lines: two verticals + two horizontals.
fn draw_sharp(x: i32, y: i32) {
    let t = pt(0.08);
    let h = py_sp(1.2); // total height
    let dh = py_sp(0.25); // vertical spread of the two bars
    let w = pt(0.55); // width of horizontal bars
                      // two vertical strokes
    stroke_line(x - pt(0.12), y - h / 2, x - pt(0.12), y + h / 2, t, BLACK);
    stroke_line(x + pt(0.12), y - h / 2, x + pt(0.12), y + h / 2, t, BLACK);
    // two horizontal bars
    stroke_line(x - w / 2, y - dh, x + w / 2, y - dh, pt(0.12), BLACK);
    stroke_line(x - w / 2, y + dh, x + w / 2, y + dh, pt(0.12), BLACK);
}

/// Draw a flat sign (b) at pixel position (x, y).
/// Approximated with one vertical stroke + a filled-arc bump.
fn draw_flat(x: i32, y: i32) {
    let t = pt(0.08);
    let h = py_sp(1.6);
    // vertical stroke
    stroke_line(x, y - h / 2, x, y + py_sp(0.4), t, BLACK);
    // bump (filled arc forming the belly of the flat)
    let rw = pt(0.5);
    let rh = py_sp(0.55);
    fill_arc(x, y + py_sp(0.1), rw.min(rh), 270, 90, BLACK);
    // white cutout on top half so it looks open
    fill_arc(
        x,
        y + py_sp(0.1),
        (rw as f64 * 0.55) as i32,
        270,
        90,
        (255, 255, 255),
    );
}

// ============================================================
// Key signature positions
// ============================================================

/// Standard positions (in half-spaces above bottom staff line) for
/// each successive sharp, for treble and bass clefs.
/// Treble: F5 C5 G5 D5 A4 E5 B4  -> halfsp: 9 6 10 7 4 8 5  (approx)
/// Bass:   F3 C3 G3 D3 A2 E3 B2  -> halfsp: 5 2 6 3 0 4 1
const SHARP_HALFSP_TREBLE: [i32; 7] = [9, 6, 10, 7, 4, 8, 5];
const SHARP_HALFSP_BASS: [i32; 7] = [5, 2, 6, 3, 0, 4, 1];
const FLAT_HALFSP_TREBLE: [i32; 7] = [5, 8, 4, 7, 3, 6, 2];
const FLAT_HALFSP_BASS: [i32; 7] = [1, 4, 0, 3, -1, 2, -2];

// ============================================================
// Command parser
// ============================================================

fn parse_f(s: &str) -> f64 {
    s.parse().unwrap_or(0.0)
}
fn parse_i(s: &str) -> i32 {
    s.parse().unwrap_or(0)
}

// ============================================================
// Per-staff clef tracking for KEYSIG placement
// ============================================================

#[derive(Clone, Copy, PartialEq)]
enum ClefKind {
    Treble,
    Bass,
}

// ============================================================
// Main dispatch  (stateful: tracks last clef per y-position)
// ============================================================

fn main() {
    let stdin = io::stdin();

    // State carried across lines.
    let mut last_clef: ClefKind = ClefKind::Treble; // clef seen most recently
    let mut first = true;

    for line in stdin.lock().lines() {
        let line = line.expect("read stdin");
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        let tokens: Vec<&str> = line.split_whitespace().collect();
        if tokens.is_empty() {
            continue;
        }

        match tokens[0] {
            // -------------------------------------------------------
            // SCORE staves=N slots=F timesig=N/D title="T"
            // -------------------------------------------------------
            "SCORE" => {
                if first {
                    println!("# draw.rs output --- Nuklear/Xlib primitives");
                    println!("# {}", line);
                    first = false;
                }
            }

            // -------------------------------------------------------
            // EXTENT total_x total_y
            // -------------------------------------------------------
            "EXTENT" => {
                if tokens.len() < 3 {
                    continue;
                }
                let total_x = parse_f(tokens[1]);
                let total_y = parse_f(tokens[2]);
                // Add margins on all sides.
                let pw = px(total_x) + MARGIN_X as i32 * 2;
                let ph = py(total_y) + MARGIN_Y as i32 * 2;
                println!("# canvas {} {}", pw, ph);
                fill_rect(0, 0, pw, ph, (255, 255, 255));
            }

            // -------------------------------------------------------
            // LINE x1 y1 x2 y2 t
            // -------------------------------------------------------
            "LINE" => {
                if tokens.len() < 6 {
                    continue;
                }
                let x1 = px(parse_f(tokens[1]));
                let y1 = py(parse_f(tokens[2]));
                let x2 = px(parse_f(tokens[3]));
                let y2 = py(parse_f(tokens[4]));
                let t = pt(parse_f(tokens[5])).max(1);
                stroke_line(x1, y1, x2, y2, t, BLACK);
            }

            // -------------------------------------------------------
            // NOTEHEAD x y filled sp
            // -------------------------------------------------------
            "NOTEHEAD" => {
                if tokens.len() < 4 {
                    continue;
                }
                let cx = px(parse_f(tokens[1]));
                let cy = py(parse_f(tokens[2]));
                let filled = tokens[3] == "1";
                if filled {
                    draw_notehead_filled(cx, cy);
                } else {
                    draw_notehead_open(cx, cy);
                }
            }

            // -------------------------------------------------------
            // WHOLE x y sp
            // -------------------------------------------------------
            "WHOLE" => {
                if tokens.len() < 3 {
                    continue;
                }
                let cx = px(parse_f(tokens[1]));
                let cy = py(parse_f(tokens[2]));
                draw_notehead_whole(cx, cy);
            }

            // -------------------------------------------------------
            // STEM x y_base y_tip
            // -------------------------------------------------------
            "STEM" => {
                if tokens.len() < 4 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y_base = py(parse_f(tokens[2]));
                let y_tip = py(parse_f(tokens[3]));
                stroke_line(x, y_base, x, y_tip, pt(0.10).max(1), BLACK);
            }

            // -------------------------------------------------------
            // LEDGER x y half_width
            // -------------------------------------------------------
            "LEDGER" => {
                if tokens.len() < 4 {
                    continue;
                }
                let cx = px(parse_f(tokens[1]));
                let cy = py(parse_f(tokens[2]));
                // half_width is in slots
                let hw = (parse_f(tokens[3]) * PX_PER_SLOT).round() as i32;
                stroke_line(cx - hw, cy, cx + hw, cy, pt(0.08).max(1), BLACK);
            }

            // -------------------------------------------------------
            // DOT x y
            // -------------------------------------------------------
            "DOT" => {
                if tokens.len() < 3 {
                    continue;
                }
                let cx = px(parse_f(tokens[1]));
                let cy = py(parse_f(tokens[2]));
                let r = pt(0.15).max(2);
                fill_circle(cx - r, cy - r, r * 2, r * 2, BLACK);
            }

            // -------------------------------------------------------
            // TIE x1 y x2 up
            // Rendered as a thin cubic bezier arc curving up or down.
            // -------------------------------------------------------
            "TIE" => {
                if tokens.len() < 5 {
                    continue;
                }
                let x0 = px(parse_f(tokens[1]));
                let y = py(parse_f(tokens[2]));
                let x1 = px(parse_f(tokens[3]));
                let up = tokens[4] == "1";

                // Bulge direction: up means arc curves above the notehead.
                let bulge = py_sp(0.8) * if up { -1 } else { 1 };
                let ctrl_y = y + bulge;
                // Place control points at one-third and two-thirds of the span.
                let ctrl_x0 = x0 + (x1 - x0) / 3;
                let ctrl_x1 = x0 + 2 * (x1 - x0) / 3;

                stroke_curve(
                    x0,
                    y,
                    ctrl_x0,
                    ctrl_y,
                    ctrl_x1,
                    ctrl_y,
                    x1,
                    y,
                    pt(0.08).max(1),
                    BLACK,
                );
            }

            // -------------------------------------------------------
            // REST x y dur      (dur: 1=whole 2=half 4=quarter)
            //
            // render.rs always emits y = staff_y + 2.0 (the middle line of
            // the staff in staff-spaces).  We recover staff_y by subtracting 2.
            // -------------------------------------------------------
            "REST" => {
                if tokens.len() < 4 {
                    continue;
                }
                let cx = px(parse_f(tokens[1]));
                let y_mid_sp = parse_f(tokens[2]); // staff_y + 2.0
                let y_top_sp = y_mid_sp - 2.0; // staff_y (top line)
                let y_staff_top = py(y_top_sp);
                let dur = parse_i(tokens[3]);
                match dur {
                    1 => draw_rest_whole(cx, y_staff_top),
                    2 => draw_rest_half(cx, y_staff_top),
                    _ => draw_rest_quarter(cx, y_staff_top),
                }
            }

            // -------------------------------------------------------
            // BARLINE x y_top y_bot
            // -------------------------------------------------------
            "BARLINE" => {
                if tokens.len() < 4 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y_top = py(parse_f(tokens[2]));
                let y_bot = py(parse_f(tokens[3]));
                stroke_line(x, y_top, x, y_bot, pt(0.10).max(1), BLACK);
            }

            // -------------------------------------------------------
            // CLEF x y kind
            // -------------------------------------------------------
            "CLEF" => {
                if tokens.len() < 4 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y = py(parse_f(tokens[2]));
                let kind = tokens[3];
                match kind {
                    "treble" => {
                        last_clef = ClefKind::Treble;
                        draw_clef_treble(x, y);
                    }
                    "bass" => {
                        last_clef = ClefKind::Bass;
                        draw_clef_bass(x, y);
                    }
                    _ => draw_text(x, y, kind, BLACK),
                }
            }

            // -------------------------------------------------------
            // TIMESIG x y num den
            // -------------------------------------------------------
            "TIMESIG" => {
                if tokens.len() < 5 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y = py(parse_f(tokens[2]));
                let num = tokens[3];
                let den = tokens[4];
                // Stack numerator on line 1, denominator on line 3 of the staff.
                // Line 1 is at y, line 3 is at y + 2 staff-spaces.
                draw_text(x, y + py_sp(0.5), num, BLACK);
                draw_text(x, y + py_sp(2.5), den, BLACK);
            }

            // -------------------------------------------------------
            // KEYSIG x y sharps
            //   sharps > 0: draw that many sharps at standard positions
            //   sharps < 0: draw that many flats at standard positions
            //   Uses last_clef to select correct staff-line positions.
            // -------------------------------------------------------
            "KEYSIG" => {
                if tokens.len() < 4 {
                    continue;
                }
                let x0 = px(parse_f(tokens[1]));
                let y_top = parse_f(tokens[2]); // staff top in staff-spaces
                let sharps = parse_i(tokens[3]);
                if sharps == 0 {
                    continue;
                }

                // Horizontal spacing between accidentals: ~0.6 staff-spaces wide each.
                let acc_step = pt(0.7);

                if sharps > 0 {
                    let positions = if last_clef == ClefKind::Treble {
                        &SHARP_HALFSP_TREBLE[..]
                    } else {
                        &SHARP_HALFSP_BASS[..]
                    };
                    let count = sharps.clamp(0, 7) as usize;
                    for i in 0..count {
                        // halfsp 0 = bottom staff line = y_top + 4 staff-spaces
                        let halfsp = positions[i];
                        let y_acc = py(y_top + 4.0 - halfsp as f64 * 0.5);
                        let x_acc = x0 + i as i32 * acc_step;
                        draw_sharp(x_acc, y_acc);
                    }
                } else {
                    let positions = if last_clef == ClefKind::Treble {
                        &FLAT_HALFSP_TREBLE[..]
                    } else {
                        &FLAT_HALFSP_BASS[..]
                    };
                    let count = (-sharps).clamp(0, 7) as usize;
                    for i in 0..count {
                        let halfsp = positions[i];
                        let y_acc = py(y_top + 4.0 - halfsp as f64 * 0.5);
                        let x_acc = x0 + i as i32 * acc_step;
                        draw_flat(x_acc, y_acc);
                    }
                }
            }

            // -------------------------------------------------------
            // FIGURE x y text
            // -------------------------------------------------------
            "FIGURE" => {
                if tokens.len() < 4 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y = py(parse_f(tokens[2]));
                let text = tokens[3];
                // Figured-bass numbers drawn smaller and in a muted colour.
                // The downstream renderer should honour a "small" font hint here.
                draw_text(x, y, text, GREY);
            }

            // -------------------------------------------------------
            // GLYPH x y scale name     (accidentals: sharp | flat)
            // -------------------------------------------------------
            "GLYPH" => {
                if tokens.len() < 5 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y = py(parse_f(tokens[2]));
                // tokens[3] is scale; ignored — we draw at fixed engraving size.
                let name = tokens[4];
                match name {
                    "sharp" => draw_sharp(x, y),
                    "flat" => draw_flat(x, y),
                    _ => draw_text(x, y, name, BLACK),
                }
            }

            // -------------------------------------------------------
            // TITLE x y "text"
            // -------------------------------------------------------
            "TITLE" => {
                if tokens.len() < 4 {
                    continue;
                }
                let x = px(parse_f(tokens[1]));
                let y = py(parse_f(tokens[2]));
                let raw = tokens[3..].join(" ");
                let raw = raw.trim_matches('"');
                let text = raw.replace('_', " ");
                draw_text(x, y, &text, BLACK);
            }

            _ => {
                println!("# unknown: {}", line);
            }
        }
    }
}
