/*
 * continuo.js
 *
 * This file contains the complete frontend logic, including the core game engine,
 * the Canvas renderer, MIDI and keyboard input handling, and the new
 * SessionManager for logging user performance data to the Python backend.
 */

const CONFIG = {
   BEAT_SPACING_PX: 50,
   NOTE_HEAD_FONT_SIZE: 38,
   STEM_HEIGHT: 35,
   LINE_SPACING: 10,
   KEY_WIDTH: 40
};

// Updated Duration Map including Dotted Whole
const DURATION_MAP = {
   0: 8,   // Breve (Double Whole)
   1: 4,   // Whole
   1.5: 6, // Dotted Whole (4 + 2 beats) - NOTE: This isn't strictly correct duration but works for notation.
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
      id: "l1",
      name: "Lesson 1: Root Position Triads (Anacrusis)",
      description: "Play the 3rd and 5th above the bass. (2-beat pickup)",
      defaultKey: 0,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      sequence: [
         { bass: "C3", figure: "", duration: 2 }, // Step 1 (2 beats) completes the anacrusis
         { bass: "G3", figure: "", duration: 2 }, // Start of Measure 2
         { bass: "E3", figure: "", duration: 2 },
         { bass: "A3", figure: "", duration: 2 },
         { bass: "F3", figure: "", duration: 2 },
         { bass: "C4", figure: "", duration: 2 },
         { bass: "A3", figure: "", duration: 2 },
         { bass: "F3", figure: "", duration: 2 },
         { bass: "G3", figure: "", duration: 2 },
         { bass: "A3", figure: "", duration: 2 },
         { bass: "F3", figure: "", duration: 2 },
         { bass: "G3", figure: "", duration: 2 },
         { bass: "C3", figure: "", duration: 1 }
      ]
   },
   {
      id: "l2",
      name: "Lesson 2: Full Measure Triads",
      description: "Play the 3rd and 5th above the bass.",
      defaultKey: 1,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      sequence: [
         { bass: "G2", figure: "", duration: 4 },
         { bass: "G3", figure: "", duration: 4 },
         { bass: "E3", figure: "", duration: 4 },
         { bass: "C3", figure: "", duration: 4 },
         { bass: "D3", figure: "", duration: 4 },
         { bass: "B2", figure: "", duration: 4 },
         { bass: "E3", figure: "", duration: 4 },
         { bass: "A2", figure: "", duration: 4 },
         { bass: "D3", figure: "", duration: 4 },
         { bass: "G2", figure: "", duration: 4 },
         { bass: "C3", figure: "", duration: 4 },
         { bass: "A2", figure: "", duration: 4 },
         { bass: "E3", figure: "", duration: 4 },
         { bass: "C3", figure: "", duration: 4 },
         { bass: "G3", figure: "", duration: 4 },
         { bass: "E3", figure: "", duration: 4 },
         { bass: "B3", figure: "", duration: 4 },
         { bass: "G3", figure: "", duration: 4 },
         { bass: "D4", figure: "", duration: 4 },
         { bass: "D3", figure: "", duration: 4 },
         { bass: "E3", figure: "", duration: 4 },
         { bass: "C3", figure: "", duration: 4 },
         { bass: "D3", figure: "", duration: 2 },
         { bass: "G2", figure: "", duration: 1 }
      ]
   },
   {
      id: "l3",
      name: "Lesson 3: Eighth Notes",
      description: "Play the 3rd and 5th above the bass.",
      defaultKey: -1,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      sequence: [
         { bass: "F3", figure: "", duration: 8 },
         { bass: "G3", figure: "", duration: 8 },
         { bass: "F3", figure: "", duration: 8 },
         { bass: "E3", figure: "", duration: 8 },
         { bass: "D3", figure: "", duration: 8 },
         { bass: "A3", figure: "", duration: 8 },
         { bass: "D4", figure: "", duration: 8 },
         { bass: "C4", figure: "", duration: 8 },
         { bass: "Bb3", figure: "", duration: 8 },
         { bass: "C4", figure: "", duration: 8 },
         { bass: "Bb3", figure: "", duration: 8 },
         { bass: "A3", figure: "", duration: 8 },
         { bass: "G3", figure: "", duration: 8 },
         { bass: "F3", figure: "", duration: 8 },
         { bass: "G3", figure: "", duration: 8 },
         { bass: "G2", figure: "", duration: 8 },
         { bass: "C3", figure: "", duration: 8 },
         { bass: "D3", figure: "", duration: 8 },
         { bass: "C3", figure: "", duration: 8 },
         { bass: "Bb2", figure: "", duration: 8 },
         { bass: "A2", figure: "", duration: 8 },
         { bass: "G2", figure: "", duration: 8 },
         { bass: "F2", figure: "", duration: 8 },
         { bass: "A2", figure: "", duration: 8 },
         { bass: "Bb2", figure: "", duration: 8 },
         { bass: "A2", figure: "", duration: 8 },
         { bass: "G2", figure: "", duration: 8 },
         { bass: "Bb2", figure: "", duration: 8 },
         { bass: "C3", figure: "", duration: 8 },
         { bass: "Bb2", figure: "", duration: 8 },
         { bass: "A2", figure: "", duration: 8 },
         { bass: "C3", figure: "", duration: 8 },
         { bass: "D3", figure: "", duration: 8 },
         { bass: "C3", figure: "", duration: 8 },
         { bass: "D3", figure: "", duration: 8 },
         { bass: "E3", figure: "", duration: 8 },
         { bass: "F3", figure: "", duration: 4 },
         { bass: "Bb2", figure: "", duration: 4 },
         { bass: "C3", figure: "", duration: 2 },
         { bass: "F2", figure: "", duration: 2 }
      ]
   },
   {
      id: "l4",
      name: "Lesson 4: Half notes",
      description: "The figure '#' raises the 7th note.",
      defaultKey: -1,
      timeSignature: [4, 4],
      anacrusisBeats: 0,
      sequence: [
         { bass: "C3", figure: "", duration: 2 },
         { bass: "G2", figure: "7", duration: 2 },
         { bass: "C3", figure: "", duration: 1.5 },
         { bass: "C3", figure: "", duration: 1 }
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
         this.registerUser(this.userId); // Fire and forget
      }
      this.updateDisplay();

      // UI Handler
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
      // Assuming same origin, this should work when served by Python
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

   flushLog(totalDuration, score) {
      if (this.eventLog.length === 0) return;

      const payload = {
         userId: this.userId,
         lessonId: this.currentLessonId,
         totalDuration: Math.floor(totalDuration),
         score: score,
         events: this.eventLog
      };

      console.log("Saving log...", payload);

      // Send to Python Backend
      fetch('/api/log', {
         method: 'POST',
         headers: {'Content-Type': 'application/json'},
         body: JSON.stringify(payload)
      }).then(res => res.json())
        .then(d => console.log("Log saved:", d))
        .catch(e => console.error("Logging failed", e));

      // Clear after send? Or keep until next startLesson?
      // Keeping until next start ensures we don't send partials
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
      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();
      osc.type = type;
      osc.frequency.value = this.noteToFreq(note);
      gain.gain.setValueAtTime(0, this.ctx.currentTime);
      gain.gain.linearRampToValueAtTime(1, this.ctx.currentTime + 0.02);
      gain.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + durationSec);
      osc.connect(gain);
      gain.connect(this.masterGain);
      osc.start();
      osc.stop(this.ctx.currentTime + durationSec);
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
      this.staffTop = 60;
      this.lineSpacing = CONFIG.LINE_SPACING;
      this.numLines = 5;
      this.staffGap = 90;
      this.currentKeySig = 0;
      this.viewportOffsetX = 0;
   }

   resize() {
      const rect = this.canvas.parentElement.getBoundingClientRect();
      this.canvas.width = rect.width;
      this.canvas.height = 280;
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
      // Treble Ref: B4 (Center Line) | Bass Ref: D3 (Center Line)
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
      ctx.strokeStyle = "#000";
      ctx.lineWidth = 1;
      ctx.fillStyle = "#000";
      const width = this.canvas.width;

      // Treble Staff
      for (let i = 0; i < this.numLines; i++) {
         const y = this.staffTop + i * this.lineSpacing;
         ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
      }
      // Bass Staff
      const bassTop = this.staffTop + this.staffGap;
      for (let i = 0; i < this.numLines; i++) {
         const y = bassTop + i * this.lineSpacing;
         ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
      }

      // Clef Alignment
      ctx.font = `${this.lineSpacing * 4}px serif`;
      ctx.fillText("𝄞", 10, this.staffTop + 3 * this.lineSpacing + (this.lineSpacing/2));

      ctx.font = `${this.lineSpacing * 3.5}px serif`;
      ctx.fillText("𝄢", 10, bassTop + 2.5 * this.lineSpacing);
   }

   drawKeySignature(num) {
      const ctx = this.ctx;
      ctx.font = `${this.lineSpacing * 2.5}px serif`;
      const isSharp = num > 0;
      const count = Math.abs(num);
      const symbol = isSharp ? "♯" : "♭";
      const xStart = 50;

      const sharpNotesT = ['F5','C5','G5','D5','A4','E5','B4'];
      const sharpNotesB = ['F3','C3','G3','D3','A2','E3','B2'];
      const flatNotesT = ['B4','E5','A4','D5','G4','C5','F4'];
      const flatNotesB = ['B2','E3','A2','D3','G2','C3','F2'];

      const targetT = isSharp ? sharpNotesT : flatNotesT;
      const targetB = isSharp ? sharpNotesB : flatNotesB;

      for(let i=0; i<count; i++) {
         const x = xStart + (i * 14);
         ctx.fillText(symbol, x, this.getNoteY(targetT[i], 'treble') + 5);
         ctx.fillText(symbol, x, this.getNoteY(targetB[i], 'bass') + 5);
      }
   }

   drawTimeSignature(timeSig, keySigCount) {
      if (!timeSig) return;
      const [num, den] = timeSig;
      const x = 60 + (Math.abs(keySigCount) * 14) + 25;
      const ctx = this.ctx;

      ctx.font = `bold ${this.lineSpacing * 2.8}px serif`;
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      const trebleMidY = this.staffTop + (2 * this.lineSpacing);
      const bassMidY = this.staffTop + this.staffGap + (2 * this.lineSpacing);

      let symbol = null;
      if (num === 4 && den === 4) symbol = "𝄴";
      else if (num === 2 && den === 2) symbol = "𝄵";

      if (symbol) {
         ctx.fillText(symbol, x, trebleMidY);
         ctx.fillText(symbol, x, bassMidY);
      } else {
         const offset = this.lineSpacing;
         ctx.font = `bold ${this.lineSpacing * 2}px serif`;
         ctx.fillText(num, x, trebleMidY - offset);
         ctx.fillText(den, x, trebleMidY + offset);
         ctx.fillText(num, x, bassMidY - offset);
         ctx.fillText(den, x, bassMidY + offset);
      }
      return x + 40;
   }

   drawEndBarline(x) {
      const ctx = this.ctx;
      if (x < 0 || x > this.canvas.width) return;

      const topY = this.staffTop;
      const botY = this.staffTop + this.staffGap + (this.numLines - 1) * this.lineSpacing;

      // Thin line
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x - 6, topY);
      ctx.lineTo(x - 6, botY);
      ctx.stroke();

      // Thick line
      ctx.lineWidth = 4;
      ctx.beginPath();
      ctx.moveTo(x, topY);
      ctx.lineTo(x, botY);
      ctx.stroke();

      ctx.lineWidth = 1;
   }

   drawBarLine(x) {
      const ctx = this.ctx;
      if (x < 0 || x > this.canvas.width) return;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x, this.staffTop);
      ctx.lineTo(x, this.staffTop + this.staffGap + (this.numLines-1)*this.lineSpacing);
      ctx.stroke();
   }

   drawNote(note, x, clef, color = "black", durationCode = 4, alpha = 1.0) {
      const drawX = x - this.viewportOffsetX;
      // Cull if offscreen
      if (drawX < -50 || drawX > this.canvas.width + 50) return;

      const y = this.getNoteY(note, clef);
      const ctx = this.ctx;

      ctx.globalAlpha = alpha;
      ctx.fillStyle = color;
      ctx.strokeStyle = color;
      ctx.font = `${CONFIG.NOTE_HEAD_FONT_SIZE}px serif`;
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      // Map durationCode to Unicode Noteheads
      let glyph = "𝅘"; // Default Filled (Quarter, Eighth)
      let hasStem = true;
      let showDot = false;
      let isHollow = false;

      if (durationCode === 1.5) {
         glyph = "𝅝"; hasStem = false; showDot = true; isHollow = true;
      }
      else if (durationCode === 2) { glyph = "𝅗"; hasStem = true; isHollow = true; } // Half
      else if (durationCode === 1) { glyph = "𝅝"; hasStem = false; isHollow = true; } // Whole
      else if (durationCode === 0) { glyph = "𝅜"; hasStem = false; isHollow = true; } // Breve
      // durationCode 4 (quarter) and 8 (eighth) use the default filled glyph

      // Visual Adjustment:
      const visualY = y - (this.lineSpacing * 0.6);

      // Draw Notehead
      ctx.fillText(glyph, drawX, visualY);

      // Draw Dot
      if (showDot) {
         ctx.beginPath();
         ctx.arc(drawX + 22, visualY, 3, 0, Math.PI * 2);
         ctx.fill();
      }

      // Draw Stem and Flag
      if (hasStem) {
         ctx.lineWidth = 1.5;
         ctx.beginPath();

         const middleLineY = (clef === "treble")
            ? (this.staffTop + 2 * this.lineSpacing)
            : (this.staffTop + this.staffGap + 2 * this.lineSpacing);

         const isUp = y > middleLineY;
         const stemLen = CONFIG.STEM_HEIGHT;

         const xOffset = isUp ? 3.2 : -3.2;
         const stemBaseY = visualY + (isHollow ? 0 : 4);

         const stemTopY = stemBaseY - stemLen;
         const stemBotY = stemBaseY + stemLen;

         ctx.moveTo(drawX + xOffset, stemBaseY);
         const endY = isUp ? stemTopY : stemBotY;
         ctx.lineTo(drawX + xOffset, endY);
         ctx.stroke();

         if (durationCode === 8) {
             // Eighth note flag (basic implementation)
             this.drawFlag(drawX + xOffset, endY, isUp);
         }
      }

      // Accidentals
      const accidental = this.getAccidentalSymbol(note, this.currentKeySig);
      if (accidental) {
         ctx.font = `${this.lineSpacing * 2.5}px serif`;
         ctx.textAlign = "center";
         ctx.textBaseline = "middle";
         ctx.fillText(accidental, drawX - 22, visualY);
      }

      // Ledger Lines
      const topLineY = clef === "treble" ? this.staffTop : this.staffTop + this.staffGap;
      const bottomLineY = topLineY + (this.numLines - 1) * this.lineSpacing;
      const ledgerWidth = 24;

      ctx.lineWidth = 1;
      if (y <= topLineY - this.lineSpacing) {
         for (let ly = topLineY - this.lineSpacing; ly >= y - 2; ly -= this.lineSpacing) {
            ctx.beginPath(); ctx.moveTo(drawX - ledgerWidth/2, ly); ctx.lineTo(drawX + ledgerWidth/2, ly); ctx.stroke();
         }
      }
      if (y >= bottomLineY + this.lineSpacing) {
         for (let ly = bottomLineY + this.lineSpacing; ly <= y + 2; ly += this.lineSpacing) {
            ctx.beginPath(); ctx.moveTo(drawX - ledgerWidth/2, ly); ctx.lineTo(drawX + ledgerWidth/2, ly); ctx.stroke();
         }
      }
      ctx.globalAlpha = 1.0;
   }

   drawFlag(x, y, isUp) {
      const ctx = this.ctx;
      ctx.beginPath();
      // Simple flag for eighth notes (duration 8)
      if (isUp) {
         ctx.moveTo(x, y);
         ctx.quadraticCurveTo(x + 10, y + 10, x + 6, y + 30);
         ctx.stroke();
      } else {
         ctx.moveTo(x, y);
         ctx.quadraticCurveTo(x + 10, y - 10, x + 6, y - 30);
         ctx.stroke();
      }
   }

   getAccidentalSymbol(note, keySig) {
      const letter = note.replace(/[\d#b]+/, '');
      const acc = note.includes('#') ? '#' : (note.includes('b') ? 'b' : '');
      const scale = KEY_SCALES[keySig.toString()];
      const keyNote = scale.find(n => n.startsWith(letter));
      const keyAcc = keyNote.includes('#') ? '#' : (keyNote.includes('b') ? 'b' : '');

      if (acc === keyAcc) return null;
      if (acc === '#' && keyAcc !== '#') return '♯';
      if (acc === 'b' && keyAcc !== 'b') return '♭';
      if (acc === '' && keyAcc !== '') return '♮';
      return null;
   }

   drawFigure(figure, x, note, clef) {
      const drawX = x - this.viewportOffsetX;
      if (drawX < -50 || drawX > this.canvas.width + 50) return;
      const y = this.getNoteY(note, clef) + 45;
      this.ctx.font = `bold ${this.lineSpacing * 1.4}px serif`;
      this.ctx.textAlign = "center";
      this.ctx.fillStyle = "black";
      this.ctx.fillText(figure, drawX, y);
   }

   renderLessonState(lesson, currentStepIndex, userHistory, currentHeldNotes, correctionNotes) {
      this.currentKeySig = lesson.defaultKey;
      this.clear();
      this.drawStaves();
      this.drawKeySignature(lesson.defaultKey);

      const contentStartX = this.drawTimeSignature(lesson.timeSignature, lesson.defaultKey);

      let cursorX = contentStartX;
      let currentMeasureBeats = 0;

      const fullMeasureBeats = lesson.timeSignature[0];
      // Get anacrusis beats. If not defined or 0, default to a full measure.
      const anacrusisBeats = lesson.anacrusisBeats || fullMeasureBeats;

      let measureTarget = anacrusisBeats; // Start with the anacrusis target
      let activeNoteX = 0;

      // First pass: positioning
      const stepsWithPos = lesson.sequence.map((step, index) => {
         // Duration in beats (e.g., half note (2) in 4/4 is 2 beats)
         const beats = DURATION_MAP[step.duration];
         const width = beats * CONFIG.BEAT_SPACING_PX;
         const pos = cursorX + (width / 2);

         if (index === currentStepIndex) activeNoteX = pos;

         const stepData = { x: pos, width: width, ...step };
         cursorX += width;
         currentMeasureBeats += beats;

         let barlineX = null;
         // Check if the current measure is complete
         // Use a small epsilon (0.01) for float comparisons
         if (Math.abs(currentMeasureBeats - measureTarget) < 0.01 || currentMeasureBeats > measureTarget) {
            barlineX = cursorX;
            currentMeasureBeats = 0;
            // After the first measure (anacrusis), switch to the full measure target for all subsequent measures.
            measureTarget = fullMeasureBeats;
         }

         return { step: stepData, barlineX: barlineX, index: index };
      });

      // Continuous Smooth Scrolling
      const targetScreenX = this.canvas.width / 3;
      const desiredOffset = activeNoteX - targetScreenX;
      this.viewportOffsetX = Math.max(0, desiredOffset);


      // Second pass: Draw
      stepsWithPos.forEach((item) => {
         const { step, barlineX, index } = item;

         // Highlight active step
         if (index === currentStepIndex) {
            const drawX = step.x - (step.width/2) - this.viewportOffsetX;
            if (drawX > -100 && drawX < this.canvas.width) {
               this.ctx.fillStyle = "rgba(255, 255, 0, 0.2)";
               this.ctx.fillRect(drawX, 0, step.width, this.canvas.height);
            }
         }

         this.drawNote(step.bass, step.x, "bass", "black", step.duration);
         this.drawFigure(step.figure, step.x, step.bass, "bass");

         // History (user notes)
         if (userHistory[index]) {
            userHistory[index].forEach(attempt => {
               const octave = parseInt(attempt.note.match(/-?\d+/)[0]);
               const clef = octave >= 4 ? 'treble' : 'bass';
               const color = attempt.correct ? '#22c55e' : '#ef4444';
               this.drawNote(attempt.note, step.x, clef, color, step.duration);
            });

            // Correction (suggestion)
            if (correctionNotes && correctionNotes[index]) {
               correctionNotes[index].forEach(note => {
                  const octave = parseInt(note.match(/-?\d+/)[0]);
                  const clef = octave >= 4 ? 'treble' : 'bass';
                  this.drawNote(note, step.x + 15, clef, '#eab308', step.duration);
               });
            }
         }

         // Held (real-time input)
         if (index === currentStepIndex && currentHeldNotes.size > 0) {
            currentHeldNotes.forEach(note => {
               const octave = parseInt(note.match(/-?\d+/)[0]);
               const clef = octave >= 4 ? 'treble' : 'bass';
               this.drawNote(note, step.x, clef, 'blue', step.duration, 0.7);
            });
         }

         // Draw Barline
         if (barlineX) {
            if (index === stepsWithPos.length - 1) {
               this.drawEndBarline(barlineX - this.viewportOffsetX);
            } else {
               this.drawBarLine(barlineX - this.viewportOffsetX);
            }
         }
      });
   }
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
      this.timerInterval = null;
      this.startTime = 0;
      this.accumulatedTime = 0;
      this.lastActivityTime = 0;
      this.isPlaying = false;
      this.isFinished = false;
   }

   loadLesson(lessonId) {
      const data = LESSONS.find(l => l.id === lessonId) || LESSONS[0];
      this.currentLesson = JSON.parse(JSON.stringify(data));
      this.reset();
   }

   reset() {
      if(!this.currentLesson) return;
      this.currentStepIndex = 0;
      this.history = new Array(this.currentLesson.sequence.length).fill(null).map(() => []);
      this.corrections = {};
      this.score = 0;
      this.renderer.viewportOffsetX = 0;

      this.stopTimer();
      this.accumulatedTime = 0;
      this.startTime = 0;
      this.lastActivityTime = 0;
      this.isPlaying = false;
      this.isFinished = false;

      // Log the reset as a new attempt start
      this.session.startLesson(this.currentLesson.id);

      document.getElementById("lessonDescription").textContent = this.currentLesson.description;
      this.updateScoreboard();

      this.render();
      this.playBassForCurrentStep();
   }

   notifyActivity() {
      if (this.isFinished) return;
      const now = Date.now();
      this.lastActivityTime = now;
      if (!this.isPlaying) {
         this.isPlaying = true;
         this.startTime = now;
         this.startTimerLoop();
      }
   }

   startTimerLoop() {
      if (this.timerInterval) clearInterval(this.timerInterval);
      this.timerInterval = setInterval(() => this._tick(), 100);
   }

   stopTimer() {
      if (this.timerInterval) clearInterval(this.timerInterval);
      this.timerInterval = null;
   }

   _tick() {
      if (!this.isPlaying || this.isFinished) return;
      const now = Date.now();
      if (now - this.lastActivityTime > 5000) {
         this.isPlaying = false;
         this.accumulatedTime += (this.lastActivityTime - this.startTime);
         this.updateScoreboard(true);
         return;
      }
      this.updateScoreboard();
   }

   getTotalTimeMS() {
      if (!this.isPlaying) return this.accumulatedTime;
      return this.accumulatedTime + (Date.now() - this.startTime);
   }

   // Return just the offset for logging
   getSessionTime() {
      if (!this.isPlaying && this.accumulatedTime === 0) return 0;
      if (!this.isPlaying) return this.accumulatedTime;
      return this.accumulatedTime + (Date.now() - this.startTime);
   }

   formatTime(ms) {
      const totalSeconds = Math.floor(ms / 1000);
      const m = Math.floor(totalSeconds / 60);
      const s = totalSeconds % 60;
      return `${m}:${s.toString().padStart(2, '0')}`;
   }

   updateScoreboard(isPaused = false) {
      const timeStr = this.formatTime(this.getTotalTimeMS());
      const pausedText = (isPaused || (!this.isPlaying && this.accumulatedTime > 0 && !this.isFinished)) ? " (Paused)" : "";
      document.getElementById("scoreboard").textContent = `Score: ${this.score} | Time: ${timeStr}${pausedText}`;
   }

   playBassForCurrentStep() {
      if (!document.getElementById("chkSoundBass").checked) return;
      if (!this.currentLesson || this.currentStepIndex >= this.currentLesson.sequence.length) return;

      const step = this.currentLesson.sequence[this.currentStepIndex];
      // Note: 0.5 is an arbitrary constant for sound duration relative to beats.
      const durBeats = DURATION_MAP[step.duration] || 1;
      this.audio.playNote(step.bass, durBeats * 0.5, 'triangle');
   }

   getCorrectPitchClasses(step) {
      const { bass, figure } = step;
      const scale = KEY_SCALES[this.currentLesson.defaultKey.toString()];
      const bassLetter = bass.replace(/[\d-]+/, '');
      let scaleIndex = scale.indexOf(bassLetter.charAt(0).toUpperCase() + bassLetter.slice(1));
      if (scaleIndex === -1) {
         const map = { 'C': 0, 'D': 1, 'E': 2, 'F': 3, 'G': 4, 'A': 5, 'B': 6 };
         scaleIndex = map[bassLetter.charAt(0).toUpperCase()];
      }

      let intervals = [0, 2, 4];
      if (figure === "6") intervals = [0, 2, 5];
      if (figure === "6/4") intervals = [0, 3, 5];
      if (figure === "7") intervals = [0, 2, 4, 6];

      return intervals.map(iv => {
         const targetIndex = (scaleIndex + iv) % 7;
         return this.noteToMidiClass(scale[targetIndex]);
      });
   }

   generateCorrection(step) {
      const classes = this.getCorrectPitchClasses(step);
      const correction = [];
      const range = ['C4','C#4','D4','D#4','E4','F4','F#4','G4','G#4','A4','A#4','B4',
         'C5','C#5','D5','D#5','E5','F5','F#5','G5'];
      const usedClasses = new Set();
      range.forEach(note => {
         const nc = this.noteToMidiClass(note);
         if (classes.includes(nc) && !usedClasses.has(nc)) {
            correction.push(note);
            usedClasses.add(nc);
         }
      });
      // Sort correction notes by pitch for cleaner display
      correction.sort((a,b) => this.noteToMidiNum(a) - this.noteToMidiNum(b));
      return correction.slice(0, classes.length);
   }

   noteToMidiClass(noteName) {
      const map = { 'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11 };
      const letter = noteName.replace(/[\d#b-]+/, '');
      const acc = noteName.includes('#') ? 1 : (noteName.includes('b') ? -1 : 0);
      return (map[letter.charAt(0).toUpperCase()] + acc + 12) % 12;
   }

   noteToMidiNum(noteName) {
      const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
      const letter = noteName.replace(/[\d#b-]+/, '');
      const acc = noteName.includes('#') ? 1 : (noteName.includes('b') ? -1 : 0);
      const octave = parseInt(noteName.match(/-?\d+/)[0]);
      const noteIndex = notes.indexOf(letter.charAt(0).toUpperCase());
      return (octave + 1) * 12 + noteIndex + acc;
   }

   submitChord(notesPlayed) {
      if (!this.currentLesson) return;
      if (this.currentStepIndex >= this.currentLesson.sequence.length) return;

      const currentStep = this.currentLesson.sequence[this.currentStepIndex];
      const correctClasses = this.getCorrectPitchClasses(currentStep);

      let allCorrect = true;
      const stepResult = [];

      notesPlayed.forEach(note => {
         const noteClass = this.noteToMidiClass(note);
         const isCorrect = correctClasses.includes(noteClass);
         if (!isCorrect) allCorrect = false;
         stepResult.push({ note: note, correct: isCorrect });
      });

      this.history[this.currentStepIndex] = stepResult;

      const isSuccess = (allCorrect && notesPlayed.length > 0);
      if (isSuccess) {
         this.score += 10;
      } else {
         this.corrections[this.currentStepIndex] = this.generateCorrection(currentStep);
      }

      // Log Submission
      this.session.logEvent(this.getSessionTime(), "submit", {
         stepIndex: this.currentStepIndex,
         notes: notesPlayed,
         correct: isSuccess
      });

      if (this.currentStepIndex < this.currentLesson.sequence.length - 1) {
         this.currentStepIndex++;
         this.updateScoreboard();
         setTimeout(() => this.playBassForCurrentStep(), 500);
      } else {
         this.isFinished = true;
         this.accumulatedTime = this.getTotalTimeMS();
         this.isPlaying = false;
         this.stopTimer();
         // Save to DB
         this.session.flushLog(this.accumulatedTime, this.score);
         this.showFinalJudgment();
      }

      this.render();
   }

   showFinalJudgment() {
      const totalSteps = this.currentLesson.sequence.length;
      const maxScore = totalSteps * 10;
      const accuracy = this.score / maxScore;
      const totalSeconds = this.accumulatedTime / 1000;
      const secondsPerStep = totalSeconds / totalSteps;

      let msg = "";
      let icon = "";

      if (accuracy >= 0.9) {
         if (secondsPerStep < 3) {
            icon = "🏆"; msg = "Masterful! Fast and accurate.";
         } else {
            icon = "✅"; msg = "Excellent accuracy! Try to play faster next time.";
         }
      } else if (accuracy >= 0.7) {
         icon = "🙂"; msg = "Good job. Keep practicing to improve accuracy.";
      } else {
         icon = "⚠️"; msg = "Try again. Focus on hitting the right notes.";
      }

      const timeStr = this.formatTime(this.accumulatedTime);
      document.getElementById("scoreboard").innerHTML =
         `Score: ${this.score} | Time: ${timeStr} <span style="color:#2563eb; margin-left:15px;">${icon} ${msg}</span>`;
   }

   render(currentHeldNotes = new Set()) {
      if (!this.currentLesson) return;
      this.renderer.renderLessonState(
         this.currentLesson,
         this.currentStepIndex,
         this.history,
         currentHeldNotes,
         this.corrections
      );
   }
}

class InputHandler {
   constructor(manager, audio) {
      this.manager = manager;
      this.audio = audio;
      this.heldNotes = new Set();
      this.chordBuffer = new Set();
   }

   handleNoteOn(note, velocity = 80) {
      this.manager.notifyActivity();

      // Log RAW event
      this.manager.session.logEvent(this.manager.getSessionTime(), "noteOn", { note: note, velocity: velocity });

      if (document.getElementById("chkSoundUser").checked) {
         this.audio.playNote(note, 0.3, 'sine');
      }
      this.heldNotes.add(note);
      this.chordBuffer.add(note);

      this.manager.render(this.heldNotes);
      this.updateKeyboardUI();
   }

   handleNoteOff(note) {
      // Log RAW event
      this.manager.session.logEvent(this.manager.getSessionTime(), "noteOff", { note: note });

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
      this.manager.submitChord(notes);
      this.chordBuffer.clear();
   }

   onMidiMessage(msg) {
      const [status, data1, data2] = msg.data;
      const command = status >> 4;
      const noteNum = data1;
      const velocity = (data2 > 127) ? 127 : data2;
      const noteName = this.midiNumToNote(noteNum);

      if (command === 9 && velocity > 0) this.handleNoteOn(noteName, velocity);
      else if (command === 8 || (command === 9 && velocity === 0)) this.handleNoteOff(noteName);
   }

   midiNumToNote(num) {
      const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
      const octave = Math.floor(num / 12) - 1;
      return notes[num % 12] + octave;
   }
}

class Keyboard {
   static create(containerId, inputHandler) {
      const container = document.getElementById(containerId);
      const canvas = document.getElementById("sheet");

      // Use the actual canvas width for the keyboard container width
      const width = canvas.width;
      container.style.width = `${width}px`;
      container.innerHTML = "";

      const kWidth = CONFIG.KEY_WIDTH;
      const numKeysToDraw = Math.ceil(width / kWidth) + 5; // Draw slightly more keys than fit

      // Center C4 on the screen's center point
      const middleNoteOffset = 2; // C4 is the center index (0), C5 is +7, C3 is -7
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

      const c4Left = width / 2; // Approximate center

      whiteNotes.forEach(item => {
         const left = c4Left + (item.offset * kWidth) - (kWidth / 2); // Center of C4 should be at c4Left
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
         // Check if a black key exists (no black key between E-F and B-C)
         if (['C','D','F','G','A'].includes(letter)) {
            const blackNote = letter + '#' + item.note.slice(-1);
            const bKey = document.createElement("div");
            bKey.className = "black-key";
            // Position black key 1/3 across the white key
            bKey.style.left = `${left + kWidth - (blackKeyWidth / 2)}px`;
            bKey.style.width = `${blackKeyWidth}px`;
            bKey.style.height = `${blackKeyHeight}px`;
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

   // Initial setup
   renderer.resize();
   Keyboard.create("keyboard", inputHandler);

   // --- CANVAS DRAG/SCROLL LOGIC ---
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
      // The manager takes care of rendering and sets the viewportOffsetX
      manager.render(inputHandler.heldNotes, newOffset);
   };

   const endDrag = () => {
      isDragging = false;
      canvas.style.cursor = 'grab';
   };

   canvas.addEventListener('mousedown', (e) => startDrag(e.clientX));
   window.addEventListener('mousemove', (e) => moveDrag(e.clientX));
   window.addEventListener('mouseup', endDrag);

   canvas.addEventListener('touchstart', (e) => startDrag(e.touches[0].clientX), {passive: false});
   window.addEventListener('touchmove', (e) => moveDrag(e.touches[0].clientX), {passive: false});
   window.addEventListener('touchend', endDrag);
   // --- END CANVAS DRAG/SCROLL LOGIC ---

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

   // Lesson Selection and Controls
   const lessonSelect = document.getElementById("lessonSelect");
   LESSONS.forEach(l => {
      const opt = document.createElement("option");
      opt.value = l.id;
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

   // Initial Lesson Load
   manager.loadLesson(LESSONS[0].id);
});
