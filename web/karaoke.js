// SPDX-License-Identifier: MIT
// karaoke.js --- melody playback via MIDI output (port of karaoke.rs)
// Copyright (c) 2026 Jakob Kastelic
//
// Receives LESSON / MELODY / BPM lines from the server.
// start(bpm) plays count-in then melody; stop() cancels immediately.
// Calls onDone() when playback finishes naturally.

function _parseKaraokeToken(tok, beatsDenominator) {
    tok = tok.replace(/~/g, "");
    if (!tok) return null;

    const isRest = tok[0] === "r";
    let midi = null, cursor = 0;

    if (!isRest) {
        const NAMES = [
            ["cis",1],["dis",3],["fis",6],["gis",8],["ais",10],
            ["c",0],["d",2],["e",4],["f",5],["g",7],["a",9],["b",11],
        ];
        let pc = -1;
        for (const [name, val] of NAMES) {
            if (tok.startsWith(name)) { pc = val; cursor = name.length; break; }
        }
        if (pc < 0) return null;

        let octave = 3;
        while (cursor < tok.length) {
            if      (tok[cursor] === "'") { octave++; cursor++; }
            else if (tok[cursor] === ",") { octave--; cursor++; }
            else break;
        }
        const m = (octave + 1) * 12 + pc;
        if (m < 0 || m > 127) return null;
        midi = m;
    } else {
        cursor = 1;
    }

    const rem  = tok.slice(cursor);
    const dm   = rem.match(/^(\d+)/);
    const durNum = dm ? parseFloat(dm[1]) : 4;
    cursor += dm ? dm[0].length : 0;

    let dots = 0;
    while (cursor < tok.length && tok[cursor] === ".") { dots++; cursor++; }

    // dot factor: 1.0 for no dots, 1.5 for one dot, 1.75 for two, etc.
    const dotFactor = (Math.pow(2, dots + 1) - 1) / Math.pow(2, dots);
    const beats = (beatsDenominator / durNum) * dotFactor;

    return { midi, beats };
}

class Karaoke {
    constructor({ getMidiOutput, onDone }) {
        this.getMidiOutput = getMidiOutput;
        this.onDone        = onDone;

        this.bpm             = 120;
        this.beatsPerBar     = 4;
        this.beatsDenominator = 4;
        this.melody          = [];
        this._stop           = false;
    }

    handleLine(line) {
        if (line.startsWith("LESSON ")) {
            this.melody = [];
            const parts  = line.split(/\s+/);
            const [num, den] = (parts[3] || "4/4").split("/").map(Number);
            if (num > 0) this.beatsPerBar      = num;
            if (den > 0) this.beatsDenominator = den;
        } else if (line.startsWith("MELODY ")) {
            const colon = line.indexOf(": ");
            if (colon < 0) return;
            for (const tok of line.slice(colon + 2).split(/\s+/)) {
                const n = _parseKaraokeToken(tok, this.beatsDenominator);
                if (n) this.melody.push(n);
            }
        } else if (line.startsWith("BPM ")) {
            const v = parseFloat(line.slice(4));
            if (v > 0) this.bpm = v;
        }
    }

    start(bpm) {
        this._stop = false;
        if (bpm > 0) this.bpm = bpm;
        this._run(this.bpm, this.beatsPerBar, [...this.melody]);
    }

    stop() {
        this._stop = true;
        const out = this.getMidiOutput();
        if (out) out.send([0xB0, 123, 0]); // All Notes Off
    }

    async _run(bpm, beatsPerBar, melody) {
        const sleep  = ms => new Promise(r => setTimeout(r, ms));
        const noteOn  = (note, vel) => { const o = this.getMidiOutput(); if (o) o.send([0x90, note, vel]); };
        const noteOff = (note)      => { const o = this.getMidiOutput(); if (o) o.send([0x80, note, 0]);   };

        // Count-in: one click per beat using B5 (MIDI 83), matching karaoke.rs
        const CLICK      = 83;
        const beatMs     = 60000 / bpm;
        const clickOnMs  = 30;
        const clickOffMs = Math.max(0, beatMs - clickOnMs);

        for (let i = 0; i < beatsPerBar; i++) {
            if (this._stop) return;
            noteOn(CLICK, 64);
            await sleep(clickOnMs);
            noteOff(CLICK);
            await sleep(clickOffMs);
        }

        // Melody
        for (const note of melody) {
            if (this._stop) return;
            const totalMs = note.beats * 60000 / bpm;
            if (note.midi !== null) {
                noteOn(note.midi, 80);
                await sleep(totalMs * 0.9);
                if (this._stop) { noteOff(note.midi); return; }
                noteOff(note.midi);
                await sleep(totalMs * 0.1);
            } else {
                await sleep(totalMs);
            }
        }

        if (!this._stop) this.onDone();
    }
}
