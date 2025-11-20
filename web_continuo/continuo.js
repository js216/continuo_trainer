// ---------------------- Audio Engine ----------------------
class AudioEngine {
   constructor() {
      this.ctx = null;
      this.masterGain = null;
   }

   init() {
      if (!this.ctx) {
         this.ctx = new (window.AudioContext || window.webkitAudioContext)();
         this.masterGain = this.ctx.createGain();
         this.masterGain.gain.value = 0.3; // Master volume
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

      // Envelope
      gain.gain.setValueAtTime(0, this.ctx.currentTime);
      gain.gain.linearRampToValueAtTime(1, this.ctx.currentTime + 0.05);
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

      // A4 is 440Hz. A4 index is 0.
      // Steps from A4
      const semitones = (octave - 4) * 12 + noteMap[letter] + acc;
      return 440 * Math.pow(2, semitones / 12);
   }
}

// ---------------------- Music Constants & Lessons ----------------------
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

// Duration: 4 = Whole, 2 = Half, 1 = Quarter
const LESSONS = [
   {
      id: "triads",
      name: "Lesson 1: Root Position Triads",
      description: "Play the 3rd and 5th above the bass. (Common figures: None, 5, 3)",
      defaultKey: 0,
      timeSignature: [4, 4],
      sequence: [
         { bass: "C3", figure: "", duration: 2 },
         { bass: "G3", figure: "", duration: 2 },
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
         { bass: "C3", figure: "", duration: 4 }
      ]
   },
   {
      id: "inversion1",
      name: "Lesson 2: First Inversion",
      description: "The figure '6' indicates a first inversion chord.",
      defaultKey: 1, // G Major
      timeSignature: [4, 4],
      sequence: [
         { bass: "B2", figure: "6", duration: 2 },
         { bass: "C3", figure: "6", duration: 2 },
         { bass: "D3", figure: "6", duration: 2 },
         { bass: "G2", figure: "", duration: 2 }
      ]
   },
   {
      id: "cadence",
      name: "Lesson 3: Simple Cadence",
      description: "I - IV - V - I progression.",
      defaultKey: 0,
      timeSignature: [3, 4],
      sequence: [
         { bass: "C3", figure: "", duration: 1 },
         { bass: "F2", figure: "", duration: 1 },
         { bass: "G2", figure: "", duration: 1 },
         { bass: "C3", figure: "", duration: 3 }
      ]
   },
   {
      id: "sevenths",
      name: "Lesson 4: Dominant Sevenths",
      description: "The figure '7' adds a 7th above the bass.",
      defaultKey: 0,
      timeSignature: [4, 4],
      sequence: [
         { bass: "C3", figure: "", duration: 2 },
         { bass: "G2", figure: "7", duration: 2 },
         { bass: "C3", figure: "", duration: 4 }
      ]
   }
];

// ---------------------- CanvasRenderer ----------------------
class CanvasRenderer {
   constructor(canvas) {
      this.canvas = canvas;
      this.ctx = canvas.getContext("2d");

      this.staffTop = 60;
      this.lineSpacing = 10;
      this.numLines = 5;
      this.staffGap = 90; 
      this.currentKeySig = 0; 
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
      ctx.strokeStyle = "#000";
      ctx.lineWidth = 1;
      ctx.fillStyle = "#000";

      // Treble
      for (let i = 0; i < this.numLines; i++) {
         const y = this.staffTop + i * this.lineSpacing;
         ctx.beginPath(); ctx.moveTo(10, y); ctx.lineTo(this.canvas.width - 10, y); ctx.stroke();
      }
      // Bass
      const bassTop = this.staffTop + this.staffGap;
      for (let i = 0; i < this.numLines; i++) {
         const y = bassTop + i * this.lineSpacing;
         ctx.beginPath(); ctx.moveTo(10, y); ctx.lineTo(this.canvas.width - 10, y); ctx.stroke();
      }

      // Clefs
      ctx.font = `${this.lineSpacing * 3.5}px serif`;
      ctx.fillText("𝄞", 10, this.staffTop + 30);
      ctx.fillText("𝄢", 10, bassTop + 25);
   }

   drawKeySignature(num) {
      const ctx = this.ctx;
      ctx.font = `${this.lineSpacing * 2}px serif`;
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
         const x = xStart + (i * 12);
         ctx.fillText(symbol, x, this.getNoteY(targetT[i], 'treble') + 4);
         ctx.fillText(symbol, x, this.getNoteY(targetB[i], 'bass') + 4);
      }
   }

   drawTimeSignature(timeSig) {
      if (!timeSig) return;
      const [num, den] = timeSig;
      const x = 50 + (Math.abs(this.currentKeySig) * 12) + 15;
      const ctx = this.ctx;
      ctx.font = `bold ${this.lineSpacing * 2}px serif`;
      // Treble
      ctx.fillText(num, x, this.staffTop + 20);
      ctx.fillText(den, x, this.staffTop + 40);
      // Bass
      ctx.fillText(num, x, this.staffTop + this.staffGap + 20);
      ctx.fillText(den, x, this.staffTop + this.staffGap + 40);
      return x + 30; // Return X offset for notes
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

   // Duration: 4=Whole, 2=Half, 1=Quarter
   drawNote(note, x, clef, color = "black", duration = 4, alpha = 1.0) {
      const y = this.getNoteY(note, clef);
      const ctx = this.ctx;

      ctx.globalAlpha = alpha;
      ctx.fillStyle = color;
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      // Notehead size tuned to staff geometry
      const fontSize = this.lineSpacing * 5.5; // Strongly scaled up
      ctx.font = `${fontSize}px serif`;

      // Correct duration mapping
      let glyph;
      if (duration >= 4) glyph = "𝅝";   // Whole
      else if (duration >= 2) glyph = "𝅗𝅥"; // Half
      else glyph = "♩";                 // Quarter

      // Raise glyph center into proper pitch position
      const yOffset = -this.lineSpacing * 1.1;

      ctx.fillText(glyph, x-0.9*this.lineSpacing, y + yOffset);

      // Accidentals — scale up proportionally
      const accidental = this.getAccidentalSymbol(note, this.currentKeySig);
      if (accidental) {
         ctx.font = `${this.lineSpacing * 3.3}px serif`;
         ctx.fillText(accidental, x - this.lineSpacing * 3.0, y + yOffset);
      }

      // ------- Ledger Lines (unchanged) -------
      ctx.strokeStyle = color;
      ctx.lineWidth = 1;
      const topLineY = clef === "treble" ? this.staffTop : this.staffTop + this.staffGap;
      const bottomLineY = topLineY + (this.numLines - 1) * this.lineSpacing;

      const drawLedger = (ly) => {
         ctx.beginPath();
         ctx.moveTo(x - 1.5*this.lineSpacing, ly);
         ctx.lineTo(x + 1.5*this.lineSpacing, ly);
         ctx.stroke();
      };

      if (y < topLineY) {
         for (let ly = topLineY - this.lineSpacing; ly >= y; ly -= this.lineSpacing) {
            drawLedger(ly);
         }
      }
      if (y > bottomLineY) {
         for (let ly = bottomLineY + this.lineSpacing; ly <= y; ly += this.lineSpacing) {
            drawLedger(ly);
         }
      }

      ctx.globalAlpha = 1.0;
   }


   drawFigure(figure, x, note, clef) {
      const y = this.getNoteY(note, clef) - 35;
      this.ctx.font = `bold ${this.lineSpacing * 1.4}px serif`;
      this.ctx.fillStyle = "black";
      this.ctx.fillText(figure, x, y);
   }

   drawBarLine(x) {
      const ctx = this.ctx;
      ctx.beginPath();
      ctx.moveTo(x, this.staffTop);
      ctx.lineTo(x, this.staffTop + (this.numLines-1)*this.lineSpacing);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(x, this.staffTop + this.staffGap);
      ctx.lineTo(x, this.staffTop + this.staffGap + (this.numLines-1)*this.lineSpacing);
      ctx.stroke();
   }

   drawFinalBarLine(x) {
      const ctx = this.ctx;
      // Thin line
      this.drawBarLine(x);
      // Thick line
      ctx.lineWidth = 4;
      ctx.beginPath();
      ctx.moveTo(x + 6, this.staffTop);
      ctx.lineTo(x + 6, this.staffTop + (this.numLines-1)*this.lineSpacing);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(x + 6, this.staffTop + this.staffGap);
      ctx.lineTo(x + 6, this.staffTop + this.staffGap + (this.numLines-1)*this.lineSpacing);
      ctx.stroke();
      ctx.lineWidth = 1;
   }

   renderLessonState(lesson, currentStepIndex, userHistory, currentHeldNotes, correctionNotes) {
      this.currentKeySig = lesson.defaultKey; 
      this.clear();
      this.drawStaves();
      this.drawKeySignature(lesson.defaultKey);
      let cursorX = this.drawTimeSignature(lesson.timeSignature);

      // Spacing based on duration (simple scaling)
      // 1 beat = 60px
      const beatPixels = 60;
      let currentMeasureBeats = 0;
      const beatsPerMeasure = lesson.timeSignature[0];

      lesson.sequence.forEach((step, index) => {
         const x = cursorX + (beatPixels * (step.duration / 2)); // Centered in its time slot approx

         // Highlight current measure
         if (index === currentStepIndex) {
            this.ctx.fillStyle = "rgba(255, 255, 0, 0.2)";
            this.ctx.fillRect(cursorX, 0, beatPixels * step.duration, this.canvas.height);
         }

         // Draw Bass
         this.drawNote(step.bass, x, "bass", "black", step.duration);
         this.drawFigure(step.figure, x, step.bass, "bass");

         // Draw User History
         if (userHistory[index]) {
            userHistory[index].forEach(attempt => {
               const octave = parseInt(attempt.note.match(/-?\d+/)[0]);
               const clef = octave >= 4 ? 'treble' : 'bass';
               const color = attempt.correct ? '#22c55e' : '#ef4444';
               this.drawNote(attempt.note, x, clef, color, step.duration);
            });

            // Draw Corrections (Yellow) if they exist for this step
            if (correctionNotes && correctionNotes[index]) {
               correctionNotes[index].forEach(note => {
                  const octave = parseInt(note.match(/-?\d+/)[0]);
                  const clef = octave >= 4 ? 'treble' : 'bass';
                  // Offset slightly to right (x + 20)
                  this.drawNote(note, x + 15, clef, '#eab308', step.duration); 
               });
            }
         }

         // Draw Active Held Notes (only for current step)
         if (index === currentStepIndex && currentHeldNotes && currentHeldNotes.size > 0) {
            currentHeldNotes.forEach(note => {
               const octave = parseInt(note.match(/-?\d+/)[0]);
               const clef = octave >= 4 ? 'treble' : 'bass';
               this.drawNote(note, x, clef, 'blue', step.duration, 0.7);
            });
         }

         // Advance Cursor
         cursorX += beatPixels * step.duration;
         currentMeasureBeats += step.duration;

         // Draw Barlines
         if (currentMeasureBeats >= beatsPerMeasure) {
            this.drawBarLine(cursorX);
            currentMeasureBeats = 0; // simplistic reset
         }
      });

      // Final Barline
      this.drawFinalBarLine(cursorX);
   }
}

// ---------------------- Logic / State Manager ----------------------
class LessonManager {
   constructor(renderer, audio) {
      this.renderer = renderer;
      this.audio = audio;
      this.currentLesson = null;
      this.currentStepIndex = 0;

      this.history = []; 
      this.corrections = {}; // Map stepIndex -> [Notes]
      this.score = 0;
   }

   loadLesson(lessonId) {
      const data = LESSONS.find(l => l.id === lessonId) || LESSONS[0];
      this.currentLesson = JSON.parse(JSON.stringify(data)); 

      this.currentStepIndex = 0;
      this.history = new Array(this.currentLesson.sequence.length).fill(null).map(() => []);
      this.corrections = {};
      this.score = 0;

      document.getElementById("lessonDescription").textContent = this.currentLesson.description;
      document.getElementById("scoreboard").textContent = `Score: 0`;

      this.render();
      this.playBassForCurrentStep();
   }

   playBassForCurrentStep() {
      if (!document.getElementById("chkSoundBass").checked) return;
      if (!this.currentLesson || this.currentStepIndex >= this.currentLesson.sequence.length) return;

      const step = this.currentLesson.sequence[this.currentStepIndex];
      // Play slightly longer for whole notes? simplified to 0.5s or 1s
      this.audio.playNote(step.bass, 0.5, 'triangle');
   }

   getCorrectPitchClasses(step) {
      const { bass, figure } = step;
      const scale = KEY_SCALES[this.currentLesson.defaultKey.toString()];
      const bassLetter = bass.replace(/[\d-]+/, '');
      let scaleIndex = scale.indexOf(bassLetter);
      if (scaleIndex === -1) scaleIndex = 0;

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
      // Simple realization: Find notes in 4th octave that match pitch classes
      const classes = this.getCorrectPitchClasses(step);
      const correction = [];
      // Brute force check C4 to G5
      const range = ['C4','C#4','D4','D#4','E4','F4','F#4','G4','G#4','A4','A#4','B4',
         'C5','C#5','D5','D#5','E5','F5','F#5','G5'];

      // We just need one note per pitch class to show a "chord"
      // This is a naive voicing generator but sufficient for visual feedback
      const usedClasses = new Set();

      range.forEach(note => {
         const nc = this.noteToMidiClass(note);
         if (classes.includes(nc) && !usedClasses.has(nc)) {
            correction.push(note);
            usedClasses.add(nc);
         }
      });
      return correction.slice(0, classes.length); // Limit to number of voices needed
   }

   noteToMidiClass(noteName) {
      const map = { 'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11 };
      const letter = noteName.replace(/[\d#b-]+/, '');
      const acc = noteName.includes('#') ? 1 : (noteName.includes('b') ? -1 : 0);
      return (map[letter] + acc + 12) % 12;
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

      // Must play at least correct number of notes? For now, just check if what was played is right.
      // Strict: must find all classes? Let's stay simple.

      this.history[this.currentStepIndex] = stepResult;

      if (allCorrect && notesPlayed.length > 0) {
         this.score += 10;
      } else {
         // Generate Correction
         this.corrections[this.currentStepIndex] = this.generateCorrection(currentStep);
      }

      document.getElementById("scoreboard").textContent = `Score: ${this.score}`;

      if (this.currentStepIndex < this.currentLesson.sequence.length - 1) {
         this.currentStepIndex++;
         setTimeout(() => this.playBassForCurrentStep(), 500); // Play next bass after short delay
      }

      this.render();
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

// ---------------------- Input Handler ----------------------
class InputHandler {
   constructor(manager, audio) {
      this.manager = manager;
      this.audio = audio;
      this.heldNotes = new Set();
      this.chordBuffer = new Set();
   }

   handleNoteOn(note) {
      // Audio Feedback
      if (document.getElementById("chkSoundUser").checked) {
         this.audio.playNote(note, 0.3, 'sine');
      }

      this.heldNotes.add(note);
      this.chordBuffer.add(note);
      this.manager.render(this.heldNotes);
      this.updateKeyboardUI();
   }

   handleNoteOff(note) {
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

   // MIDI Handling
   onMidiMessage(msg) {
      const [status, data1, data2] = msg.data;
      const command = status >> 4;
      const noteNum = data1;
      const velocity = (data2 > 127) ? 127 : data2; // Safety

      const noteName = this.midiNumToNote(noteNum);

      // Note On (command 9) with velocity > 0
      if (command === 9 && velocity > 0) {
         this.handleNoteOn(noteName);
      }
      // Note Off (command 8) or Note On with velocity 0
      else if (command === 8 || (command === 9 && velocity === 0)) {
         this.handleNoteOff(noteName);
      }
   }

   midiNumToNote(num) {
      const notes = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
      const octave = Math.floor(num / 12) - 1;
      const note = notes[num % 12];
      return note + octave;
   }
}

// ---------------------- Keyboard ----------------------
class Keyboard {
   static create(containerId, inputHandler) {
      const container = document.getElementById(containerId);
      container.innerHTML = "";

      const whiteKeyWidth = 40;
      const whiteKeyHeight = 120;
      const blackKeyWidth = 25;
      const blackKeyHeight = 80;

      const whiteNotes = [
         'C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4',
         'C5', 'D5', 'E5', 'F5', 'G5', 'A5', 'B5',
         'C6'
      ];

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

      whiteNotes.forEach((note, i) => {
         const key = document.createElement("div");
         key.className = "white-key";
         key.style.left = `${i * whiteKeyWidth}px`;
         key.style.width = `${whiteKeyWidth}px`;
         key.style.height = `${whiteKeyHeight}px`;
         key.textContent = note;
         key.style.display = "flex"; key.style.alignItems="flex-end"; key.style.justifyContent="center";
         key.style.fontSize="10px"; key.style.paddingBottom="5px"; key.style.fontWeight="bold";
         attachEvents(key, note);
         container.appendChild(key);
      });

      const blackKeys = [
         { note: 'C#4', afterWhiteIndex: 0 },
         { note: 'D#4', afterWhiteIndex: 1 },
         { note: 'F#4', afterWhiteIndex: 3 },
         { note: 'G#4', afterWhiteIndex: 4 },
         { note: 'A#4', afterWhiteIndex: 5 },
         { note: 'C#5', afterWhiteIndex: 7 },
         { note: 'D#5', afterWhiteIndex: 8 },
         { note: 'F#5', afterWhiteIndex: 10 },
         { note: 'G#5', afterWhiteIndex: 11 },
         { note: 'A#5', afterWhiteIndex: 12 }
      ];

      blackKeys.forEach(({ note, afterWhiteIndex }) => {
         const key = document.createElement("div");
         key.className = "black-key";
         key.style.left = `${(afterWhiteIndex + 1) * whiteKeyWidth - blackKeyWidth / 2}px`;
         key.style.width = `${blackKeyWidth}px`;
         key.style.height = `${blackKeyHeight}px`;
         attachEvents(key, note);
         container.appendChild(key);
      });

      container.style.width = `${whiteNotes.length * whiteKeyWidth}px`;
   }
}

// ---------------------- Init ----------------------
window.addEventListener("DOMContentLoaded", () => {
   const audio = new AudioEngine();
   const renderer = new CanvasRenderer(document.getElementById("sheet"));
   const manager = new LessonManager(renderer, audio);
   const inputHandler = new InputHandler(manager, audio);

   // Populate UI
   const lessonSelect = document.getElementById("lessonSelect");
   LESSONS.forEach(l => {
      const opt = document.createElement("option");
      opt.value = l.id;
      opt.textContent = l.name;
      lessonSelect.appendChild(opt);
   });

   lessonSelect.addEventListener("change", (e) => manager.loadLesson(e.target.value));

   // MIDI Connect
   document.getElementById("midiBtn").addEventListener("click", () => {
      // Init audio context on user gesture as well
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
         }, () => {
            alert("MIDI Access Failed or Not Supported");
         });
      } else {
         alert("Web MIDI API not supported in this browser.");
      }
   });

   Keyboard.create("keyboard", inputHandler);
   manager.loadLesson(LESSONS[0].id);
});
