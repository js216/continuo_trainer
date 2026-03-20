// SPDX-License-Identifier: MIT
// all.js --- browser-side chunk server (port of all.lua's runtime LOAD_CHUNK/QUERY_CHILDREN)
// Copyright (c) 2026 Jakob Kastelic
//
// On init(), fetches /chn/index.json and emits CHUNK_NAME lines for every
// known chunk.  LOAD_CHUNK fetches /chn/<hash>.txt and emits the lesson
// protocol.  QUERY_CHILDREN answers synchronously from the pre-loaded index.

class All {
    // onLine: callback that receives emitted protocol lines (CHUNK_NAME, CHUNK_SESSION,
    //         LESSON, BASSNOTE, FIGURES, MELODY, LESSON_END, CHILDREN)
    constructor(onLine) {
        this._onLine = onLine;
        this._index  = null;  // hash → { level, skills, children:[{hash,s,e}] }
    }

    // Fetch index.json and emit CHUNK_NAME for every known chunk.
    async init() {
        try {
            const r = await fetch("/chn/index.json");
            if (r.ok) this._index = await r.json();
        } catch (_) {}
        if (this._index) {
            for (const [hash, info] of Object.entries(this._index))
                this._onLine(`CHUNK_NAME ${hash} ${info.level} ${info.skills || ""}`);
        }
    }

    // Fetch /chn/<hash>.txt, parse it, emit lesson protocol lines.
    async loadChunk(hash) {
        let text;
        try {
            const r = await fetch(`/chn/${hash}.txt`);
            if (!r.ok) { console.error(`all.js: cannot fetch /chn/${hash}.txt (${r.status})`); return; }
            text = await r.text();
        } catch (e) { console.error(`all.js: fetch error for /chn/${hash}.txt:`, e); return; }

        const lesson = this._parseChunk(text);
        const groups = this._groupMelodyTokens(lesson.rawBass, lesson.melody);

        this._onLine(`CHUNK_SESSION ${hash}`);
        this._onLine(`LESSON ${hash} ${lesson.key} ${lesson.time} ${lesson.bpm} ${lesson.bar}`);
        for (let i = 0; i < lesson.rawBass.length; i++) {
            const tok     = lesson.rawBass[i];
            const passing = this._isPassingTok(tok);
            const clean   = passing ? tok.slice(0, -1) : tok;
            this._onLine(`BASSNOTE ${i}: ${clean}${passing ? " passing" : ""}`);
            this._onLine(`FIGURES ${i}: ${lesson.figures[i]}`);
            const mel = groups[i];
            this._onLine(`MELODY ${i}: ${mel || "-"}`);
        }
        this._onLine("LESSON_END");
    }

    // Answer QUERY_CHILDREN synchronously from the pre-loaded index.
    queryChildren(hash) {
        const info = this._index && this._index[hash];
        const children = info ? (info.children || []) : [];
        if (children.length === 0) {
            this._onLine(`CHILDREN ${hash}`);
            return;
        }
        const parts = [hash, ...children.map(c => `${c.hash}:${c.s}:${c.e}`)];
        this._onLine(`CHILDREN ${parts.join(" ")}`);
    }

    // ── Chunk file parser (mirrors all.lua's parse_chunk) ─────────────────────

    _parseChunk(text) {
        const r = { key: "C", time: "4/4", bpm: 120, bar: 1, rawBass: [], figures: [], melody: [] };
        let mode = "";
        for (let line of text.replace(/\r/g, "").split("\n")) {
            line = line.trim();
            if (!line) continue;
            if (mode === "") {
                let m;
                if (m = line.match(/^key:\s*(.+)$/))    { r.key  = m[1].trim(); continue; }
                if (m = line.match(/^time:\s*(.+)$/))   { r.time = m[1].trim(); continue; }
                if (m = line.match(/^bpm:\s*(\d+)$/))   { r.bpm  = parseInt(m[1]); continue; }
                if (m = line.match(/^bar:\s*(\d+)$/))   { r.bar  = parseInt(m[1]); continue; }
                if (line.match(/^bassline/))             { mode = "bass"; continue; }
                if (line.match(/^figures/))              { mode = "fig";  continue; }
                if (line.match(/^melody/))               { mode = "mel";  continue; }
            } else if (line.match(/^}/)) {
                mode = "";
            } else {
                for (const tok of line.split(/\s+/)) {
                    if (!tok) continue;
                    if      (mode === "bass") r.rawBass.push(tok);
                    else if (mode === "fig")  r.figures.push(tok);
                    else if (mode === "mel")  r.melody.push(tok);
                }
            }
        }
        return r;
    }

    // A token is a passing note if it ends in 'p' preceded by a digit or '.'.
    _isPassingTok(tok) {
        if (!tok.endsWith("p")) return false;
        const pre = tok[tok.length - 2];
        return pre !== undefined && (/\d/.test(pre) || pre === ".");
    }

    // Duration in sixteenth-notes (mirrors all.lua's dur_sixteenths).
    _durSixteenths(tok) {
        const s = tok.replace(/^[a-z]+/i, "").replace(/^[',]+/, "");
        const m = s.match(/^(\d+)(\.+)?/);
        if (!m) return null;
        const base = { 1: 16, 2: 8, 4: 4, 8: 2, 16: 1 }[parseInt(m[1])];
        if (!base) return null;
        const dots = m[2] ? m[2].length : 0;
        let total = base, add = base;
        for (let i = 0; i < dots; i++) { add = Math.floor(add / 2); total += add; }
        return total;
    }

    // Group melody tokens to bass notes by accumulating duration
    // (mirrors all.lua's group_melody_tokens).
    _groupMelodyTokens(rawBass, melody) {
        // Compute duration per bass note (carrying forward last known duration)
        const bassDurs = [];
        let lastDur = 4;
        for (const tok of rawBass) {
            const d = this._durSixteenths(tok);
            if (d !== null) lastDur = d;
            bassDurs.push(lastDur);
        }

        const groups = new Array(rawBass.length).fill("");
        let melIdx = 0, lastMelDur = 4, bassTotal = 0, melTotal = 0;
        for (let bi = 0; bi < rawBass.length; bi++) {
            bassTotal += bassDurs[bi];
            while (melTotal < bassTotal && melIdx < melody.length) {
                const tok = melody[melIdx];
                const d   = this._durSixteenths(tok);
                const dur = d !== null ? d : lastMelDur;
                if (d !== null) lastMelDur = d;
                groups[bi] += (groups[bi] ? " " : "") + tok;
                melTotal += dur;
                melIdx++;
            }
        }
        return groups;
    }
}
