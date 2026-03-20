// SPDX-License-Identifier: MIT
// app.js --- main application (port of gui.cpp) + note grouping (port of group.rs)
// Copyright (c) 2026 Jakob Kastelic

// ── note spelling ──────────────────────────────────────────────────────────────

const LILY_NAMES_SHARP = ["c","cis","d","dis","e","f","fis","g","gis","a","ais","b"];
const LILY_NAMES_FLAT  = ["c","des","d","ees","e","f","ges","g","aes","a","bes","b"];

function midiToLily(midi, names) {
    names = names || LILY_NAMES_SHARP;
    const pc = midi % 12;
    const octave = Math.floor(midi / 12) - 1;
    const name = names[pc];
    const lilyOct = octave - 3; // c' = MIDI 60 → octave 4 → lilyOct 1
    if (lilyOct > 0) return name + "'".repeat(lilyOct);
    if (lilyOct < 0) return name + ",".repeat(-lilyOct);
    return name;
}

function lilyToMidi(note) {
    note = note.replace(/[\d.]+$/, "");
    if (!note) return null;
    const PC = {
        bis: 0,  c: 0,   cis: 1,  des: 1,  d: 2,
        dis: 3,  ees: 3, e: 4,    fes: 4,  f: 5,
        eis: 5,  fis: 6, ges: 6,  g: 7,    gis: 8,
        aes: 8,  a: 9,   ais: 10, bes: 10, b: 11, ces: 11,
    };
    const sorted = Object.keys(PC).sort((a, b) => b.length - a.length);
    let pc = -1, rest = "";
    for (const n of sorted) {
        if (note.startsWith(n)) { pc = PC[n]; rest = note.slice(n.length); break; }
    }
    if (pc < 0) return null;
    let octave = 3;
    for (const c of rest) { if (c === "'") octave++; else if (c === ",") octave--; }
    const midi = (octave + 1) * 12 + pc;
    return (midi >= 0 && midi <= 127) ? midi : null;
}

// ── group (port of group.rs) ───────────────────────────────────────────────────

class Group {
    constructor(onGroup) {
        this.onGroup = onGroup;
        this._reset("C");
    }

    _reset(key) {
        this.entries = {};   // id → {bassnote, figures, melody, passing}
        this.held    = {};   // midi → count
        this.groupId = 0;
        this.bass    = null;
        this.bassTime = 0;
        this.chord   = [];
        const flatKeys = ["C","F","Bb","Eb","Ab","Db","Gb","Cb"];
        this.names = flatKeys.includes(key) ? LILY_NAMES_FLAT : LILY_NAMES_SHARP;
    }

    _entry(id) {
        if (!this.entries[id])
            this.entries[id] = { bassnote: "", figures: "0", melody: "-", passing: false };
        return this.entries[id];
    }

    handleLine(line) {
        if (line.startsWith("LESSON ")) {
            this._reset(line.split(/\s+/)[2] || "C");
        } else if (line.startsWith("BASSNOTE ")) {
            const m = line.match(/^BASSNOTE\s+(\d+):\s+(\S+)(.*)?$/);
            if (m) { const e = this._entry(parseInt(m[1])); e.bassnote = m[2]; e.passing = !!m[3] && m[3].includes("passing"); }
        } else if (line.startsWith("FIGURES ")) {
            const m = line.match(/^FIGURES\s+(\d+):\s+(.+)$/);
            if (m) this._entry(parseInt(m[1])).figures = m[2].trim();
        } else if (line.startsWith("MELODY ")) {
            const m = line.match(/^MELODY\s+(\d+):\s+(.+)$/);
            if (m) this._entry(parseInt(m[1])).melody = m[2].trim();
        }
    }

    noteOn(midi, time) {
        const nextId = this.groupId + 1;
        const next = this.entries[nextId];
        if (next && next.passing && this.bass !== null) {
            const exp = lilyToMidi(next.bassnote);
            if (exp !== null && midi === exp) {
                this.onGroup(this._emit(this.groupId, this.bass, this.bassTime, this.chord));
                this.chord = this.chord.filter(m => m !== this.bass);
                this.groupId = nextId;
                this.bass = null;
                this.bassTime = 0;
            }
        }
        this.held[midi] = (this.held[midi] || 0) + 1;
        if (this.bass === null || midi < this.bass) { this.bass = midi; this.bassTime = time; }
        if (!this.chord.includes(midi)) this.chord.push(midi);
    }

    noteOff(midi) {
        if (this.held[midi] !== undefined) {
            if (this.held[midi] > 1) { this.held[midi]--; return; }
            delete this.held[midi];
        }
        if (Object.keys(this.held).length > 0 || this.bass === null) return;
        this.onGroup(this._emit(this.groupId, this.bass, this.bassTime, this.chord));
        this.groupId++;
        this.bass = null; this.bassTime = 0; this.chord = [];
    }

    _emit(id, bass, bassTime, chord) {
        const e = this.entries[id];
        const upper = chord.filter(m => m !== bass).sort((a, b) => b - a);
        return [
            `GROUP ID:${id}`,
            e && e.passing ? "passing" : null,
            `BASS:${e ? e.bassnote : "?"}`,
            `FIGURES:${e ? e.figures : "?"}`,
            `MELODY:${e ? e.melody : "?"}`,
            `BASS_ACTUAL:${midiToLily(bass, this.names)}`,
            `TIME:${bassTime}`,
            `REALIZATION:${upper.map(m => midiToLily(m, this.names)).join("/")}`,
        ].filter(Boolean).join(" ");
    }
}

// ── state ─────────────────────────────────────────────────────────────────────

const state = {
    currentChunk:      "",
    currentChunkLevel: -1,
    level0Hashes:      [],
    currentLevel0Idx:  0,

    squares:     [],   // array of "ok" | "fail" | "done" | null
    numNotes:    0,    // max BASSNOTE id seen for current chunk
    explanation: "",

    mastery: 0, masteryThresh: 80,
    power:   0, powerThresh:   70,
    pts: 0, goal: 0, goalMet: false, streak: 0,

    suggestion:     "",
    suggestionTime: -Infinity,

    ptsDelta:     0,
    ptsDeltaTime: -Infinity,
    statsInitialized: false,

    bpm:       120,
    karaokeOn: false,
};

// ── actions ───────────────────────────────────────────────────────────────────

function clearStatus() {
    state.squares     = [];
    state.numNotes    = 0;
    state.explanation = "";
    renderSquares();
    renderExplanations();
}

function loadChunk(hash) {
    stats.handleLine(`LOAD_CHUNK ${hash}`);
    all.loadChunk(hash);   // async fetch; emits CHUNK_SESSION/LESSON/etc. via parseLine
}

function reloadLesson() {
    const hash = state.currentChunk || state.level0Hashes[state.currentLevel0Idx] || "";
    if (hash) loadChunk(hash);
    clearStatus();
}

function quitLesson() {
    if (state.karaokeOn) {
        karaoke.stop();
        state.karaokeOn = false;
        updateKaraokeBtn();
    }
    clearStatus();
}

function suggestLesson() {
    karaoke.stop();
    state.karaokeOn = false;
    updateKaraokeBtn();
    stats.handleLine("SUGGEST_LESSON");
}

function toggleKaraoke() {
    state.karaokeOn = !state.karaokeOn;
    if (state.karaokeOn) karaoke.start(state.bpm);
    else                 karaoke.stop();
    updateKaraokeBtn();
}

function adjustBpm(delta) { setBpm(state.bpm + delta); }

function setBpm(val) {
    val = Math.max(1, Math.min(999, Math.round(Number(val))));
    state.bpm = val;
    document.getElementById("bpm-val").value = val;
}

// ── message parsing ────────────────────────────────────────────────────────────

function parseLine(line) {
    line = line.trim();
    if (!line) return;

    if (line.startsWith("KARAOKE_DONE")) {
        state.karaokeOn = false;
        updateKaraokeBtn();
        return;
    }
    if (line.startsWith("BPM ")) {
        const v = parseFloat(line.slice(4));
        if (v > 0) { state.bpm = Math.round(v); document.getElementById("bpm-val").value = state.bpm; }
        karaoke.handleLine(line);
        return;
    }

    if (line.startsWith("CHUNK_NAME "))   { handleChunkName(line); stats.handleLine(line); }
    if (line.startsWith("CHUNK_SESSION ")) handleChunkSession(line);
    if (line.startsWith("LESSON "))       { state.numNotes = 0; group.handleLine(line); karaoke.handleLine(line); handleRulesLine(line); stats.handleLine(line); }
    if (line.startsWith("BASSNOTE "))     { handleBassnote(line); group.handleLine(line); stats.handleLine(line); }
    if (line.startsWith("FIGURES "))      group.handleLine(line);
    if (line.startsWith("MELODY "))       { group.handleLine(line); karaoke.handleLine(line); }
    if (line.startsWith("RESULT "))       handleResult(line);
    if (line.startsWith("CHILDREN "))     stats.handleLine(line);
    if (line.startsWith("STATS "))        handleStats(line);
    if (line.startsWith("SUGGESTION "))   handleSuggestion(line);
}

function handleChunkName(line) {
    // CHUNK_NAME <hash> <level> [skills...]
    const parts = line.split(/\s+/);
    const hash = parts[1], level = parseInt(parts[2]);
    if (level === 0 && !state.level0Hashes.includes(hash))
        state.level0Hashes.push(hash);
}

function handleChunkSession(line) {
    const hash = line.split(/\s+/)[1];
    if (hash) loadScoreImage(hash);
}

function handleBassnote(line) {
    const m = line.match(/^BASSNOTE\s+(\d+):/);
    if (m) state.numNotes = Math.max(state.numNotes, parseInt(m[1]));
}

function handleResult(line) {
    // RESULT <id> TIME:<t> OK|FAIL [msg]
    // Strip ANSI colour codes (rules.lua may emit them)
    line = line.replace(/\x1b\[[0-9;]*m/g, "");
    const m = line.match(/^RESULT\s+(\d+)\s+TIME:\S+\s+(OK|FAIL)(.*)?$/);
    if (!m) return;

    const id = parseInt(m[1]);
    const ok = m[2] === "OK";
    const msg = m[3] ? m[3].trim() : "";

    if (id === 0) {
        clearStatus();
        state.ptsDelta = 0;
    }

    while (state.squares.length <= id) state.squares.push(null);
    state.squares[id] = ok ? "ok" : "fail";

    if (!ok && msg) {
        state.explanation += `${id}\t${msg}\n`;
        renderExplanations();
    }

    // Last note: add done marker and auto-reload
    if (id === state.numNotes) {
        while (state.squares.length <= id + 1) state.squares.push(null);
        state.squares[id + 1] = "done";
        const hash = state.currentChunk || state.level0Hashes[state.currentLevel0Idx] || "";
        if (hash) loadChunk(hash);
    }

    renderSquares();
}

function handleStats(line) {
    const num = (key) => { const m = line.match(new RegExp(`${key}=([\\d.]+)`)); return m ? parseFloat(m[1]) : null; };

    const total = num("total_today");
    if (total !== null) {
        const newPts = Math.round(total);
        if (state.statsInitialized && newPts > state.pts) {
            state.ptsDelta    += newPts - state.pts;
            state.ptsDeltaTime = Date.now();
        }
        state.pts = newPts;
    }

    const goal = num("goal");             if (goal  !== null) state.goal  = goal;
    const streak = num("streak");         if (streak !== null) state.streak = streak;
    const mastery = num("mastery");       if (mastery !== null) state.mastery = mastery;
    const power   = num("power");         if (power   !== null) state.power   = power;
    const mt = num("mastery_thresh");     if (mt !== null) state.masteryThresh = mt;
    const pt = num("power_thresh");       if (pt !== null) state.powerThresh   = pt;

    state.goalMet = state.goal > 0 && state.pts >= state.goal;
    state.statsInitialized = true;

    const sm = line.match(/suggestion=(\S+)/);
    if (sm) { state.suggestion = sm[1]; state.suggestionTime = Date.now(); }

    renderStatusBar();
}

function handleSuggestion(line) {
    // SUGGESTION chunk=<hash> skills=<s> level=<n> reason=<r>
    // SUGGESTION lesson=<id> reason=<r>
    // SUGGESTION none reason=<r>
    const chunkM = line.match(/chunk=(\S+)/);
    if (chunkM) {
        state.currentChunk = chunkM[1];
        const lvlM = line.match(/level=(\d+)/);
        state.currentChunkLevel = lvlM ? parseInt(lvlM[1]) : -1;
        loadChunk(state.currentChunk);
        clearStatus();
        const skillsM = line.match(/skills=(\S+)/);
        state.suggestion     = skillsM ? `chunk: ${skillsM[1]}` : "chunk";
        state.suggestionTime = Date.now();
        renderInfoBar();
        return;
    }
    const reasonM = line.match(/reason=(\S+)/);
    if (reasonM) { state.suggestion = reasonM[1]; state.suggestionTime = Date.now(); }
    renderInfoBar();
}

// ── score image ────────────────────────────────────────────────────────────────

function loadScoreImage(hash) {
    const img = document.getElementById("score-img");
    img.onload  = () => { img.style.display = "block"; };
    img.onerror = () => { img.style.display = "none";  };
    img.src = `/chn/${hash}.png`;
}

// ── rendering ──────────────────────────────────────────────────────────────────

function renderSquares() {
    const el = document.getElementById("squares");
    el.innerHTML = "";
    for (let i = 0; i < state.squares.length; i++) {
        const s = state.squares[i];
        if (s === null) continue;
        const div = document.createElement("div");
        div.className = "sq sq-" + s;
        div.textContent = s === "done" ? "|" : String(i);
        el.appendChild(div);
    }
}

function renderExplanations() {
    document.getElementById("explanations").textContent = state.explanation;
}

function renderStatusBar() {
    function setBar(fillId, value, max, color) {
        const fill = document.getElementById(fillId);
        if (!fill) return;
        fill.style.width = Math.min(value / max * 100, 100) + "%";
        fill.style.setProperty("--color", color);
    }

    const mColor = state.mastery >= state.masteryThresh ? "#33cc55" : "#cc8022";
    setBar("m-fill", state.mastery, 100, mColor);
    document.getElementById("m-label").textContent = `M ${state.mastery.toFixed(1)}`;

    const pColor = state.power >= state.powerThresh ? "#33cc55" : "#cc8022";
    setBar("p-fill", state.power, 100, pColor);
    document.getElementById("p-label").textContent = `P ${state.power.toFixed(1)}`;

    const dMax = state.goal > 0 ? state.goal : 100;
    setBar("d-fill", state.pts, dMax, state.goalMet ? "#00cc44" : "#3366cc");
    document.getElementById("d-label").textContent = `${state.pts} pts`;

    const streakEl = document.getElementById("streak-text");
    streakEl.textContent = (state.goalMet && state.streak > 0) ? `Streak: ${state.streak}` : "";
}

const SUGGESTIONS = {
    try_again:                           "Try again!",
    play_faster:                         "Try to play faster!",
    play_more_evenly:                    "Focus on an even tempo!",
    be_more_consistent:                  "Be more consistent!",
    already_mastered:                    "Excellent! Already mastered.",
    raise_quality_be_more_consistent:    "Good score! Play more consistently to grow mastery.",
    raise_quality_play_faster:           "Good score! Play faster to grow mastery.",
    raise_quality_play_more_evenly:      "Good score! Even up your tempo to grow mastery.",
    try_another_lesson:                  "Press Space for a new lesson.",
    no_lessons_available:                "No lessons found in seq/.",
    all_up_to_date:                      "All caught up! Come back later.",
    overdue:                             "Due for review.",
    needs_work:                          "Needs more practice.",
    new_lesson:                          "New lesson!",
};

function renderInfoBar() {
    const age = Date.now() - state.suggestionTime;
    const suggEl = document.getElementById("suggestion");
    if (state.suggestion && age < 3000) {
        suggEl.textContent = SUGGESTIONS[state.suggestion] || state.suggestion;
    } else {
        suggEl.textContent = "";
    }

    const hash = state.currentChunk || state.level0Hashes[state.currentLevel0Idx] || "";
    const chunkEl = document.getElementById("chunk-label");
    if (hash) {
        const lvl = state.currentChunkLevel >= 0 ? `${state.currentChunkLevel}:` : "";
        chunkEl.textContent = lvl + hash.slice(0, 8);
    } else {
        chunkEl.textContent = "";
    }
}

function updateKaraokeBtn() {
    document.getElementById("karaoke-btn").classList.toggle("active", state.karaokeOn);
}

function renderCelebration() {
    const el  = document.getElementById("celebration");
    const age = Date.now() - state.ptsDeltaTime;
    const DUR = 2000;
    if (state.ptsDelta <= 0 || age >= DUR) { el.style.display = "none"; return; }
    const t = age / DUR;
    el.style.display   = "block";
    el.style.opacity   = 1 - t;
    el.style.transform = `translateX(-50%) translateY(${-t * 40}px)`;
    el.style.color     = state.goalMet ? "#50ff78" : "#ffd228";
    el.textContent     = `+${state.ptsDelta} pts!`;
}

// ── keyboard shortcuts ─────────────────────────────────────────────────────────

document.addEventListener("keydown", (e) => {
    if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT") return;
    switch (e.key) {
        case "Escape": quitLesson();    break;
        case "x": case "X": quitLesson();    break;
        case "r": case "R": reloadLesson();  break;
        case " ":  e.preventDefault(); suggestLesson(); break;
        case "k": case "K": toggleKaraoke(); break;
    }
});

// ── synth ──────────────────────────────────────────────────────────────────────

const synth = new Synth();

function setSynthVolume(v) { synth.setVolume(parseFloat(v)); }

// ── karaoke ────────────────────────────────────────────────────────────────────

const karaoke = new Karaoke({
    getMidiOutput: () => midi.activeOutput,
    onDone: () => parseLine("KARAOKE_DONE"),
});

// ── MIDI + group integration ───────────────────────────────────────────────────

const group = new Group((groupLine) => {
    const resultLines = evaluateGroup(groupLine);
    for (const line of resultLines.split("\n")) if (line) {
        stats.handleLine(line);   // update stats first (finalize before loadChunk)
        parseLine(line);
    }
});

const midi = new MidiInput({
    onNoteOn(midiNote, _vel, t) { group.noteOn(midiNote, t); synth.noteOn(midiNote); },
    onNoteOff(midiNote, _t)     { group.noteOff(midiNote);  synth.noteOff(midiNote); },
    onDevicesChanged(inputs, outputs) {
        function repopulate(selId, devices, onSelect) {
            const sel  = document.getElementById(selId);
            const prev = sel.value;
            sel.innerHTML = '<option value="">—</option>';
            for (const dev of devices) {
                const opt = document.createElement("option");
                opt.value = dev.id; opt.textContent = dev.name;
                if (dev.id === prev) opt.selected = true;
                sel.appendChild(opt);
            }
            if (!prev && devices.length > 0) {
                sel.value = devices[0].id;
                onSelect(devices[0].id);
            }
        }
        repopulate("midi-in",  inputs,  (id) => midi.openInput(id));
        repopulate("midi-out", outputs, (id) => midi.openOutput(id));
    },
});

function selectMidiIn(id)  { midi.openInput(id); }
function selectMidiOut(id) { midi.openOutput(id); }
function setForward(checked) { midi.forward = checked; }

// ── all + stats ────────────────────────────────────────────────────────────────

const all = new All(parseLine);

const stats = new Stats(parseLine, (line) => {
    // Route QUERY_CHILDREN from stats.js to all.js (synchronous, answered from index)
    if (line.startsWith("QUERY_CHILDREN ")) all.queryChildren(line.split(" ")[1]);
});

// ── render loop ────────────────────────────────────────────────────────────────

function tick() {
    renderInfoBar();
    renderCelebration();
    requestAnimationFrame(tick);
}

// ── init ───────────────────────────────────────────────────────────────────────

async function init() {
    await synth.init();
    await midi.init();
    await all.init();           // loads index.json, emits CHUNK_NAME for all chunks
    stats.handleLine("SUGGEST_LESSON");
    requestAnimationFrame(tick);
}

init();
