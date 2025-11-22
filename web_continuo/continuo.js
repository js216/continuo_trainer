const CONFIG = {
   BEAT_SPACING_PX: 60,
   NOTE_HEAD_WIDTH: 14,
   NOTE_HEAD_HEIGHT: 9,
   STEM_HEIGHT: 35,
   LINE_SPACING: 10,
   KEY_WIDTH: 40,
   MUSIC_VERT_OFFS: 110,
   SCORE_WEIGHTS: { // NEW: Scoring criteria weights
      CORRECT_NOTE: 10,
      INCORRECT_NOTE: -10, // Can go negative
      TIMING_PERFECT: 5,   // Bonus for hitting the beat (within TIMING_WINDOW_MS)
      TIMING_OFF: -5,      // Penalty for missing the window
      TIMING_WINDOW_MS: 150 // Acceptable time variance for perfect timing
   }
};

const DURATION_MAP = {
   0: 8,   // Breve (Double Whole)
   1: 4,   // Whole
   1.5: 6, // Dotted Whole
   2: 2,   // Half
   4: 1,   // Quarter
   8: 0.5  // Eighth
};

const KEY_SCALES = {
   "-7": ['Cb', 'Db', 'Eb', 'Fb', 'Gb', 'Ab', 'Bb'],
   "-6": ['Gb', 'Ab', 'Bb', 'Cb', 'Db', 'Eb', 'F'],
   "-5": ['Db', 'Eb', 'F', 'Gb', 'Ab', 'Bb', 'C'],
   "-4": ['Ab', 'Bb', 'C', 'Db', 'Eb', 'F', 'G'],
   "-3": ['Eb', 'F', 'G', 'Ab', 'Bb', 'C', 'D'],
   "-2": ['Bb', 'C', 'D', 'Eb', 'F', 'G', 'A'],
   "-1": ['F', 'G', 'A', 'Bb', 'C', 'D', 'E'],
   "0":  ['C', 'D', 'E', 'F', 'G', 'A', 'B'],
   "1":  ['G', 'A', 'B', 'C', 'D', 'E', 'F#'],
   "2":  ['D', 'E', 'F#', 'G', 'A', 'B', 'C#'],
   "3":  ['A', 'B', 'C#', 'D', 'E', 'F#', 'G#'],
   "4":  ['E', 'F#', 'G#', 'A', 'B', 'C#', 'D#'],
   "5":  ['B', 'C#', 'D#', 'E', 'F#', 'G#', 'A#'],
   "6":  ['F#', 'G#', 'A#', 'B', 'C#', 'D#', 'E#'],
   "7":  ['C#', 'D#', 'E#', 'F#', 'G#', 'A#', 'B#']
};

const LESSONS = [
   {
      name: "Lesson 1: Root Position Triads",
      description: "Play the 3rd and 5th above the bass.",
      defaultKey: 0,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      tempo: 160, // BPM
      sequence: [
         { bass: "C3", figure: "6/3", duration: 2, correctAnswer: ["E4", "G4"] },
         { bass: "G3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "E3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "A3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "F3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "C4", figure: "", duration: 2, correctAnswer: [] },
         { bass: "A3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "F3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "G3", figure: "6/3", duration: 2, correctAnswer: ["B4", "D5"] },
         { bass: "A3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "F3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "G3", figure: "", duration: 2, correctAnswer: [] },
         { bass: "C3", figure: "", duration: 1, correctAnswer: [] }
      ]
   },
   {
      name: "Lesson 2: Full Measure Triads",
      description: "Play the 3rd and 5th above the bass.",
      defaultKey: 1,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      tempo: 72, // BPM
      sequence: [
         { bass: "G2", figure: "", duration: 4, correctAnswer: ["B3", "D4"] },
         { bass: "G3", figure: "", duration: 4, correctAnswer: ["B3", "D4"] },
         { bass: "E3", figure: "", duration: 4, correctAnswer: ["G3", "B3"] },
         { bass: "C3", figure: "", duration: 4, correctAnswer: ["E3", "G3"] },
         { bass: "D3", figure: "", duration: 4, correctAnswer: ["F#3", "A3"] }
      ]
   },
   {
      name: "Lesson 3: Eighth Notes",
      description: "Play the 3rd and 5th above the bass. (Note the beams)",
      defaultKey: -1,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      tempo: 90, // BPM
      sequence: [
         { bass: "F3", figure: "", duration: 8, correctAnswer: [] },
         { bass: "G3", figure: "", duration: 8, correctAnswer: [] },
         { bass: "F3", figure: "", duration: 8, correctAnswer: [] },
         { bass: "E3", figure: "", duration: 8, correctAnswer: [] },
         { bass: "D3", figure: "", duration: 8, correctAnswer: [] },
         { bass: "A3", figure: "", duration: 8, correctAnswer: [] },
         { bass: "D4", figure: "", duration: 8, correctAnswer: [] },
         { bass: "C4", figure: "", duration: 8, correctAnswer: ["E4", "G4"] }
      ]
   },
   {
      name: "Lesson 4: Half notes",
      description: "The figure '#' raises the 7th note.",
      defaultKey: -1,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      tempo: 60, // BPM
      sequence: [
         { bass: "C3", figure: "", duration: 2, correctAnswer: ["E3", "G3"] },
         { bass: "G2", figure: "7", duration: 2, correctAnswer: ["B2", "D3", "F3"] },
         { bass: "C3", figure: "", duration: 1.5, correctAnswer: ["E3", "G3"] },
         { bass: "C3", figure: "", duration: 1, correctAnswer: [] }
      ]
   }
];


// --- AUTH & LOGGING ---

class SessionManager {
   constructor() {
      this.userId = localStorage.getItem('continuo_user_id');
      this.eventLog = [];
      this.currentLessonId = null;

      this.initAuth();
   }

   initAuth() {
      if (!this.userId) {
         this.userId = 'user-' + Math.random().toString(36).substr(2, 6);
         localStorage.setItem('continuo_user_id', this.userId);
         this.registerUser(this.userId);
      }
      this.updateDisplay();

      document.getElementById('loginBtn').addEventListener('click', () => {
         const input = prompt("Enter User ID (or leave empty for new):");
         if (input !== null) {
            if (input.trim() === "") {
               this.userId = 'user-' + Math.random().toString(36).substr(2, 6);
            } else {
               this.userId = input.trim();
            }
            localStorage.setItem('continuo_user_id', this.userId);
            this.registerUser(this.userId);
            this.updateDisplay();
         }
      });
   }

   updateDisplay() {
      document.getElementById('userDisplay').textContent = this.userId;
   }

   registerUser(uid) {
      fetch('/api/login', {
         method: 'POST',
         headers: {'Content-Type': 'application/json'},
         body: JSON.stringify({ userId: uid })
      }).catch(e => console.error("Login sync failed", e));
   }

   startLesson(lessonId) {
      this.currentLessonId = lessonId;
      this.eventLog = [];
   }

   logEvent(timestamp, type, data) {
      this.eventLog.push({ t: Math.floor(timestamp), type, ...data });
   }

   // Modified to return a Promise
   flushLog(totalDuration, score, tempo) {
      if (this.eventLog.length === 0) return Promise.resolve();

      const payload = {
         userId: this.userId,
         lessonId: this.currentLessonId,
         totalDuration: Math.floor(totalDuration),
         score: score,
         tempo: tempo, // NEW: Send tempo to backend
         events: this.eventLog
      };

      console.log("Saving log...", payload);

      return fetch('/api/log', {
         method: 'POST',
         headers: {'Content-Type': 'application/json'},
         body: JSON.stringify(payload)
      }).then(res => res.json())
        .catch(e => console.error("Logging failed", e));
   }
}

// --- ENGINE ---

class AudioEngine {
   constructor() {
      this.ctx = null;
      this.masterGain = null;
   }

   init() {
      if (!this.ctx) {
         this.ctx = new (window.AudioContext || window.webkitAudioContext)();
         this.masterGain = this.ctx.createGain();
         this.masterGain.gain.value = 0.3;
         this.masterGain.connect(this.ctx.destination);
      }
      if (this.ctx.state === 'suspended') {
         this.ctx.resume();
      }
   }

   playNote(note, durationSec = 0.5, type = 'triangle') {
      if (!this.ctx) this.init();
      const freq = this.noteToFreq(note);

      // Helper to create and trigger an oscillator
      const triggerOsc = (waveType, gainLevel) => {
          const osc = this.ctx.createOscillator();
          const gain = this.ctx.createGain();
          osc.type = waveType;
          osc.frequency.value = freq;

          const now = this.ctx.currentTime;
          gain.gain.setValueAtTime(0, now);
          gain.gain.linearRampToValueAtTime(gainLevel, now + 0.02); // Attack
          gain.gain.exponentialRampToValueAtTime(0.001, now + durationSec); // Decay

          osc.connect(gain);
          gain.connect(this.masterGain);
          osc.start();
          osc.stop(now + durationSec);
      };

      // 1. Main Tone (Triangle)
      triggerOsc(type, 1.0);

      // 2. Smooth Harmonic Reinforcement for Bass
      // Instead of a hard cutoff, we fade the sawtooth layer in.

      const highCutoff = 1000;
      const lowCutoff = 100;
      const maxSawGain = 0.15;

      if (freq < highCutoff) {
         let blend = 1.0;

         if (freq > lowCutoff) {
             // Linear interpolation: 0.0 at highCutoff, 1.0 at lowCutoff
             blend = (highCutoff - freq) / (highCutoff - lowCutoff);
         }

         const sawGain = maxSawGain * blend;
         if (sawGain > 0.01) {
             triggerOsc('sawtooth', sawGain);
         }
      }
   }

   noteToFreq(note) {
      const noteMap = { 'C': -9, 'D': -7, 'E': -5, 'F': -4, 'G': -2, 'A': 0, 'B': 2 };
      const letter = note.replace(/[\d#b-]+/, '');
      const acc = note.includes('#') ? 1 : (note.includes('b') ? -1 : 0);
      const octave = parseInt(note.match(/-?\d+/)[0]);
      const semitones = (octave - 4) * 12 + noteMap[letter] + acc;
      return 440 * Math.pow(2, semitones / 12);
   }
}

class CanvasRenderer {
   constructor(canvas) {
      this.canvas = canvas;
      this.ctx = canvas.getContext("2d");
      this.staffTop = CONFIG.MUSIC_VERT_OFFS;
      this.lineSpacing = CONFIG.LINE_SPACING;
      this.numLines = 5;
      this.staffGap = 90;
      this.currentKeySig = 0;
      this.viewportOffsetX = 0;
   }
   
   get inkColor() {
       return document.body.getAttribute('data-theme') === 'dark' ? '#f3f4f6' : '#000000';
   }

   resize() {
      if (!this.canvas.parentElement) return;
      const rect = this.canvas.parentElement.getBoundingClientRect();
      this.canvas.width = rect.width;
      this.canvas.height = 340;
   }

   clear() {
      this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
   }

   getDiatonicRank(note) {
      const noteName = note.replace(/[\d#b]+/, '');
      const octave = parseInt(note.match(/-?\d+/)[0]);
      const ranks = { 'C': 0, 'D': 1, 'E': 2, 'F': 3, 'G': 4, 'A': 5, 'B': 6 };
      return (octave * 7) + ranks[noteName];
   }

   getNoteY(note, clef) {
      const trebleRefRank = this.getDiatonicRank("B4");
      const bassRefRank = this.getDiatonicRank("D3");
      const currentRank = this.getDiatonicRank(note);

      let baseLineY;
      let rankDiff;

      if (clef === "treble") {
         baseLineY = this.staffTop + 2 * this.lineSpacing;
         rankDiff = currentRank - trebleRefRank;
      } else {
         baseLineY = this.staffTop + this.staffGap + 2 * this.lineSpacing;
         rankDiff = currentRank - bassRefRank;
      }
      return baseLineY - (rankDiff * (this.lineSpacing / 2));
   }

   drawStaves() {
      const ctx = this.ctx;
      ctx.strokeStyle = this.inkColor;
      ctx.lineWidth = 1;
      const width = this.canvas.width;
      const lineHorizOffset = 10;

      for (let i = 0; i < this.numLines; i++) {
         const y = this.staffTop + i * this.lineSpacing;
         ctx.beginPath(); ctx.moveTo(lineHorizOffset, y); ctx.lineTo(width, y); ctx.stroke();
      }

      const bassTop = this.staffTop + this.staffGap;
      for (let i = 0; i < this.numLines; i++) {
         const y = bassTop + i * this.lineSpacing;
         ctx.beginPath(); ctx.moveTo(lineHorizOffset, y); ctx.lineTo(width, y); ctx.stroke();
      }

      ctx.beginPath();
      ctx.moveTo(lineHorizOffset, this.staffTop);
      ctx.lineTo(lineHorizOffset, bassTop + (this.numLines - 1) * this.lineSpacing);
      ctx.lineWidth = 1;
      ctx.stroke();

      this.drawTrebleClef(lineHorizOffset + 30, this.staffTop);
      this.drawBassClef(lineHorizOffset + 30, bassTop);
   }

   drawTrebleClef(x, staffTopY) {
      const ctx = this.ctx;
      ctx.strokeStyle = this.inkColor;
      ctx.fillStyle = this.inkColor;
      
      const S = this.lineSpacing;
      const anchorY = staffTopY + (3 * S);
      ctx.save();
      ctx.translate(x, anchorY);
      const k = S * 0.72;
      ctx.scale(k, k);
      ctx.lineWidth = 2.4 / k;
      ctx.lineCap = "round";
      ctx.lineJoin = "round";
      ctx.beginPath();
      ctx.moveTo(0.00, 0.00);
      ctx.bezierCurveTo( 1.2,  0.2,  1.6, -1.1,  0.3, -1.6);
      ctx.bezierCurveTo(-1.4, -2.2, -2.2,  0.4, -0.4,  1.4);
      ctx.bezierCurveTo( 1.8,  2.5,  3.3, -1.8,  0.6, -3.6);
      ctx.bezierCurveTo(-2.0, -5.4, -1.0, -6.9,  0.4, -6.2);
      ctx.bezierCurveTo( 1.4, -5.7,  1.6, -4.4,  0.2, -3.6);
      ctx.lineTo( -0.3, 3.5 );
      ctx.bezierCurveTo(-0.4,  4.6, -1.7,  4.8, -2.0,  3.9);
      ctx.bezierCurveTo(-2.3,  2.8, -1.2,  2.5, -0.4,  3.1);
      ctx.stroke();
      ctx.beginPath(); ctx.arc(-1.6, 3.8, 0.35, 0, Math.PI * 2); ctx.fill();
      ctx.restore();
   }

   drawBassClef(x, staffTopY) {
      const ctx = this.ctx;
      ctx.strokeStyle = this.inkColor;
      ctx.fillStyle = this.inkColor;
      
      const S = this.lineSpacing;
      const anchorY = staffTopY + (1 * S);
      ctx.save();
      ctx.translate(x, anchorY);
      const drawScale = S * 0.75;
      ctx.scale(drawScale, drawScale);
      ctx.beginPath(); ctx.arc(0, 0, 0.6, 0, Math.PI * 2); ctx.fill();
      ctx.beginPath(); ctx.moveTo(0, 0);
      ctx.bezierCurveTo(1.5, -2.5, 3.5, -1.0, 3.0, 1.5); 
      ctx.bezierCurveTo(2.8, 3.0, 1.0, 3.5, 0.5, 3.0);
      ctx.lineWidth = 2.5 / drawScale;
      ctx.stroke();
      ctx.beginPath(); ctx.arc(4.5, -0.7, 0.3, 0, Math.PI * 2); ctx.arc(4.5, 0.7, 0.3, 0, Math.PI * 2); ctx.fill();
      ctx.restore();
   }

   drawAccidental(type, x, y) {
      const ctx = this.ctx;
      ctx.strokeStyle = this.inkColor; // Ensure accidental uses ink color
      const size = 6;
      ctx.beginPath();
      ctx.lineWidth = 1.5;

      if (type === '#' || type === '♯') {
         ctx.moveTo(x - 2, y - size); ctx.lineTo(x - 2, y + size);
         ctx.moveTo(x + 2, y - size); ctx.lineTo(x + 2, y + size);
         ctx.moveTo(x - 5, y + 2); ctx.lineTo(x + 5, y - 1);
         ctx.moveTo(x - 5, y - 1); ctx.lineTo(x + 5, y - 4);
      } 
      else if (type === 'b' || type === '♭') {
         ctx.moveTo(x - 3, y - 10); ctx.lineTo(x - 3, y + 4); 
         ctx.bezierCurveTo(x, y + 6, x + 5, y + 2, x - 3, y - 2); 
      }
      else if (type === 'n' || type === '♮') {
         ctx.moveTo(x - 2, y - 8); ctx.lineTo(x - 2, y + 4);
         ctx.moveTo(x + 2, y - 4); ctx.lineTo(x + 2, y + 8);
         ctx.moveTo(x - 2, y); ctx.lineTo(x + 2, y - 2);
         ctx.moveTo(x - 2, y + 4); ctx.lineTo(x + 2, y + 2);
      }
      ctx.stroke();
   }

   drawKeySignature(num) {
      // Accidentals set their own color, so loop is fine
      const ctx = this.ctx;
      const isSharp = num > 0;
      const count = Math.abs(num);
      const symbol = isSharp ? "#" : "b";
      const xStart = 90;
      const sharpNotesT = ['F5','C5','G5','D5','A4','E5','B4'];
      const sharpNotesB = ['F3','C3','G3','D3','A2','E3','B2'];
      const flatNotesT = ['B4','E5','A4','D5','G4','C5','F4'];
      const flatNotesB = ['B2','E3','A2','D3','G2','C3','F2'];
      const targetT = isSharp ? sharpNotesT : flatNotesT;
      const targetB = isSharp ? sharpNotesB : flatNotesB;

      for(let i=0; i<count; i++) {
         const x = xStart + (i * 12);
         this.drawAccidental(symbol, x, this.getNoteY(targetT[i], 'treble'));
         this.drawAccidental(symbol, x, this.getNoteY(targetB[i], 'bass'));
      }
   }

   drawTimeSignature(timeSig, keySigCount) {
      if (!timeSig) return 100;
      const [num, den] = timeSig;
      const x = 80 + (Math.abs(keySigCount) * 12) + 25;
      const ctx = this.ctx;

      ctx.font = `bold ${this.lineSpacing * 2}px serif`;
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillStyle = this.inkColor;

      const trebleMidY = this.staffTop + (2 * this.lineSpacing);
      const bassMidY = this.staffTop + this.staffGap + (2 * this.lineSpacing);
      const offset = this.lineSpacing;

      ctx.fillText(num, x, trebleMidY - offset);
      ctx.fillText(den, x, trebleMidY + offset);
      ctx.fillText(num, x, bassMidY - offset);
      ctx.fillText(den, x, bassMidY + offset);

      return x + 50; 
   }

   drawTempoMark(tempo) {
      const ctx = this.ctx;
      ctx.font = "bold 14px sans-serif";
      ctx.fillStyle = this.inkColor;
      ctx.textAlign = "left";
      const bpm = Math.round(tempo); 
      ctx.fillText(`♩ = ${bpm}`, 20, this.staffTop - 15);
   }

   drawEndBarline(x) {
      const ctx = this.ctx;
      ctx.strokeStyle = this.inkColor;
      const topY = this.staffTop;
      const botY = this.staffTop + this.staffGap + (this.numLines - 1) * this.lineSpacing;
      ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(x - 6, topY); ctx.lineTo(x - 6, botY); ctx.stroke();
      ctx.lineWidth = 4;
      ctx.beginPath(); ctx.moveTo(x, topY); ctx.lineTo(x, botY); ctx.stroke();
      ctx.lineWidth = 1;
   }

   drawBarLine(x) {
      const ctx = this.ctx;
      ctx.strokeStyle = this.inkColor;
      ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(x, this.staffTop); ctx.lineTo(x, this.staffTop + this.staffGap + (this.numLines-1)*this.lineSpacing); ctx.stroke();
   }

   drawNoteHead(x, y, isHollow) {
      const ctx = this.ctx;
      ctx.save();
      ctx.translate(x, y);
      ctx.rotate(-20 * Math.PI / 180);
      ctx.beginPath();
      ctx.ellipse(0, 0, CONFIG.NOTE_HEAD_WIDTH/2, CONFIG.NOTE_HEAD_HEIGHT/2, 0, 0, 2 * Math.PI);
      if (isHollow) { ctx.lineWidth = 2; ctx.stroke(); } else { ctx.fill(); }
      ctx.restore();
   }

   drawSingleFlag(x, stemTopY, stemBotY, isUp) {
      const ctx = this.ctx;
      const endY = isUp ? stemTopY : stemBotY;
      ctx.beginPath(); ctx.moveTo(x, endY);
      if (isUp) { ctx.bezierCurveTo(x + 8, endY + 10, x + 8, endY + 25, x, endY + 40); } 
      else { ctx.bezierCurveTo(x + 8, endY - 10, x + 8, endY - 25, x, endY - 40); }
      ctx.stroke();
   }

   drawNoteAt(note, x, clef, color = "black", durationCode = 4, alpha = 1.0, noStem = false) {
      const drawX = x - this.viewportOffsetX;
      if (drawX > this.canvas.width + 50) return; 

      const y = this.getNoteY(note, clef);
      const ctx = this.ctx;
      ctx.globalAlpha = alpha;
      
      // Dynamic color mapping: If "black", use theme ink. If colored (red/green/blue), keep as is.
      const effectiveColor = (color === "black" || color === "#000") ? this.inkColor : color;
      
      ctx.fillStyle = effectiveColor;
      ctx.strokeStyle = effectiveColor;

      const isHollow = (durationCode <= 2); 
      this.drawNoteHead(drawX, y, isHollow);

      if (durationCode === 1.5) {
         ctx.beginPath(); ctx.arc(drawX + 12, y, 2, 0, Math.PI*2); ctx.fill();
      }

      if (durationCode >= 1.5 && durationCode !== 1 && !noStem) {
         const middleLineY = (clef === "treble") ? (this.staffTop + 2 * this.lineSpacing) : (this.staffTop + this.staffGap + 2 * this.lineSpacing);
         const isUp = y >= middleLineY; 
         const stemLen = CONFIG.STEM_HEIGHT;
         const stemX = drawX + (isUp ? (CONFIG.NOTE_HEAD_WIDTH/2 - 1) : -(CONFIG.NOTE_HEAD_WIDTH/2 - 1));
         const stemStart = y + (isUp ? -2 : 2);
         const stemEnd = stemStart + (isUp ? -stemLen : stemLen);
         ctx.lineWidth = 1.5; ctx.beginPath(); ctx.moveTo(stemX, stemStart); ctx.lineTo(stemX, stemEnd); ctx.stroke();
         if (durationCode === 8) { this.drawSingleFlag(stemX, stemEnd, stemEnd, isUp); }
      }

      const accidental = this.getAccidentalSymbol(note, this.currentKeySig);
      if (accidental) {
         this.drawAccidental(accidental, drawX - 18, y);
      }

      this.drawLedgerLines(drawX, y, clef);
      ctx.globalAlpha = 1.0;
      return { x: drawX, y: y }; 
   }

   drawLedgerLines(x, y, clef) {
      const ctx = this.ctx;
      const topLineY = clef === "treble" ? this.staffTop : this.staffTop + this.staffGap;
      const bottomLineY = topLineY + (this.numLines - 1) * this.lineSpacing;
      const ledgerWidth = 24;
      ctx.lineWidth = 1;
      ctx.strokeStyle = this.inkColor; 
      if (y <= topLineY - this.lineSpacing) {
         for (let ly = topLineY - this.lineSpacing; ly >= y - 2; ly -= this.lineSpacing) {
            ctx.beginPath(); ctx.moveTo(x - ledgerWidth/2, ly); ctx.lineTo(x + ledgerWidth/2, ly); ctx.stroke();
         }
      }
      if (y >= bottomLineY + this.lineSpacing) {
         for (let ly = bottomLineY + this.lineSpacing; ly <= y + 2; ly += this.lineSpacing) {
            ctx.beginPath(); ctx.moveTo(x - ledgerWidth/2, ly); ctx.lineTo(x + ledgerWidth/2, ly); ctx.stroke();
         }
      }
   }

   // ... getAccidentalSymbol same as previous ...
   getAccidentalSymbol(note, keySig) {
      const letter = note.replace(/[\d#b]+/, '');
      const acc = note.includes('#') ? '#' : (note.includes('b') ? 'b' : '');
      const scale = KEY_SCALES[keySig.toString()];
      
      let keyAcc = '';
      const keyNote = scale.find(n => n.startsWith(letter));
      if (keyNote) {
          keyAcc = keyNote.includes('#') ? '#' : (keyNote.includes('b') ? 'b' : '');
      }

      if (acc === keyAcc) return null;
      if (acc === '#' && keyAcc !== '#') return '#';
      if (acc === 'b' && keyAcc !== 'b') return 'b';
      if (acc === '' && keyAcc !== '') return 'n';
      return null;
   }

   drawFigure(figure, x, note, clef) {
      const drawX = x - this.viewportOffsetX;
      if (drawX > this.canvas.width + 50) return;
      const y = this.getNoteY(note, clef) + 45;
      this.ctx.font = `bold ${this.lineSpacing * 1.4}px sans-serif`;
      this.ctx.textAlign = "center";
      this.ctx.fillStyle = this.inkColor;
      this.ctx.fillText(figure, drawX, y);
   }

   renderLessonState(lesson, currentStepIndex, userHistory, currentHeldNotes, correctionNotes) {
      this.currentKeySig = lesson.defaultKey;
      this.clear();

      this.drawStaves(); 
      this.drawKeySignature(lesson.defaultKey);
      this.drawTempoMark(lesson.tempo);
      const contentStartX = this.drawTimeSignature(lesson.timeSignature, lesson.defaultKey);

      this.ctx.save();
      this.ctx.beginPath();
      this.ctx.rect(contentStartX - 20, 0, this.canvas.width - contentStartX + 20, this.canvas.height);
      this.ctx.clip();

      let cursorX = contentStartX;
      let currentMeasureBeats = 0;
      const fullMeasureBeats = lesson.timeSignature[0];
      let measureTarget = lesson.anacrusisBeats || fullMeasureBeats;
      let activeNoteX = 0;

      const noteObjects = lesson.sequence.map((step, index) => {
         const beats = DURATION_MAP[step.duration];
         const width = beats * CONFIG.BEAT_SPACING_PX;
         const pos = cursorX + (width / 2);

         if (index === currentStepIndex) activeNoteX = pos;
         cursorX += width;

         currentMeasureBeats += beats;
         let isBarline = false;

         if (Math.abs(currentMeasureBeats - measureTarget) < 0.01 || currentMeasureBeats > measureTarget) {
            isBarline = true;
            currentMeasureBeats = 0;
            measureTarget = fullMeasureBeats;
         }

         const y = this.getNoteY(step.bass, 'bass');
         const midLine = this.staffTop + this.staffGap + 2 * this.lineSpacing;
         const preferredStemDir = y >= midLine ? 'up' : 'down'; 

         return {
            ...step,
            x: pos,
            width: width,
            y: y,
            index: index,
            barlineX: isBarline ? cursorX : null,
            stemDir: preferredStemDir,
            beats: beats
         };
      });

      const beams = [];
      let currentGroup = [];
      const flushGroup = () => {
         if (currentGroup.length > 1) beams.push([...currentGroup]);
         currentGroup = [];
      };

      noteObjects.forEach((note) => {
         if (note.duration === 8) { 
            currentGroup.push(note);
            if (currentGroup.length === 4 || note.barlineX) flushGroup();
         } else { flushGroup(); }
      });
      flushGroup();

      if (currentStepIndex < noteObjects.length) {
         const step = noteObjects[currentStepIndex];
         const drawX = step.x - (step.width/2) - this.viewportOffsetX;
         if (drawX + step.width > 0 && drawX < this.canvas.width) {
            this.ctx.fillStyle = document.body.getAttribute('data-theme') === 'dark' 
               ? "rgba(255, 255, 0, 0.15)" 
               : "rgba(255, 255, 0, 0.2)";
            this.ctx.fillRect(drawX, 0, step.width, this.canvas.height);
         }
      }

      const ctx = this.ctx;
      const beamedIndices = new Set(beams.flat().map(n => n.index));

      noteObjects.forEach(note => {
         if (!beamedIndices.has(note.index)) {
            this.drawNoteAt(note.bass, note.x, "bass", "black", note.duration);
         }
         this.drawFigure(note.figure, note.x, note.bass, "bass");

         if (note.barlineX) {
            if (note.index === noteObjects.length - 1) {
               this.drawEndBarline(note.barlineX - this.viewportOffsetX);
            } else {
               this.drawBarLine(note.barlineX - this.viewportOffsetX);
            }
         }
      });

      beams.forEach(group => {
         const upCount = group.filter(n => n.stemDir === 'up').length;
         const isUp = upCount >= group.length / 2; 

         const noteContexts = group.map(note => {
            const drawX = note.x - this.viewportOffsetX;
            const stemOffsetX = isUp ? (CONFIG.NOTE_HEAD_WIDTH/2 - 1) : -(CONFIG.NOTE_HEAD_WIDTH/2 - 1);
            return { note: note, drawX: drawX, stemX: drawX + stemOffsetX, y: note.y };
         });

         const startCtx = noteContexts[0];
         const endCtx = noteContexts[noteContexts.length - 1];
         if (startCtx.drawX > this.canvas.width) return;

         const stemLen = CONFIG.STEM_HEIGHT;
         let startBeamY, endBeamY;

         if (isUp) { startBeamY = startCtx.y - stemLen; endBeamY = endCtx.y - stemLen; } 
         else { startBeamY = startCtx.y + stemLen; endBeamY = endCtx.y + stemLen; }

         ctx.beginPath(); ctx.lineWidth = 5; ctx.strokeStyle = this.inkColor; ctx.lineCap = "butt"; 
         ctx.moveTo(startCtx.stemX, startBeamY); ctx.lineTo(endCtx.stemX, endBeamY); ctx.stroke();

         noteContexts.forEach(nCtx => {
            this.drawNoteAt(nCtx.note.bass, nCtx.note.x, "bass", "black", nCtx.note.duration, 1.0, true);
            const totalDist = endCtx.stemX - startCtx.stemX;
            const currentDist = nCtx.stemX - startCtx.stemX;
            const t = totalDist === 0 ? 0 : currentDist / totalDist;
            const beamYAtStem = startBeamY + (endBeamY - startBeamY) * t;
            const stemStart = nCtx.y + (isUp ? -2 : 2);
            ctx.beginPath(); ctx.lineWidth = 1.5; ctx.moveTo(nCtx.stemX, stemStart); ctx.lineTo(nCtx.stemX, beamYAtStem); ctx.stroke();
         });
      });

      userHistory.forEach((attemptGroup, idx) => {
         if (!attemptGroup) return;
         const stepX = noteObjects[idx].x;
         attemptGroup.forEach(attempt => {
            const octave = parseInt(attempt.note.match(/-?\d+/)[0]);
            const clef = octave >= 4 ? 'treble' : 'bass';
            const color = attempt.correct ? '#22c55e' : '#ef4444';
            this.drawNoteAt(attempt.note, stepX, clef, color, noteObjects[idx].duration);
         });
      });

      Object.keys(correctionNotes).forEach(idx => {
         const stepX = noteObjects[idx].x;
         correctionNotes[idx].forEach(note => {
            const octave = parseInt(note.match(/-?\d+/)[0]);
            const clef = octave >= 4 ? 'treble' : 'bass';
            this.drawNoteAt(note, stepX + 15, clef, '#eab308', noteObjects[idx].duration);
         });
      });

      if (currentHeldNotes.size > 0 && currentStepIndex < noteObjects.length) {
         const stepX = noteObjects[currentStepIndex].x;
         currentHeldNotes.forEach(note => {
            const octave = parseInt(note.match(/-?\d+/)[0]);
            const clef = octave >= 4 ? 'treble' : 'bass';
            this.drawNoteAt(note, stepX, clef, '3b82f6', noteObjects[currentStepIndex].duration, 0.7);
         });
      }

      this.ctx.restore();
      return activeNoteX;
   }
}

function generateLessonId(lesson) {
   const content = lesson.name + lesson.description + JSON.stringify(lesson.sequence);
   let hash = 0;
   for (let i = 0; i < content.length; i++) {
       const char = content.charCodeAt(i);
       hash = ((hash << 5) - hash) + char;
       hash |= 0; // Convert to 32bit integer
   }
   return `l${Math.abs(hash).toString(36)}`;
}

class LessonManager {
   constructor(renderer, audio, session) {
      this.renderer = renderer;
      this.audio = audio;
      this.session = session;

      this.currentLesson = null;
      this.currentStepIndex = 0;
      this.history = [];
      this.corrections = {};
      this.score = 0;

      this.startTime = 0;
      this.accumulatedTime = 0;
      this.lastActivityTime = 0;
      this.isPlaying = false;
      this.isFinished = false;

      this.lastBeatTime = 0;
      this.nextStepTime = 0;
      this.loopTimeout = null;
      this.currentTempo = 60; // Default BPM
   }

   loadLesson(lessonIdOrData) {
      let data;
      if (typeof lessonIdOrData === 'string') {
          data = LESSONS.find(l => generateLessonId(l) === lessonIdOrData) || LESSONS[0];
      } else {
          data = lessonIdOrData;
      }

      this.currentLesson = JSON.parse(JSON.stringify(data));
      this.currentTempo = this.currentLesson.tempo;
      this.reset();
   }

   reset(newTempo) {
      if(!this.currentLesson) return;
      if (newTempo) this.currentTempo = newTempo;

      this.currentStepIndex = 0;
      this.history = new Array(this.currentLesson.sequence.length).fill(null).map(() => []);
      this.corrections = {};
      this.score = 0;
      this.renderer.viewportOffsetX = 0;

      this.isPlaying = false;
      this.isFinished = false;
      this.accumulatedTime = 0;
      this.startTime = 0;

      if (this.loopTimeout) clearTimeout(this.loopTimeout);

      // Hide overlay and un-fade description
      document.getElementById("lessonCompleteOverlay").classList.remove('visible');
      document.getElementById("lessonDescription").classList.remove('fade-hidden');

      const id = generateLessonId(this.currentLesson);
      this.session.startLesson(id);

      document.getElementById("lessonDescription").textContent = this.currentLesson.description;
      this.updateScoreboard();
      this.render();

      this.startLessonLoop();
   }

   startLessonLoop() {
       this.isPlaying = true;
       this.startTime = Date.now();
       this.lastActivityTime = Date.now();
       this.playSequenceStep();
   }

   playSequenceStep() {
       if (this.isFinished) return;

       if (this.currentStepIndex >= this.currentLesson.sequence.length) {
           this.finishLesson();
           return;
       }

       const step = this.currentLesson.sequence[this.currentStepIndex];
       const beats = DURATION_MAP[step.duration] || 1;
       // Tempo is BPM, divide by 60 to get BPS
       const durationSec = beats / (this.currentTempo / 60);
       const durationMs = durationSec * 1000;

       this.lastBeatTime = Date.now();
       this.nextStepTime = this.lastBeatTime + durationMs;

       const activeNoteX = this.renderer.renderLessonState(this.currentLesson, this.currentStepIndex, this.history, new Set(), this.corrections);
       this.renderer.viewportOffsetX = this.calculateAutoScrollOffset(activeNoteX);

       if (this.currentStepIndex > 5) {
           document.getElementById("lessonDescription").classList.add('fade-hidden');
       }

       if (document.getElementById("chkSoundBass").checked) {
          this.audio.playNote(step.bass, durationSec * 0.9, 'triangle');
       }

       const requiresInput = (step.correctAnswer && step.correctAnswer.length > 0);

       if (!requiresInput) {
           this.loopTimeout = setTimeout(() => {
               this.currentStepIndex++;
               this.playSequenceStep();
           }, durationMs);
       }

       this.updateScoreboard();
   }

   notifyActivity() {
      this.lastActivityTime = Date.now();
   }

   calculateAutoScrollOffset(activeNoteX) {
      const targetScreenX = this.renderer.canvas.width / 3;
      const desiredOffset = activeNoteX - targetScreenX;
      return Math.max(0, desiredOffset);
   }

   formatTime(ms) {
      const totalSeconds = Math.floor(ms / 1000);
      const m = Math.floor(totalSeconds / 60);
      const s = totalSeconds % 60;
      return `${m}:${s.toString().padStart(2, '0')}`;
   }

   updateScoreboard() {
      const now = Date.now();
      const elapsed = this.accumulatedTime + (this.isPlaying ? (now - this.startTime) : 0);
      const timeStr = this.formatTime(elapsed);
      document.getElementById("scoreboard").textContent = `Score: ${this.score} | Time: ${timeStr}`;
   }

   submitChord(notesPlayed, firstNoteTime) {
      if (this.isFinished || !this.isPlaying) return;

      const currentStep = this.currentLesson.sequence[this.currentStepIndex];
      if (!currentStep.correctAnswer || currentStep.correctAnswer.length === 0) return;

      const correctNotes = currentStep.correctAnswer;
      const correctClasses = correctNotes.map(n => this.noteToMidiClass(n));

      let correctCount = 0;
      let incorrectCount = 0;

      const stepResult = [];

      notesPlayed.forEach(note => {
         const noteClass = this.noteToMidiClass(note);
         if (correctClasses.includes(noteClass)) {
             correctCount++;
             stepResult.push({ note, correct: true });
         } else {
             incorrectCount++;
             stepResult.push({ note, correct: false });
         }
      });

      this.history[this.currentStepIndex] = stepResult;

      this.score += (correctCount * CONFIG.SCORE_WEIGHTS.CORRECT_NOTE);
      this.score += (incorrectCount * CONFIG.SCORE_WEIGHTS.INCORRECT_NOTE);

      const timeDiff = firstNoteTime - this.lastBeatTime;

      if (Math.abs(timeDiff) <= CONFIG.SCORE_WEIGHTS.TIMING_WINDOW_MS) {
          this.score += CONFIG.SCORE_WEIGHTS.TIMING_PERFECT;
      } else {
          this.score += CONFIG.SCORE_WEIGHTS.TIMING_OFF;
      }

      const isSuccess = (correctCount > 0 && incorrectCount === 0 && correctCount === correctNotes.length);

      if (!isSuccess) {
         this.corrections[this.currentStepIndex] = correctNotes;
      }

      this.session.logEvent(Date.now() - this.startTime, "submit", {
         stepIndex: this.currentStepIndex,
         notes: notesPlayed,
         scoreDelta: this.score,
         timing: timeDiff
      });

      this.currentStepIndex++;
      this.updateScoreboard();

      const now = Date.now();
      const timeRemaining = this.nextStepTime - now;

      if (timeRemaining > 0) {
          this.loopTimeout = setTimeout(() => {
             this.playSequenceStep();
          }, timeRemaining);
      } else {
          this.playSequenceStep();
      }
   }

   finishLesson() {
       this.isFinished = true;
       this.isPlaying = false;
       const totalTime = Date.now() - this.startTime;

       // Immediate UI Feedback: Show Loading State
       const overlay = document.getElementById("lessonCompleteOverlay");
       const buttonsContainer = overlay.querySelector('.overlay-buttons');
       const title = overlay.querySelector('h2');

       if (buttonsContainer) buttonsContainer.style.display = 'none';
       if (title) title.textContent = "Saving progress...";
       overlay.classList.add("visible");

       // Wait for BOTH the API save and the 1s delay
       const savePromise = this.session.flushLog(totalTime, this.score, this.currentTempo);
       const delayPromise = new Promise(resolve => setTimeout(resolve, 1000));

       Promise.all([savePromise, delayPromise])
         .then(() => {
             this.showEndOverlay();
         })
         .catch((err) => {
             console.error("Error saving log:", err);
             // Show buttons anyway if save fails
             this.showEndOverlay();
         });
   }

   showEndOverlay() {
       const overlay = document.getElementById("lessonCompleteOverlay");
       const title = overlay.querySelector('h2');
       const buttonsContainer = overlay.querySelector('.overlay-buttons');

       // Restore UI state (Buttons visible, Title "Lesson Complete")
       if (title) title.textContent = "Lesson Complete!";
       if (buttonsContainer) buttonsContainer.style.display = 'flex';

       const btnRepeat = document.getElementById("btnRepeat");
       const btnNext = document.getElementById("btnNextLesson");

       let speedText = "Same Tempo";
       let newTempo = this.currentTempo;

       if (this.score > (this.currentLesson.sequence.length * 8)) {
           speedText = "Faster!";
           newTempo += 5; // Increase by 5 BPM
       } else if (this.score < 0) {
           speedText = "Slower";
           newTempo = Math.max(40, newTempo - 5);
       }

       document.getElementById("repeatSubtext").textContent = speedText;

       // Re-create buttons to strip old listeners
       const newBtnRepeat = btnRepeat.cloneNode(true);
       btnRepeat.parentNode.replaceChild(newBtnRepeat, btnRepeat);

       const newBtnNext = btnNext.cloneNode(true);
       btnNext.parentNode.replaceChild(newBtnNext, btnNext);

       newBtnRepeat.addEventListener("click", () => {
           this.reset(newTempo);
       });

       newBtnNext.addEventListener("click", () => {
           const currentId = generateLessonId(this.currentLesson);
           let idx = LESSONS.findIndex(l => generateLessonId(l) === currentId);
           if (idx < LESSONS.length - 1) {
               const nextLesson = LESSONS[idx + 1];
               this.loadLesson(nextLesson);
               // Update UI Pulldown
               const select = document.getElementById("lessonSelect");
               select.value = generateLessonId(nextLesson);
           } else {
               alert("No more lessons!");
               this.reset();
           }
       });
   }

   noteToMidiClass(noteName) {
      const map = { 'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11 };
      const letter = noteName.replace(/[\d#b-]+/, '');
      const acc = noteName.includes('#') ? 1 : (noteName.includes('b') ? -1 : 0);
      return (map[letter.charAt(0).toUpperCase()] + acc + 12) % 12;
   }

   render(heldNotes) {
       if(this.currentLesson) {
           this.renderer.renderLessonState(this.currentLesson, this.currentStepIndex, this.history, heldNotes || new Set(), this.corrections);
       }
   }
}

class InputHandler {
   constructor(manager, audio) {
      this.manager = manager;
      this.audio = audio;
      this.heldNotes = new Set();
      this.chordBuffer = new Set();

      this.firstNoteTime = 0;
   }

   handleNoteOn(note, velocity = 80) {
      // Stop accepting input if finished
      if (this.manager.isFinished) return;

      this.manager.notifyActivity();

      if (this.heldNotes.size === 0) {
          this.firstNoteTime = Date.now();
      }

      this.manager.session.logEvent(Date.now(), "noteOn", { note: note, velocity: velocity });

      if (document.getElementById("chkSoundUser").checked) {
         this.audio.playNote(note, 0.3, 'sine');
      }
      this.heldNotes.add(note);
      this.chordBuffer.add(note);

      this.manager.render(this.heldNotes);
      this.updateKeyboardUI();
   }

   handleNoteOff(note) {
      this.manager.session.logEvent(Date.now(), "noteOff", { note: note });

      this.heldNotes.delete(note);
      this.manager.render(this.heldNotes);
      this.updateKeyboardUI();

      if (this.heldNotes.size === 0 && this.chordBuffer.size > 0) {
         this.submit();
      }
   }

   updateKeyboardUI() {
      document.querySelectorAll('.white-key, .black-key').forEach(el => {
         if (this.heldNotes.has(el.dataset.note)) {
            el.classList.add('active');
         } else {
            el.classList.remove('active');
         }
      });
   }

   submit() {
      const notes = Array.from(this.chordBuffer);
      this.manager.submitChord(notes, this.firstNoteTime);
      this.chordBuffer.clear();
      this.firstNoteTime = 0;
   }

   onMidiMessage(msg) {
      const [status, data1, data2] = msg.data;
      const command = status >> 4;
      const noteNum = data1;
      const velocity = (data2 > 127) ? 127 : data2;

      if (command === 9 && velocity > 0) {
          const noteName = this.midiNumToNote(noteNum, this.manager.currentLesson ? this.manager.currentLesson.defaultKey : 0);
          this.handleNoteOn(noteName, velocity);
      }
      else if (command === 8 || (command === 9 && velocity === 0)) {
          const noteName = this.midiNumToNote(noteNum, this.manager.currentLesson ? this.manager.currentLesson.defaultKey : 0);
          this.handleNoteOff(noteName);
      }
   }

   midiNumToNote(num, keySig) {
      const octave = Math.floor(num / 12) - 1;
      const noteIndex = num % 12;

      const sharps = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
      const flats = ['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B'];

      // Use flats if key is negative, or randomly for C major (0) to handle accidentals neutrally
      const useFlats = (keySig < 0) || (keySig === 0 && Math.random() > 1);

      if (keySig < 0) return flats[noteIndex] + octave;

      return sharps[noteIndex] + octave;
   }
}

class Keyboard {
   static create(containerId, inputHandler) {
      const container = document.getElementById(containerId);
      const canvas = document.getElementById("sheet");
      const width = canvas.width;
      container.style.width = `${width}px`;
      container.innerHTML = "";

      const kWidth = CONFIG.KEY_WIDTH;
      const numKeysToDraw = Math.ceil(width / kWidth) + 5; 
      const middleNoteOffset = 2; 
      const centerIndexOffset = Math.floor(numKeysToDraw / 2);

      const getWhiteNote = (offset) => {
         const map = ['C','D','E','F','G','A','B'];
         let index = offset + middleNoteOffset;
         let octave = 4 + Math.floor(index / 7);
         let noteIdx = index % 7;
         if (noteIdx < 0) noteIdx += 7;
         return map[noteIdx] + octave;
      };

      const whiteNotes = [];
      for (let i = -centerIndexOffset; i <= centerIndexOffset; i++) {
         whiteNotes.push({ note: getWhiteNote(i), offset: i });
      }

      const whiteKeyHeight = 120;
      const blackKeyWidth = kWidth * 0.65;
      const blackKeyHeight = 80;

      const attachEvents = (el, note) => {
         el.dataset.note = note;
         el.addEventListener("pointerdown", (e) => {
            e.preventDefault();
            el.setPointerCapture(e.pointerId);
            inputHandler.handleNoteOn(note);
         });
         el.addEventListener("pointerup", (e) => {
            e.preventDefault();
            inputHandler.handleNoteOff(note);
         });
         el.addEventListener("pointercancel", (e) => inputHandler.handleNoteOff(note));
      };

      const c4Left = width / 2; 

      whiteNotes.forEach(item => {
         const left = c4Left + (item.offset * kWidth) - (kWidth / 2); 
         if (left > width || left + kWidth < 0) return;

         const key = document.createElement("div");
         key.className = "white-key";
         key.style.left = `${left}px`;
         key.style.width = `${kWidth}px`;
         key.style.height = `${whiteKeyHeight}px`;
         key.textContent = item.note.includes('C') ? item.note : '';
         key.style.display = "flex"; key.style.alignItems="flex-end"; key.style.justifyContent="center";
         key.style.fontSize="10px"; key.style.paddingBottom="5px"; key.style.fontWeight="bold";
         attachEvents(key, item.note);
         container.appendChild(key);

         const letter = item.note.charAt(0);
         if (['C','D','F','G','A'].includes(letter)) {
            const blackNote = letter + '#' + item.note.slice(-1);
            const bKey = document.createElement("div");
            bKey.className = "black-key";
            bKey.style.left = `${left + kWidth - (blackKeyWidth / 2)}px`;
            bKey.style.width = `${blackKeyWidth}px`;
            bKey.style.height = `${blackKeyHeight}px`;
            
            // Enharmonic hack for keyboard keys (displaying correct ID)
            // We bind the specific sharp note here, but visual could be improved later
            attachEvents(bKey, blackNote);
            container.appendChild(bKey);
         }
      });
   }
}

window.addEventListener("DOMContentLoaded", () => {
   const session = new SessionManager();
   const audio = new AudioEngine();
   const canvas = document.getElementById("sheet");
   const renderer = new CanvasRenderer(canvas);
   const manager = new LessonManager(renderer, audio, session);
   const inputHandler = new InputHandler(manager, audio);

   // --- THEME HANDLING ---
   const themeBtn = document.getElementById("themeBtn");
   const currentTheme = localStorage.getItem("continuo_theme") || "light";
   if (currentTheme === "dark") {
       document.body.setAttribute("data-theme", "dark");
       themeBtn.textContent = "☀️";
   }

   themeBtn.addEventListener("click", () => {
       const isDark = document.body.getAttribute("data-theme") === "dark";
       if (isDark) {
           document.body.removeAttribute("data-theme");
           themeBtn.textContent = "🌙";
           localStorage.setItem("continuo_theme", "light");
       } else {
           document.body.setAttribute("data-theme", "dark");
           themeBtn.textContent = "☀️";
           localStorage.setItem("continuo_theme", "dark");
       }
       // Force re-render to update canvas colors
       manager.render(inputHandler.heldNotes);
   });

   renderer.resize();
   Keyboard.create("keyboard", inputHandler);

   // --- DRAG/SCROLL LOGIC ---
   let isDragging = false;
   let startDragX = 0;
   let startScrollX = 0;

   const startDrag = (x) => {
      isDragging = true;
      startDragX = x;
      startScrollX = renderer.viewportOffsetX;
      canvas.style.cursor = 'grabbing';
   };

   const moveDrag = (x) => {
      if (!isDragging) return;
      const delta = startDragX - x;
      const newOffset = Math.max(0, startScrollX + delta);
      renderer.viewportOffsetX = newOffset;
      manager.render(inputHandler.heldNotes);
   };

   const endDrag = () => {
      isDragging = false;
      canvas.style.cursor = 'grab';
   };

   canvas.addEventListener('mousedown', (e) => startDrag(e.clientX));
   window.addEventListener('mousemove', (e) => moveDrag(e.clientX));
   window.addEventListener('mouseup', endDrag);

   canvas.addEventListener('touchstart', (e) => {
      if (e.target === canvas) {
         startDrag(e.touches[0].clientX);
      }
   }, {passive: false});

   canvas.addEventListener('touchmove', (e) => {
      if (e.target === canvas && isDragging) {
         moveDrag(e.touches[0].clientX);
         e.preventDefault();
      }
   }, {passive: false});

   window.addEventListener('touchend', endDrag);

   // Resize Handler
   let resizeTimeout;
   window.addEventListener("resize", () => {
      clearTimeout(resizeTimeout);
      resizeTimeout = setTimeout(() => {
         renderer.resize();
         Keyboard.create("keyboard", inputHandler);
         manager.render();
      }, 100);
   });

   // Lesson Selection
   const lessonSelect = document.getElementById("lessonSelect");
   LESSONS.forEach(l => {
      const opt = document.createElement("option");
      opt.value = generateLessonId(l);
      opt.textContent = l.name;
      lessonSelect.appendChild(opt);
   });

   lessonSelect.addEventListener("change", (e) => manager.loadLesson(e.target.value));
   document.getElementById("resetBtn").addEventListener("click", () => manager.reset());

   // MIDI Setup
   document.getElementById("midiBtn").addEventListener("click", () => {
      audio.init();
      if (navigator.requestMIDIAccess) {
         navigator.requestMIDIAccess().then(access => {
            document.getElementById("midiBtn").classList.add("active");
            document.getElementById("midiBtn").textContent = "MIDI Connected";
            const inputs = access.inputs.values();
            for (let input of inputs) {
               input.onmidimessage = (msg) => inputHandler.onMidiMessage(msg);
            }
            access.onstatechange = (e) => {
               if(e.port.type === 'input' && e.port.state === 'connected') {
                  e.port.onmidimessage = (msg) => inputHandler.onMidiMessage(msg);
               }
            };
         }, () => alert("MIDI Access Failed"));
      } else alert("Web MIDI API not supported.");
   });

   manager.loadLesson(LESSONS[0]);
});
