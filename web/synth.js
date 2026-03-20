// SPDX-License-Identifier: MIT
// synth.js --- dual-oscillator ADSR synth (port of synth.c) via Web Audio API
// Copyright (c) 2026 Jakob Kastelic
//
// noteOn(midiNote) / noteOff(midiNote) — called from app.js MIDI handlers.
// setVolume(0..1)  — 0 suspends the AudioContext entirely.

const _SYNTH_WORKLET = `
class SynthProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.voices = Array.from({length: 16}, () => ({
            freq: 0, targetF1: 0, targetF2: 0,
            phase1: 0, phase2: 0, lpf1: 0, lpf2: 0,
            state: 'idle', envVol: 0, active: false,
        }));
        // Defaults match synth.c
        this.p = {
            osc1Gain: 0.8,  osc2Gain: 0.5,
            osc1Cutoff: 0.15, osc2Cutoff: 0.08,
            osc1Detune: 1.0,  osc2Detune: 1.004,
            osc1Oct: 0,     osc2Oct: -1,
            attack: 0.01, decay: 0.1, sustain: 0.7, release: 0.05,
            masterGain: 0.3,
        };
        this.port.onmessage = ({data}) => this._handle(data);
    }

    _handle(msg) {
        const p = this.p;
        if (msg.type === 'noteOn') {
            for (const v of this.voices) {
                if (v.state === 'idle') {
                    v.freq     = msg.freq;
                    v.targetF1 = msg.freq * Math.pow(2, p.osc1Oct) * p.osc1Detune;
                    v.targetF2 = msg.freq * Math.pow(2, p.osc2Oct) * p.osc2Detune;
                    v.phase1 = v.phase2 = v.lpf1 = v.lpf2 = 0;
                    v.envVol = 0; v.state = 'attack'; v.active = true;
                    break;
                }
            }
        } else if (msg.type === 'noteOff') {
            for (const v of this.voices)
                if (v.active && Math.abs(v.freq - msg.freq) < 0.1)
                    v.state = 'release';
        } else if (msg.type === 'set') {
            p[msg.param] = msg.value;
            if (['osc1Oct','osc2Oct','osc1Detune','osc2Detune'].includes(msg.param))
                for (const v of this.voices) if (v.active) {
                    v.targetF1 = v.freq * Math.pow(2, p.osc1Oct) * p.osc1Detune;
                    v.targetF2 = v.freq * Math.pow(2, p.osc2Oct) * p.osc2Detune;
                }
        }
    }

    process(inputs, outputs) {
        const p  = this.p;
        const sr = sampleRate;
        const L  = outputs[0][0];
        const R  = outputs[0][1];
        const aStep = 1 / (p.attack  * sr);
        const dStep = (1 - p.sustain) / (p.decay   * sr);
        const rStep = p.sustain       / (p.release  * sr);

        for (let i = 0; i < L.length; i++) {
            let mixed = 0;
            for (const v of this.voices) {
                if (!v.active) continue;

                // ADSR envelope
                if      (v.state === 'attack')  { v.envVol += aStep; if (v.envVol >= 1)        { v.envVol = 1;         v.state = 'decay';   } }
                else if (v.state === 'decay')   { v.envVol -= dStep; if (v.envVol <= p.sustain) { v.envVol = p.sustain; v.state = 'sustain'; } }
                else if (v.state === 'release') { v.envVol -= rStep; if (v.envVol <= 0)         { v.envVol = 0; v.state = 'idle'; v.active = false; } }

                // Osc 1: square wave + first-order LPF
                v.phase1 += v.targetF1 / sr; if (v.phase1 >= 1) v.phase1 -= 1;
                const s1 = v.phase1 < 0.5 ? p.osc1Gain : -p.osc1Gain;
                v.lpf1  += p.osc1Cutoff * (s1 - v.lpf1);

                // Osc 2: square wave + first-order LPF
                v.phase2 += v.targetF2 / sr; if (v.phase2 >= 1) v.phase2 -= 1;
                const s2 = v.phase2 < 0.5 ? p.osc2Gain : -p.osc2Gain;
                v.lpf2  += p.osc2Cutoff * (s2 - v.lpf2);

                mixed += (v.lpf1 + v.lpf2) * v.envVol;
            }
            L[i] = R[i] = (mixed / 4.0) * p.masterGain;
        }
        return true;
    }
}
registerProcessor('synth-processor', SynthProcessor);
`;

class Synth {
    constructor() {
        this.ctx      = null;
        this.node     = null;
        this._freqMap = new Map(); // midiNote → freq, for noteOff matching
    }

    async init() {
        this.ctx = new AudioContext({ sampleRate: 48000 });
        const blob = new Blob([_SYNTH_WORKLET], { type: "application/javascript" });
        await this.ctx.audioWorklet.addModule(URL.createObjectURL(blob));
        this.node = new AudioWorkletNode(this.ctx, "synth-processor", {
            numberOfOutputs: 1,
            outputChannelCount: [2],
        });
        this.node.connect(this.ctx.destination);
        await this.ctx.suspend(); // silent until setVolume() > 0
    }

    setVolume(v) {
        if (!this.ctx) return;
        if (v <= 0) {
            this.ctx.suspend();
        } else {
            this._send({ type: "set", param: "masterGain", value: v });
            this.ctx.resume();
        }
    }

    noteOn(midiNote) {
        if (!this.ctx || this.ctx.state !== "running") return;
        const freq = 440 * Math.pow(2, (midiNote - 69) / 12);
        this._freqMap.set(midiNote, freq);
        this._send({ type: "noteOn", freq });
    }

    noteOff(midiNote) {
        const freq = this._freqMap.get(midiNote);
        if (freq === undefined) return;
        this._freqMap.delete(midiNote);
        this._send({ type: "noteOff", freq });
    }

    _send(msg) { if (this.node) this.node.port.postMessage(msg); }
}
