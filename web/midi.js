// SPDX-License-Identifier: MIT
// midi.js --- Web MIDI API wrapper (replaces bin/midi)
// Copyright (c) 2026 Jakob Kastelic
//
// onNoteOn / onNoteOff receive raw MIDI note numbers (0-127).
// onDevicesChanged(inputs, outputs) receives arrays of MIDIInput / MIDIOutput.

class MidiInput {
    constructor({ onNoteOn, onNoteOff, onDevicesChanged }) {
        this.onNoteOn        = onNoteOn;
        this.onNoteOff       = onNoteOff;
        this.onDevicesChanged = onDevicesChanged;
        this.access          = null;
        this.activeInput     = null;
        this.activeOutput    = null;
        this.forward         = false;
    }

    async init() {
        if (!navigator.requestMIDIAccess) {
            console.warn("Web MIDI API not supported in this browser");
            return false;
        }
        try {
            this.access = await navigator.requestMIDIAccess({ sysex: false });
            this.access.onstatechange = () => this._refresh();
            this._refresh();
            return true;
        } catch (e) {
            console.warn("MIDI access denied:", e);
            return false;
        }
    }

    _refresh() {
        const inputs  = [...this.access.inputs.values()];
        const outputs = [...this.access.outputs.values()];
        this.onDevicesChanged(inputs, outputs);
    }

    openInput(id) {
        if (this.activeInput) this.activeInput.onmidimessage = null;
        this.activeInput = id ? this.access.inputs.get(id) : null;
        if (this.activeInput) this.activeInput.onmidimessage = (e) => this._handle(e);
    }

    openOutput(id) {
        this.activeOutput = id ? this.access.outputs.get(id) : null;
    }

    _handle(event) {
        const [status, note, velocity] = event.data;
        const type = status & 0xf0;
        const t    = Date.now();

        if (type === 0x90 && velocity > 0) {
            this.onNoteOn(note, velocity, t);
        } else if (type === 0x80 || (type === 0x90 && velocity === 0)) {
            this.onNoteOff(note, t);
        }

        if (this.forward && this.activeOutput) {
            this.activeOutput.send(event.data);
        }
    }
}
