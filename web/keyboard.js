// SPDX-License-Identifier: MIT
// keyboard.js — on-screen piano keyboard visualizer
// Copyright (c) 2026 Jakob Kastelic

const keyboard = (() => {
    const FIRST = 36; // C2
    const LAST  = 96; // C7
    const WHITE = new Set([0, 2, 4, 5, 7, 9, 11]);

    function isWhite(n) { return WHITE.has(n % 12); }

    // Build white key list and reverse index
    const whites = [];
    const wIdx   = {};   // midi note → position in whites[]
    for (let n = FIRST; n <= LAST; n++) {
        if (isWhite(n)) { wIdx[n] = whites.length; whites.push(n); }
    }

    let canvas = null, ctx = null;
    const active = new Set();

    function draw() {
        if (!canvas || !ctx) return;
        const dpr = window.devicePixelRatio || 1;
        const W   = canvas.width  / dpr;
        const H   = canvas.height / dpr;
        const ww  = W / whites.length;
        const bw  = ww * 0.60;
        const bh  = H  * 0.62;

        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.save();
        ctx.scale(dpr, dpr);

        // ── white keys ──
        for (let i = 0; i < whites.length; i++) {
            const n   = whites[i];
            const x   = i * ww;
            const hit = active.has(n);
            ctx.fillStyle   = hit ? "#99ddff" : "#eeeeee";
            ctx.fillRect(x + 0.5, 0.5, ww - 1.5, H - 1.5);
            ctx.strokeStyle = "#999";
            ctx.lineWidth   = 0.5;
            ctx.strokeRect(x + 0.5, 0.5, ww - 1.5, H - 1.5);

            // Label each C note
            if (n % 12 === 0) {
                const oct = Math.floor(n / 12) - 1;
                const fs  = Math.max(8, Math.min(13, ww * 0.55));
                ctx.fillStyle  = "#777";
                ctx.font       = `${fs}px monospace`;
                ctx.textAlign  = "center";
                ctx.fillText(`C${oct}`, x + ww / 2, H - 4);
            }
        }

        // ── black keys (drawn on top) ──
        for (let n = FIRST; n <= LAST; n++) {
            if (isWhite(n)) continue;
            const li = wIdx[n - 1]; // white key immediately to the left
            if (li === undefined) continue;
            const x   = (li + 1) * ww - bw / 2;
            const hit = active.has(n);

            // Body
            ctx.fillStyle = hit ? "#55aadd" : "#1c1c1c";
            ctx.fillRect(x, 0, bw, bh);

            // Subtle highlight stripe at the top
            ctx.fillStyle = hit ? "rgba(255,255,255,0.25)" : "rgba(255,255,255,0.12)";
            ctx.fillRect(x + 1, 0, bw - 2, Math.max(3, bh * 0.12));
        }

        ctx.restore();
    }

    function _resize() {
        if (!canvas) return;
        const dpr     = window.devicePixelRatio || 1;
        canvas.width  = canvas.offsetWidth  * dpr;
        canvas.height = canvas.offsetHeight * dpr;
        draw();
    }

    // ── hit-testing (black keys first, then white) ──

    function _hitTest(cx, cy) {
        const dpr = window.devicePixelRatio || 1;
        const W   = canvas.width  / dpr;
        const H   = canvas.height / dpr;
        const ww  = W / whites.length;
        const bw  = ww * 0.60;
        const bh  = H  * 0.62;

        // Black keys sit on top; check them first if y is in range
        if (cy < bh) {
            for (let n = FIRST; n <= LAST; n++) {
                if (isWhite(n)) continue;
                const li = wIdx[n - 1];
                if (li === undefined) continue;
                const x = (li + 1) * ww - bw / 2;
                if (cx >= x && cx < x + bw) return n;
            }
        }

        // White keys
        const wi = Math.floor(cx / ww);
        if (wi >= 0 && wi < whites.length) return whites[wi];
        return null;
    }

    // ── mouse / touch interaction ──

    let _mouseNote = null;
    let _onNoteOn  = null;
    let _onNoteOff = null;

    function _pointerPos(e) {
        const rect = canvas.getBoundingClientRect();
        const src  = e.touches ? e.touches[0] : e;
        return [src.clientX - rect.left, src.clientY - rect.top];
    }

    function _down(e) {
        e.preventDefault();
        const [cx, cy] = _pointerPos(e);
        const n = _hitTest(cx, cy);
        if (n !== null) {
            _mouseNote = n;
            if (_onNoteOn) _onNoteOn(n);
        }
    }

    function _move(e) {
        if (_mouseNote === null) return;
        const [cx, cy] = _pointerPos(e);
        const n = _hitTest(cx, cy);
        if (n !== _mouseNote) {
            if (_mouseNote !== null && _onNoteOff) _onNoteOff(_mouseNote);
            _mouseNote = n;
            if (n !== null && _onNoteOn) _onNoteOn(n);
        }
    }

    function _up() {
        if (_mouseNote !== null) {
            if (_onNoteOff) _onNoteOff(_mouseNote);
            _mouseNote = null;
        }
    }

    // ── public API ──

    function noteOn(n)  { if (n >= FIRST && n <= LAST) { active.add(n);    draw(); } }
    function noteOff(n) { active.delete(n); draw(); }
    function clearNotes() { active.clear(); draw(); }

    function init(panelEl, onNoteOn, onNoteOff) {
        _onNoteOn  = onNoteOn  || null;
        _onNoteOff = onNoteOff || null;

        if (!canvas) {
            canvas = document.createElement("canvas");
            canvas.style.cssText = "display:block;width:100%;height:100%;";
            panelEl.appendChild(canvas);
            ctx = canvas.getContext("2d");
            window.addEventListener("resize", _resize);

            // Mouse
            canvas.addEventListener("mousedown",  _down);
            canvas.addEventListener("mousemove",   _move);
            canvas.addEventListener("mouseup",     _up);
            canvas.addEventListener("mouseleave",  _up);
            // Touch
            canvas.addEventListener("touchstart",  _down, { passive: false });
            canvas.addEventListener("touchmove",   _move, { passive: false });
            canvas.addEventListener("touchend",    _up);
            canvas.addEventListener("touchcancel", _up);
        }
        // Panel may have just become visible — re-measure and redraw
        requestAnimationFrame(_resize);
    }

    return { noteOn, noteOff, clearNotes, init };
})();
