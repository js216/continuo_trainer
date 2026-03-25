// SPDX-License-Identifier: MIT
// rules.js --- voice-leading validation (port of rules.lua)
// Copyright (c) 2026 Jakob Kastelic

// ── helpers ───────────────────────────────────────────────────────────────────

function remEuclid(a, b) { const r = a % b; return r < 0 ? r + b : r; }
function signum(x)        { return x > 0 ? 1 : x < 0 ? -1 : 0; }

function toSemitone(p) {
    if (!p) return null;
    p = p.replace(/[\d.]+$/, "").trim();
    if (!p) return null;
    const first = p[0].toLowerCase();
    const rest  = p.slice(1);
    let acc, octPart;
    if      (rest.startsWith("isis"))                              { acc =  2; octPart = rest.slice(4); }
    else if (rest.startsWith("eses"))                              { acc = -2; octPart = rest.slice(4); }
    else if (rest.startsWith("is"))                                { acc =  1; octPart = rest.slice(2); }
    else if (rest.startsWith("es") || rest.startsWith("as"))       { acc = -1; octPart = rest.slice(2); }
    else                                                           { acc =  0; octPart = rest; }
    let octaves = 0;
    for (const c of octPart) { if (c === "'") octaves += 12; else if (c === ",") octaves -= 12; }
    const baseMap = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 };
    const base = baseMap[first];
    if (base === undefined) return null;
    return base + acc + octaves + 48; // c' = 60
}

const _PC_NAMES = ["c","cis","d","dis","e","f","fis","g","gis","a","ais","b"];
function pcName(semitone)    { return _PC_NAMES[remEuclid(semitone, 12)]; }
function pitchName(semitone) {
    const pc     = remEuclid(semitone, 12);
    const octave = Math.floor(semitone / 12) - 4;
    const marks  = octave > 0 ? "'".repeat(octave) : octave < 0 ? ",".repeat(-octave) : "";
    return _PC_NAMES[pc] + marks;
}

function flattenVoices(g) {
    const voices = [{ semitone: g.bass, role: "bass" }];
    const mel  = g.melody[0];
    const bPc  = remEuclid(g.bass, 12);
    const mPc  = mel !== undefined ? remEuclid(mel, 12) : null;
    for (let i = 0; i < g.inner.length; i++) {
        const nPc = remEuclid(g.inner[i], 12);
        if (nPc !== mPc && nPc !== bPc)
            voices.push({ semitone: g.inner[i], role: "inner_" + (i + 1) });
    }
    if (mel !== undefined) voices.push({ semitone: mel, role: "melody" });
    return voices;
}

// ── key ───────────────────────────────────────────────────────────────────────

const _MAJOR_STEPS = [0, 2, 4, 5, 7, 9, 11];
const _MINOR_STEPS = [0, 2, 3, 5, 7, 8, 10];

function parseKey(token) {
    if (!token) return { scalePcs: [] };
    const first = token[0];
    const up    = first.toUpperCase();
    const rootBaseMap = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
    const rootBase = rootBaseMap[up];
    if (rootBase === undefined) return { scalePcs: [] };
    const rest   = token.slice(1);
    const accMap = { "#": 1, "is": 1, "##": 2, "isis": 2,
                     "b": -1, "es": -1, "as": -1, "bb": -2, "eses": -2 };
    const acc    = accMap[rest] ?? 0;
    const root   = remEuclid(rootBase + acc, 12);
    const steps  = (first === first.toLowerCase()) ? _MINOR_STEPS : _MAJOR_STEPS;
    return { scalePcs: steps.map(s => (root + s) % 12) };
}

// ── figures ───────────────────────────────────────────────────────────────────

function parseOneFigure(t) {
    t = t.trim();
    if (!t) return null;
    let acc, rest;
    if      (t.startsWith("##"))                                     { acc =  2; rest = t.slice(2); }
    else if (t.startsWith("bb"))                                     { acc = -2; rest = t.slice(2); }
    else if (t.startsWith("#"))                                      { acc =  1; rest = t.slice(1); }
    else if (t[0] === "b" && (t.length === 1 || /\d/.test(t[1])))   { acc = -1; rest = t.slice(1); }
    else if (t.startsWith("n"))                                      { acc =  0; rest = t.slice(1); }
    else if (t.startsWith("isis"))                                   { acc =  2; rest = t.slice(4); }
    else if (t.startsWith("eses"))                                   { acc = -2; rest = t.slice(4); }
    else if (t.startsWith("is"))                                     { acc =  1; rest = t.slice(2); }
    else if (t.startsWith("es") || t.startsWith("as"))               { acc = -1; rest = t.slice(2); }
    else                                                             { acc =  0; rest = t; }
    const deg = rest === "" ? 3 : parseInt(rest);
    if (isNaN(deg)) return null;
    return { deg, acc };
}

function _defaultTriad() { return [{ deg: 3, acc: 0 }, { deg: 5, acc: 0 }, { deg: 8, acc: 0 }]; }

function parseFigures(s) {
    s = (s || "").trim();
    if (!s || s === "0") return _defaultTriad();
    let figs = s.split(/[/,]/).map(parseOneFigure).filter(Boolean);
    if (figs.length === 0) return _defaultTriad();
    if (figs.length === 1 && (figs[0].deg === 3 || figs[0].deg === 4))
        figs.push({ deg: 5, acc: 0 }, { deg: 8, acc: 0 });
    if (figs.length === 1 && figs[0].deg === 6)
        figs.unshift({ deg: 3, acc: 0 });
    if (figs.length === 1 && figs[0].deg === 7)
        figs.push({ deg: 5, acc: 0 }, { deg: 3, acc: 0 });
    figs.sort((a, b) => a.deg - b.deg);
    const deduped = [figs[0]];
    for (let k = 1; k < figs.length; k++)
        if (figs[k].deg !== figs[k - 1].deg) deduped.push(figs[k]);
    return deduped;
}

const _NATURAL_PCS = [0, 2, 4, 5, 7, 9, 11]; // C D E F G A B

function figureToPc(fig, bassPc, bassLetterIdx, key) {
    const deg = remEuclid(fig.deg - 1, 7) + 1;
    if (fig.acc !== 0) {
        const bassNaturalPc = remEuclid(bassPc, 12);
        let idx = _NATURAL_PCS.indexOf(bassNaturalPc);
        if (idx < 0) idx = bassLetterIdx !== null ? bassLetterIdx - 1 : -1;
        if (idx >= 0) {
            const targetIdx = remEuclid(idx + (deg - 1), 7);
            return remEuclid(_NATURAL_PCS[targetIdx] + fig.acc, 12);
        }
    }
    let bassDeg = null;
    for (let i = 0; i < key.scalePcs.length; i++)
        if (key.scalePcs[i] === bassPc) { bassDeg = i; break; }
    if (bassDeg === null) return null;
    return key.scalePcs[remEuclid(bassDeg + (deg - 1), 7)];
}

// ── rules ─────────────────────────────────────────────────────────────────────

function _parallelVoices(ctx) {
    if (ctx.window.length < 2) return null;
    const prev = ctx.window[ctx.window.length - 2];
    const curr = ctx.window[ctx.window.length - 1];
    if (curr.passing || prev.passing) return null;
    return [flattenVoices(prev), flattenVoices(curr)];
}

function _findParallel(pv, cv, interval) {
    const cByRole = {};
    for (const v of cv) cByRole[v.role] = v.semitone;
    for (let vi = 0; vi < pv.length; vi++) {
        const pvI = pv[vi], cvI = cByRole[pvI.role];
        if (cvI === undefined) continue;
        for (let vj = vi + 1; vj < pv.length; vj++) {
            const pvJ = pv[vj], cvJ = cByRole[pvJ.role];
            if (cvJ === undefined) continue;
            if (Math.abs(pvJ.semitone - pvI.semitone) % 12 === interval &&
                Math.abs(cvJ - cvI) % 12 === interval) {
                const mI = cvI - pvI.semitone, mJ = cvJ - pvJ.semitone;
                if (signum(mI) === signum(mJ) && mI !== 0)
                    return [pvI.semitone, pvJ.semitone, cvI, cvJ];
            }
        }
    }
    return null;
}

function _parallelMsg(kind, [pi, pj, ci, cj]) {
    return `Parallel ${kind}: (${pitchName(pi)}, ${pitchName(pj)}) -> (${pitchName(ci)}, ${pitchName(cj)})`;
}

function ruleNoParallelFifths(ctx) {
    const pv = _parallelVoices(ctx);
    if (!pv) return [true, null];
    const r = _findParallel(pv[0], pv[1], 7);
    return r ? [false, _parallelMsg("fifths", r)] : [true, null];
}

function ruleNoParallelOctaves(ctx) {
    const pv = _parallelVoices(ctx);
    if (!pv) return [true, null];
    const r = _findParallel(pv[0], pv[1], 0);
    return r ? [false, _parallelMsg("octaves", r)] : [true, null];
}

function ruleBassLeap(ctx) {
    if (ctx.window.length < 2) return [true, null];
    const p = ctx.window[ctx.window.length - 2];
    const c = ctx.window[ctx.window.length - 1];
    return Math.abs(c.bass - p.bass) % 12 === 6
        ? [false, "Tritone leap in bass"] : [true, null];
}

function ruleCheckRealization(ctx) {
    const g = ctx.window[ctx.window.length - 1];
    if (g.passing) return [true, null];
    const bassPc  = remEuclid(g.bass, 12);
    const allowed = new Set([bassPc]);
    for (const fig of g.figures) {
        const pc = figureToPc(fig, bassPc, g.bassLetterIdx, ctx.key);
        if (pc !== null && pc !== undefined) allowed.add(pc);
        else return [true, null];
    }
    if (ctx.window.length >= 2) {
        const prev = ctx.window[ctx.window.length - 2];
        if (remEuclid(prev.bass, 12) === bassPc) {
            for (const n of prev.inner)  allowed.add(remEuclid(n, 12));
            for (const n of prev.melody) allowed.add(remEuclid(n, 12));
            allowed.add(remEuclid(prev.bass, 12));
        }
    }
    const figLabel = g.figures.map(f => {
        const a = f.acc === 2 ? "##" : f.acc === 1 ? "#" : f.acc === -1 ? "b" : f.acc === -2 ? "bb" : "";
        return a + f.deg;
    }).join("/");
    for (const note of g.inner) {
        const pc = remEuclid(note, 12);
        if (!allowed.has(pc))
            return [false, `Incorrect realization of figure ${figLabel}: ${pcName(note)}`];
    }
    return [true, null];
}

function ruleBassInKey(ctx) {
    const g = ctx.window[ctx.window.length - 1];
    if (g.passing) return [true, null];
    const bassPc    = remEuclid(g.bass, 12);
    const notatedPc = remEuclid(g.bassNotated, 12);
    if (notatedPc === bassPc) return [true, null];
    return ctx.key.scalePcs.includes(bassPc)
        ? [true, null]
        : [false, `Bass note ${pcName(g.bass)} is not diatonic to the key`];
}

function ruleNotPastEnd(ctx) {
    const g = ctx.window[ctx.window.length - 1];
    return g.bassNotatedRaw === "?"
        ? [false, "Group is past the end of the lesson"] : [true, null];
}

function ruleRealizationNotEmpty(ctx) {
    const g = ctx.window[ctx.window.length - 1];
    if (g.passing) return [true, null];
    return g.inner.length === 0
        ? [false, "No realization provided (inner voices are empty)"] : [true, null];
}

function ruleRealizationComplete(ctx) {
    const g = ctx.window[ctx.window.length - 1];
    if (g.passing) return [true, null];
    const bassPc  = remEuclid(g.bass, 12);
    const present = new Set([bassPc]);
    for (const n of g.inner)  present.add(remEuclid(n, 12));
    for (const n of g.melody) present.add(remEuclid(n, 12));
    for (const fig of g.figures) {
        const pc = figureToPc(fig, bassPc, g.bassLetterIdx, ctx.key);
        if (pc === null || pc === undefined) return [true, null];
        if (!present.has(pc))
            return [false, `Figure ${fig.deg} (${pcName(pc)}) is missing from the realization`];
    }
    return [true, null];
}

const _RULES = [
    ruleNoParallelFifths, ruleNoParallelOctaves,
    ruleBassLeap,
    ruleCheckRealization, ruleBassInKey,
    ruleNotPastEnd, ruleRealizationNotEmpty, ruleRealizationComplete,
];

// ── group parsing ─────────────────────────────────────────────────────────────

const _LETTER_MAP = { c: 1, d: 2, e: 3, f: 4, g: 5, a: 6, b: 7 };

function _parseGroup(line) {
    if (!line.startsWith("GROUP")) return null;
    const tokens = line.split(/\s+/);
    let id = 0, passing = line.includes("passing");
    let bass = 0, bassLetterIdx = null, bassNotatedRaw = "";
    let figures = parseFigures("0"), inner = [], melody = [], timeMs = 0;

    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        let v;
        if ((v = t.match(/^ID:(.+)$/)))          id = parseInt(v[1]) || 0;
        if ((v = t.match(/^BASS:(.+)$/)))         bassNotatedRaw = v[1];
        if ((v = t.match(/^BASS_ACTUAL:(.+)$/))) {
            bass = toSemitone(v[1]) ?? 0;
            const letter = v[1].replace(/[\d.]+$/, "")[0]?.toLowerCase();
            bassLetterIdx = _LETTER_MAP[letter] ?? null;
        }
        if ((v = t.match(/^FIGURES:(.+)$/)))      figures = parseFigures(v[1]);
        if ((v = t.match(/^REALIZATION:(.+)$/))) {
            inner = v[1].split("/").map(toSemitone).filter(x => x !== null);
            inner.sort((a, b) => a - b);
        }
        if ((v = t.match(/^MELODY:(.*)$/))) {
            if (v[1]) melody.push(toSemitone(v[1]) ?? 0);
            while (i + 1 < tokens.length && !tokens[i + 1].includes(":")) {
                i++;
                melody.push(toSemitone(tokens[i]) ?? 0);
            }
        }
        if ((v = t.match(/^TIME:(.+)$/)))         timeMs = parseFloat(v[1]) || 0;
    }
    const bassNotated = bassNotatedRaw === "" ? bass : (toSemitone(bassNotatedRaw) ?? bass);
    return { id, passing, bass, bassLetterIdx, bassNotated, bassNotatedRaw,
             figures, inner, melody, time: timeMs };
}

// ── state + public API ────────────────────────────────────────────────────────

let _rulesWindow = [];
let _rulesKey    = { scalePcs: [] };

// Call for every LESSON line to keep key + window in sync.
function handleRulesLine(line) {
    if (line.startsWith("LESSON ")) {
        const parts = line.split(/\s+/);
        if (parts[2]) _rulesKey = parseKey(parts[2]);
        _rulesWindow = [];
    }
}

// Evaluate one GROUP line; returns newline-separated RESULT line(s).
function evaluateGroup(groupLine) {
    const g = _parseGroup(groupLine);
    if (!g) return "";
    _rulesWindow.push(g);
    if (_rulesWindow.length > 4) _rulesWindow.shift();

    const errors = [];
    for (const rule of _RULES) {
        const [ok, err] = rule({ window: _rulesWindow, key: _rulesKey });
        if (!ok) errors.push(err);
    }
    if (errors.length === 0) return `RESULT ${g.id} TIME:${g.time} OK`;
    return errors.map(err => `RESULT ${g.id} TIME:${g.time} FAIL ${err}`).join("\n");
}
