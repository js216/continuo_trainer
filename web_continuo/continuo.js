// ---------------------- Music Theory Constants ----------------------
const SHARP_ORDER = ['F', 'C', 'G', 'D', 'A', 'E', 'B'];
const FLAT_ORDER = ['B', 'E', 'A', 'D', 'G', 'C', 'F'];

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

// ---------------------- Lesson Definitions ----------------------
const LESSONS = [
  {
    id: "triads",
    name: "Lesson 1: Root Position Triads",
    description: "Play the 3rd and 5th above the bass. (Common figures: None, 5, 3)",
    defaultKey: 0,
    sequence: [
      { bass: "C3", figure: "" },
      { bass: "F3", figure: "" },
      { bass: "G3", figure: "" },
      { bass: "C3", figure: "" }
    ]
  },
  {
    id: "inversion1",
    name: "Lesson 2: First Inversion",
    description: "The figure '6' indicates a first inversion chord (intervals 3rd and 6th).",
    defaultKey: 1, // G Major
    sequence: [
      { bass: "B2", figure: "6" },
      { bass: "C3", figure: "6" },
      { bass: "D3", figure: "6" },
      { bass: "G2", figure: "" }
    ]
  },
  {
    id: "cadence",
    name: "Lesson 3: Simple Cadence",
    description: "Practice the I - IV - V - I progression.",
    defaultKey: 0,
    sequence: [
      { bass: "C3", figure: "" },
      { bass: "F2", figure: "" },
      { bass: "G2", figure: "" },
      { bass: "C3", figure: "" }
    ]
  },
  {
    id: "sevenths",
    name: "Lesson 4: Dominant Sevenths",
    description: "The figure '7' adds a 7th above the bass.",
    defaultKey: 0,
    sequence: [
      { bass: "C3", figure: "" },
      { bass: "G2", figure: "7" },
      { bass: "C3", figure: "" },
      { bass: "D3", figure: "7" },
      { bass: "G2", figure: "" }
    ]
  }
];

// ---------------------- CanvasRenderer ----------------------
class CanvasRenderer {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");

    this.staffTop = 50;
    this.lineSpacing = 10;
    this.numLines = 5;
    this.staffGap = 80; // Distance between treble and bass staves
    
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

  drawNote(note, x, clef, color = "black", alpha = 1.0) {
    const y = this.getNoteY(note, clef);
    const ctx = this.ctx;
    
    ctx.globalAlpha = alpha;
    ctx.fillStyle = color;
    ctx.strokeStyle = color;
    
    // Head
    ctx.beginPath();
    ctx.arc(x, y, this.lineSpacing / 2, 0, 2 * Math.PI);
    ctx.fill();
    
    // Stem
    ctx.beginPath();
    ctx.moveTo(x + this.lineSpacing / 2, y);
    ctx.lineTo(x + this.lineSpacing / 2, y - 35);
    ctx.stroke();

    // Accidental
    const accidental = this.getAccidentalSymbol(note, this.currentKeySig);
    if (accidental) {
      ctx.font = `${this.lineSpacing * 2}px serif`;
      ctx.fillText(accidental, x - 15, y + 5);
    }

    // Ledger Lines
    ctx.strokeStyle = "black"; // Ledgers always black
    const topLineY = clef === "treble" ? this.staffTop : this.staffTop + this.staffGap;
    const bottomLineY = topLineY + (this.numLines - 1) * this.lineSpacing;

    if (y < topLineY) {
      for (let ly = topLineY - this.lineSpacing; ly >= y - 1; ly -= this.lineSpacing) {
        ctx.beginPath(); ctx.moveTo(x - 8, ly); ctx.lineTo(x + 8, ly); ctx.stroke();
      }
    }
    if (y > bottomLineY) {
      for (let ly = bottomLineY + this.lineSpacing; ly <= y + 1; ly += this.lineSpacing) {
        ctx.beginPath(); ctx.moveTo(x - 8, ly); ctx.lineTo(x + 8, ly); ctx.stroke();
      }
    }
    ctx.globalAlpha = 1.0;
  }

  drawFigure(figure, x, note, clef) {
    const y = this.getNoteY(note, clef) - 30;
    this.ctx.font = `bold ${this.lineSpacing * 1.4}px serif`;
    this.ctx.fillStyle = "black";
    this.ctx.fillText(figure, x, y);
  }

  // The main render loop
  renderLessonState(lesson, currentStepIndex, userHistory, currentHeldNotes) {
    this.currentKeySig = lesson.defaultKey; 
    this.clear();
    this.drawStaves();
    this.drawKeySignature(lesson.defaultKey);

    const startX = 160;
    const spacing = 80;

    // 1. Draw the fixed sequence of Bass notes
    lesson.sequence.forEach((step, index) => {
      const x = startX + (index * spacing);
      
      // Highlight current measure background lightly
      if (index === currentStepIndex) {
        this.ctx.fillStyle = "rgba(255, 255, 0, 0.2)";
        this.ctx.fillRect(x - 30, 0, 60, this.canvas.height);
      }

      // Draw Bass Note
      this.drawNote(step.bass, x, "bass", "black");
      this.drawFigure(step.figure, x, step.bass, "bass");

      // Draw User History for this step (if any)
      if (userHistory[index]) {
        userHistory[index].forEach(attempt => {
           // Determine clef based on octave
           const octave = parseInt(attempt.note.match(/-?\d+/)[0]);
           const clef = octave >= 4 ? 'treble' : 'bass';
           const color = attempt.correct ? '#22c55e' : '#ef4444';
           this.drawNote(attempt.note, x, clef, color);
        });
      }
    });

    // 2. Draw currently held notes (Live Feedback)
    // These are drawn at the current step index position
    if (currentHeldNotes && currentHeldNotes.size > 0) {
      const x = startX + (currentStepIndex * spacing);
      currentHeldNotes.forEach(note => {
        const octave = parseInt(note.match(/-?\d+/)[0]);
        const clef = octave >= 4 ? 'treble' : 'bass';
        // Draw visually distinct (blue, semi-transparent) until submitted
        this.drawNote(note, x, clef, 'blue', 0.7);
      });
    }
  }
}

// ---------------------- Logic / State Manager ----------------------
class LessonManager {
  constructor(renderer) {
    this.renderer = renderer;
    this.currentLesson = null;
    this.currentStepIndex = 0;
    
    // History: Array of arrays. history[stepIndex] = [{note: 'C4', correct: true}, ...]
    this.history = []; 
    this.score = 0;
  }

  loadLesson(lessonId) {
    // Find lesson data
    const data = LESSONS.find(l => l.id === lessonId) || LESSONS[0];
    
    // Clone to avoid mutating const
    this.currentLesson = JSON.parse(JSON.stringify(data)); 
    
    // Reset State
    this.currentStepIndex = 0;
    this.history = new Array(this.currentLesson.sequence.length).fill(null).map(() => []);
    this.score = 0;

    // Update UI
    document.getElementById("lessonDescription").textContent = this.currentLesson.description;
    document.getElementById("keySig").value = this.currentLesson.defaultKey;
    document.getElementById("scoreboard").textContent = `Score: 0`;

    this.render();
  }

  updateKeySig(newKey) {
    if (this.currentLesson) {
      this.currentLesson.defaultKey = parseInt(newKey);
      this.render();
    }
  }

  // Logic to calculate correct notes for the current step
  getCorrectNotesForStep(step) {
    const { bass, figure } = step;
    const scale = KEY_SCALES[this.currentLesson.defaultKey.toString()];
    
    const bassLetter = bass.replace(/[\d-]+/, '');
    let scaleIndex = scale.indexOf(bassLetter);
    if (scaleIndex === -1) scaleIndex = 0; // Fallback

    // Interpret Figure
    let intervals = [0, 2, 4]; // Default triad
    if (figure === "6") intervals = [0, 2, 5];
    if (figure === "6/4") intervals = [0, 3, 5];
    if (figure === "7") intervals = [0, 2, 4, 6];
    if (figure === "6/5") intervals = [0, 2, 4, 5];

    const correctPitchClasses = [];
    
    intervals.forEach(iv => {
      const targetIndex = (scaleIndex + iv) % 7;
      const noteName = scale[targetIndex];
      // Convert scale note (e.g., F#) to midi class (0-11)
      correctPitchClasses.push(this.noteToMidiClass(noteName));
    });

    return correctPitchClasses;
  }

  noteToMidiClass(noteName) {
    const map = { 'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11 };
    const letter = noteName.replace(/[\d#b-]+/, '');
    const acc = noteName.includes('#') ? 1 : (noteName.includes('b') ? -1 : 0);
    return (map[letter] + acc + 12) % 12;
  }

  // Called when user releases all keys
  submitChord(notesPlayed) {
    if (!this.currentLesson) return;
    if (this.currentStepIndex >= this.currentLesson.sequence.length) return;

    const currentStep = this.currentLesson.sequence[this.currentStepIndex];
    const correctClasses = this.getCorrectNotesForStep(currentStep);

    // Evaluate each played note
    const stepResult = [];
    let allCorrect = true;

    notesPlayed.forEach(note => {
       const noteClass = this.noteToMidiClass(note);
       const isCorrect = correctClasses.includes(noteClass);
       if (!isCorrect) allCorrect = false;
       stepResult.push({ note: note, correct: isCorrect });
    });

    // Save to history
    this.history[this.currentStepIndex] = stepResult;

    // Scoring (simple: if all notes played were part of the chord)
    if (allCorrect && notesPlayed.length > 0) {
      this.score += 10;
    }

    document.getElementById("scoreboard").textContent = `Score: ${this.score}`;

    // Advance step
    if (this.currentStepIndex < this.currentLesson.sequence.length - 1) {
      this.currentStepIndex++;
    } else {
      // End of lesson? (Could loop or alert)
      // For now, just stay on last step
    }

    this.render();
  }

  render(currentHeldNotes = new Set()) {
    if (!this.currentLesson) return;
    this.renderer.renderLessonState(
      this.currentLesson, 
      this.currentStepIndex, 
      this.history,
      currentHeldNotes
    );
  }
}

// ---------------------- Input Handler (Chord Logic) ----------------------
class InputHandler {
  constructor(manager) {
    this.manager = manager;
    this.heldNotes = new Set(); // Currently holding down
    this.chordBuffer = new Set(); // All notes pressed in this "gesture"
    this.timeout = null;
  }

  handleNoteOn(note) {
    this.heldNotes.add(note);
    this.chordBuffer.add(note);
    this.updateVisuals();
  }

  handleNoteOff(note) {
    this.heldNotes.delete(note);
    this.updateVisuals();

    // Logic: When Last Key is released, submit the chord
    if (this.heldNotes.size === 0 && this.chordBuffer.size > 0) {
      // Small delay to ensure it's not just a jittery finger lift during a chord change
      // though "upon release" usually implies immediate.
      this.submit();
    }
  }

  updateVisuals() {
    // Tell manager to draw held notes
    this.manager.render(this.heldNotes);
    
    // Update Keyboard UI classes
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
    this.chordBuffer.clear(); // Reset for next chord
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

    // Helper to attach pointer events
    const attachEvents = (el, note) => {
      el.dataset.note = note; // Store note in DOM for easy access
      
      el.addEventListener("pointerdown", (e) => {
        e.preventDefault();
        el.setPointerCapture(e.pointerId);
        inputHandler.handleNoteOn(note);
      });

      el.addEventListener("pointerup", (e) => {
        e.preventDefault();
        inputHandler.handleNoteOff(note);
      });

      el.addEventListener("pointercancel", (e) => {
         inputHandler.handleNoteOff(note);
      });
    };

    whiteNotes.forEach((note, i) => {
      const key = document.createElement("div");
      key.className = "white-key";
      key.style.left = `${i * whiteKeyWidth}px`;
      key.style.width = `${whiteKeyWidth}px`;
      key.style.height = `${whiteKeyHeight}px`;
      key.style.display = "flex";
      key.style.alignItems = "flex-end";
      key.style.justifyContent = "center";
      key.style.paddingBottom = "5px";
      key.style.fontSize = "10px";
      key.style.fontWeight = "bold";
      key.textContent = note;
      
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
  const renderer = new CanvasRenderer(document.getElementById("sheet"));
  const manager = new LessonManager(renderer);
  const inputHandler = new InputHandler(manager);

  // Populate Lesson Dropdown
  const lessonSelect = document.getElementById("lessonSelect");
  LESSONS.forEach(l => {
    const opt = document.createElement("option");
    opt.value = l.id;
    opt.textContent = l.name;
    lessonSelect.appendChild(opt);
  });

  // Event Listeners
  lessonSelect.addEventListener("change", (e) => manager.loadLesson(e.target.value));
  
  document.getElementById("keySig").addEventListener("change", (e) => {
    manager.updateKeySig(e.target.value);
  });

  // Generate Keyboard
  Keyboard.create("keyboard", inputHandler);

  // Start first lesson
  manager.loadLesson(LESSONS[0].id);
});
