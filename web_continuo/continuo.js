// ---------------------- Music Theory Constants ----------------------
const SHARP_ORDER = ['F', 'C', 'G', 'D', 'A', 'E', 'B'];
const FLAT_ORDER = ['B', 'E', 'A', 'D', 'G', 'C', 'F'];

const KEY_SCALES = {
  "-7": ['Cb', 'Db', 'Eb', 'Fb', 'Gb', 'Ab', 'Bb'], // Cb Major
  "-6": ['Gb', 'Ab', 'Bb', 'Cb', 'Db', 'Eb', 'F'],  // Gb Major
  "-5": ['Db', 'Eb', 'F', 'Gb', 'Ab', 'Bb', 'C'],   // Db Major
  "-4": ['Ab', 'Bb', 'C', 'Db', 'Eb', 'F', 'G'],    // Ab Major
  "-3": ['Eb', 'F', 'G', 'Ab', 'Bb', 'C', 'D'],     // Eb Major
  "-2": ['Bb', 'C', 'D', 'Eb', 'F', 'G', 'A'],      // Bb Major
  "-1": ['F', 'G', 'A', 'Bb', 'C', 'D', 'E'],       // F Major
  "0":  ['C', 'D', 'E', 'F', 'G', 'A', 'B'],        // C Major
  "1":  ['G', 'A', 'B', 'C', 'D', 'E', 'F#'],       // G Major
  "2":  ['D', 'E', 'F#', 'G', 'A', 'B', 'C#'],      // D Major
  "3":  ['A', 'B', 'C#', 'D', 'E', 'F#', 'G#'],     // A Major
  "4":  ['E', 'F#', 'G#', 'A', 'B', 'C#', 'D#'],    // E Major
  "5":  ['B', 'C#', 'D#', 'E', 'F#', 'G#', 'A#'],   // B Major
  "6":  ['F#', 'G#', 'A#', 'B', 'C#', 'D#', 'E#'],  // F# Major
  "7":  ['C#', 'D#', 'E#', 'F#', 'G#', 'A#', 'B#']  // C# Major
};

// ---------------------- CanvasRenderer ----------------------
class CanvasRenderer {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");

    this.staffTop = 40;
    this.lineSpacing = 12;
    this.numLines = 5;
    this.staffGap = 80;
    
    // Current Key Signature (controlled by generator)
    this.currentKeySig = 0; 
  }

  getDiatonicRank(note) {
    const noteName = note.replace(/[\d#b]+/, ''); 
    const octave = parseInt(note.match(/-?\d+/)[0]);
    const ranks = { 'C': 0, 'D': 1, 'E': 2, 'F': 3, 'G': 4, 'A': 5, 'B': 6 };
    return (octave * 7) + ranks[noteName];
  }

  getNoteY(note, clef) {
    const trebleRefRank = this.getDiatonicRank("B4"); // Middle line treble
    const bassRefRank = this.getDiatonicRank("D3");   // Middle line bass
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

  drawStaff(yTop) {
    const ctx = this.ctx;
    ctx.strokeStyle = "#000";
    ctx.lineWidth = 1;
    ctx.fillStyle = "#000";
    for (let i = 0; i < this.numLines; i++) {
      const y = yTop + i * this.lineSpacing;
      ctx.beginPath();
      ctx.moveTo(10, y);
      ctx.lineTo(this.canvas.width - 10, y);
      ctx.stroke();
    }
  }

  drawClef(clef, x = 20) {
    const ctx = this.ctx;
    const y = clef === "treble"
      ? this.staffTop + 2 * this.lineSpacing + 5
      : this.staffTop + this.staffGap + 2 * this.lineSpacing + 5;
    ctx.font = `${this.lineSpacing * 3}px serif`;
    ctx.fillStyle = "black";
    ctx.fillText(clef === "treble" ? "𝄞" : "𝄢", x, y);
  }

  drawKeySignature(num, xStart) {
    const ctx = this.ctx;
    ctx.font = `${this.lineSpacing * 2}px serif`;
    const isSharp = num > 0;
    const count = Math.abs(num);
    const symbol = isSharp ? "♯" : "♭";
    
    // Offsets for sharps/flats pattern on staff (roughly)
    const sharpOffsetsTreble = [0, 1.5, -0.5, 1, 2.5, 0.5, 2]; // relative to top line F5
    const flatOffsetsTreble = [2, 0.5, 2.5, 1, 3, 1.5, 3.5]; // relative to middle line B4
    
    const sharpOffsetsBass = [1, 2.5, 0.5, 2, 3.5, 1.5, 3]; // relative to top line A3
    const flatOffsetsBass = [3, 1.5, 3.5, 2, 4, 2.5, 4.5]; // relative to middle line D3

    for(let i=0; i<count; i++) {
      const x = xStart + (i * 10);
      
      // Treble
      let yOff = isSharp ? sharpOffsetsTreble[i] * this.lineSpacing : flatOffsetsTreble[i] * this.lineSpacing;
      // Adjust base for treble (Top line is F5 for sharp ref, B4 for flat ref approx)
      let base = isSharp ? this.staffTop : this.staffTop + 2 * this.lineSpacing; 
      // Hardcoded adjustments to look right
      if(isSharp) base = this.staffTop; // F5 line
      else base = this.staffTop + 2*this.lineSpacing; // B4 line

      // Actually, simpler to map specific notes
      let tNote = isSharp ? SHARP_ORDER[i] + '5' : FLAT_ORDER[i] + '4';
      if(isSharp && ['A','B'].includes(SHARP_ORDER[i])) tNote = SHARP_ORDER[i] + '4'; 
      if(!isSharp && ['C','F','G'].includes(FLAT_ORDER[i])) tNote = FLAT_ORDER[i] + '5';
      
      let y = this.getNoteY(tNote, 'treble');
      ctx.fillText(symbol, x, y + this.lineSpacing/2);

      // Bass
      let bNote = isSharp ? SHARP_ORDER[i] + '3' : FLAT_ORDER[i] + '2';
      if(isSharp && ['A','B'].includes(SHARP_ORDER[i])) bNote = SHARP_ORDER[i] + '2';
      if(!isSharp && ['C','F','G'].includes(FLAT_ORDER[i])) bNote = FLAT_ORDER[i] + '3';

      y = this.getNoteY(bNote, 'bass');
      ctx.fillText(symbol, x, y + this.lineSpacing/2);
    }
  }

  // Returns the accidental symbol needed for this note in the current key
  getAccidentalSymbol(note, keySig) {
    const letter = note.replace(/[\d#b]+/, '');
    const acc = note.includes('#') ? '#' : (note.includes('b') ? 'b' : '');
    
    const scale = KEY_SCALES[keySig.toString()];
    // Find what this letter is in the current key
    const keyNote = scale.find(n => n.startsWith(letter));
    const keyAcc = keyNote.includes('#') ? '#' : (keyNote.includes('b') ? 'b' : '');

    if (acc === keyAcc) return null; // No accidental needed (diatonic)
    if (acc === '#' && keyAcc !== '#') return '♯';
    if (acc === 'b' && keyAcc !== 'b') return '♭';
    if (acc === '' && keyAcc !== '') return '♮';
    return null;
  }

  drawNote(note, x, clef, color = "black") {
    const y = this.getNoteY(note, clef);
    const ctx = this.ctx;
    
    // 1. Draw Note Head and Stem
    ctx.fillStyle = color;
    ctx.strokeStyle = color;
    
    ctx.beginPath();
    ctx.arc(x, y, this.lineSpacing / 2, 0, 2 * Math.PI);
    ctx.fill();
    
    ctx.beginPath();
    ctx.moveTo(x + this.lineSpacing / 2, y);
    ctx.lineTo(x + this.lineSpacing / 2, y - 3 * this.lineSpacing / 2);
    ctx.stroke();

    // 2. Draw Accidental if needed (Smart Logic)
    const accidental = this.getAccidentalSymbol(note, this.currentKeySig);
    if (accidental) {
      ctx.font = `${this.lineSpacing * 1.8}px serif`;
      ctx.fillText(accidental, x - 18, y + this.lineSpacing / 1.5);
    }

    // 3. Draw Ledger Lines (Always Black)
    ctx.strokeStyle = "black"; 

    const topLineY = clef === "treble" ? this.staffTop : this.staffTop + this.staffGap;
    const bottomLineY = topLineY + (this.numLines - 1) * this.lineSpacing;

    if (y < topLineY) {
      for (let ly = topLineY - this.lineSpacing; ly >= y - 1; ly -= this.lineSpacing) {
        ctx.beginPath();
        ctx.moveTo(x - this.lineSpacing, ly);
        ctx.lineTo(x + this.lineSpacing, ly);
        ctx.stroke();
      }
    }
    
    if (y > bottomLineY) {
      for (let ly = bottomLineY + this.lineSpacing; ly <= y + 1; ly += this.lineSpacing) {
        ctx.beginPath();
        ctx.moveTo(x - this.lineSpacing, ly);
        ctx.lineTo(x + this.lineSpacing, ly);
        ctx.stroke();
      }
    }
  }

  drawFigureAboveNote(figure, x, note, clef) {
    const y = this.getNoteY(note, clef) - this.lineSpacing * 2.5;
    this.ctx.font = `bold ${this.lineSpacing * 1.2}px serif`;
    this.ctx.fillStyle = "black";
    this.ctx.fillText(figure, x, y);
  }

  renderChallenge(challenge) {
    this.currentKeySig = challenge.keySig; // Update renderer state

    const x = 150; // moved over to make room for key sig
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    this.drawStaff(this.staffTop);
    this.drawStaff(this.staffTop + this.staffGap);

    this.drawClef("treble");
    this.drawClef("bass");
    
    this.drawKeySignature(challenge.keySig, 60);

    // Bass note is always black
    this.drawNote(challenge.bass, x, "bass", "black");
    this.drawFigureAboveNote(challenge.figure, x, challenge.bass, "bass");
  }

  renderFeedback(challenge, userNote, correct, correctNotes) {
    this.renderChallenge(challenge);

    const userX = 240;
    const octave = parseInt(userNote.match(/-?\d+/)[0]);
    const userClef = octave >= 4 ? 'treble' : 'bass';

    this.ctx.save();

    const userColor = correct ? '#22c55e' : '#ef4444';
    this.drawNote(userNote, userX, userClef, userColor);

    if (!correct && correctNotes) {
      correctNotes.forEach((note, i) => {
        const nOctave = parseInt(note.match(/-?\d+/)[0]);
        const nClef = nOctave >= 4 ? 'treble' : 'bass';
        const xOffset = userX + 40 + (i * 40);
        this.drawNote(note, xOffset, nClef, '#3b82f6');
      });
    }

    this.ctx.restore();
  }
}

// ---------------------- ChallengeGenerator ----------------------
class ChallengeGenerator {
  constructor() {
    this.figures = ["", "6", "6/4", "7", "6/5"];
    this.currentKeySig = 0;
  }

  setKeySignature(num) {
    this.currentKeySig = parseInt(num);
  }

  // Diatonic Intervals (steps above bass in the scale)
  // 0 = unison, 2 = third, 4 = fifth, etc.
  getIntervalsForFigure(figure) {
    switch(figure) {
      case "": return [0, 2, 4];      // 1 3 5
      case "6": return [0, 2, 5];     // 1 3 6
      case "6/4": return [0, 3, 5];   // 1 4 6
      case "7": return [0, 2, 4, 6];  // 1 3 5 7
      case "6/5": return [0, 2, 4, 5]; // 1 3 5 6
      default: return [0, 2, 4];
    }
  }

  nextChallenge() {
    const scale = KEY_SCALES[this.currentKeySig.toString()];
    
    // Pick a random degree of the scale (0 to 6)
    const degree = Math.floor(Math.random() * 7);
    const rootName = scale[degree];
    
    // Decide Octave for bass (Keep between C2 and G3)
    // We just pick reasonable octaves.
    // Let's construct a pool of diatonic bass notes from C2 to G3
    const diatonicBassPool = [];
    
    // Range C2 (midi 36) to G3 (midi 55)
    for (let octave = 2; octave <= 3; octave++) {
      scale.forEach(note => {
         const fullNote = note + octave;
         const midi = this.noteToMidi(fullNote);
         if (midi >= 36 && midi <= 55) {
           diatonicBassPool.push(fullNote);
         }
      });
    }
    
    const bass = diatonicBassPool[Math.floor(Math.random() * diatonicBassPool.length)];
    const figure = this.figures[Math.floor(Math.random() * this.figures.length)];
    
    const correctNotes = this.getCorrectNotes(bass, figure, scale);
    
    return { bass, figure, chordNotes: correctNotes, keySig: this.currentKeySig };
  }

  getCorrectNotes(bassNote, figure, scale) {
    // 1. Find where the bass note is in the scale (index)
    const bassLetter = bassNote.replace(/[\d-]+/, '');
    // Note: Scale might be ['F#', 'G#'] and bass is 'F#'. 
    // Or scale has 'F' and bass is 'F'
    // We must align scale to start at the bass note to calculate intervals easily?
    // Actually, just mapping scale indices is easier.
    
    // Find index of bass note class in the scale array
    let scaleIndex = scale.indexOf(bassLetter);
    
    // If bass note isn't in scale (shouldn't happen with current logic, but safety):
    if (scaleIndex === -1) scaleIndex = 0;

    const intervals = this.getIntervalsForFigure(figure); // e.g., [0, 2, 4]
    const correctNotes = [];
    
    const bassMidi = this.noteToMidi(bassNote);

    intervals.forEach(intervalStep => {
      // Calculate target scale degree
      const targetIndex = (scaleIndex + intervalStep) % 7;
      const targetLetter = scale[targetIndex];
      
      // We need to find the specific pitch (Octave)
      // Minimal movement: look for this note in the range C4-C6
      // Brute force: generate candidate octaves and pick the one in range
      let bestNote = targetLetter + "4";
      
      // Create valid candidates in treble range
      for(let oct = 3; oct <= 6; oct++) {
        const candidate = targetLetter + oct;
        const candMidi = this.noteToMidi(candidate);
        if (candMidi >= 60 && candMidi <= 84) { // C4 to C6
           // We want specifically the one that corresponds to the chord voicing. 
           // But for this trainer, "any valid note in the chord" is usually acceptable
           // or specific voicing? The prompt says "correct notes" (plural). 
           // The simple checker just checks if the USER note is ONE of these.
           correctNotes.push(candidate);
        }
      }
    });
    
    // Remove duplicates if any
    return [...new Set(correctNotes)];
  }

  noteToMidi(note) {
    const noteMap = { 'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11 };
    const letter = note.replace(/[\d#b-]+/, '');
    const acc = note.includes('#') ? 1 : (note.includes('b') ? -1 : 0);
    const octave = parseInt(note.match(/-?\d+/)[0]);
    
    return (octave + 1) * 12 + noteMap[letter] + acc;
  }

  checkNote(challenge, userNote) {
    // Compare Pitch Classes (ignoring octave)
    // But must allow for enharmonics? For now, exact string match on pitch class.
    // User input from keyboard is always sharp (e.g., C#, D#).
    // Scale might use flats (Db, Eb).
    // We need to normalize to Midi Pitch Class (0-11)
    
    const userMidiClass = this.noteToMidi(userNote) % 12;
    const correctMidiClasses = challenge.chordNotes.map(n => this.noteToMidi(n) % 12);
    
    return correctMidiClasses.includes(userMidiClass);
  }
}

// ---------------------- SessionManager ----------------------
class SessionManager {
  constructor() {
    this.score = 0;
    this.streak = 0;
  }

  logAttempt(correct) {
    if (correct) {
      this.streak++;
      let points = 1;
      if (this.streak >= 3) points++;
      this.score += points;
    } else this.streak = 0;
    this.updateScoreboard();
  }

  updateScoreboard() {
    const div = document.getElementById("scoreboard");
    if (div) div.textContent = `Score: ${this.score} | Streak: ${this.streak}`;
  }
}

// ---------------------- InputHandler ----------------------
class InputHandler {
  constructor(renderer, generator, session) {
    this.renderer = renderer;
    this.generator = generator;
    this.session = session;
    this.currentChallenge = null;
    
    // Hook up Key Select
    const keySelect = document.getElementById("keySig");
    keySelect.addEventListener("change", (e) => {
       this.generator.setKeySignature(e.target.value);
       this.startNext();
    });
  }

  startNext() {
    this.currentChallenge = this.generator.nextChallenge();
    this.renderer.renderChallenge(this.currentChallenge);
  }

  handleInput(note) {
    if (!this.currentChallenge) return;
    const correct = this.generator.checkNote(this.currentChallenge, note);
    this.session.logAttempt(correct);

    this.renderer.renderFeedback(
      this.currentChallenge,
      note,
      correct,
      this.currentChallenge.chordNotes
    );

    setTimeout(() => this.startNext(), 1500);
  }
}

// ---------------------- Keyboard ----------------------
class Keyboard {
  static create(containerId, onKeyPress) {
    const container = document.getElementById(containerId);
    if (!container) return;
    container.innerHTML = "";
    container.style.position = "relative";
    container.style.height = "120px";

    const whiteKeyWidth = 40;
    const whiteKeyHeight = 120;
    const blackKeyWidth = 25;
    const blackKeyHeight = 80;

    const whiteNotes = [
      'C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4',
      'C5', 'D5', 'E5', 'F5', 'G5', 'A5', 'B5',
      'C6'
    ];

    whiteNotes.forEach((note, i) => {
      const key = document.createElement("div");
      key.className = "white-key";
      key.style.left = `${i * whiteKeyWidth}px`;
      key.style.width = `${whiteKeyWidth}px`;
      key.style.height = `${whiteKeyHeight}px`;
      key.style.display = "flex";
      key.style.alignItems = "flex-end";
      key.style.justifyContent = "center";
      key.style.fontWeight = "bold";
      key.style.cursor = "pointer";
      key.style.paddingBottom = "8px";
      key.style.fontSize = "10px";
      key.textContent = note;
      key.onclick = () => onKeyPress(note);
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
      key.style.cursor = "pointer";
      key.onclick = () => onKeyPress(note);
      container.appendChild(key);
    });

    container.style.width = `${whiteNotes.length * whiteKeyWidth}px`;
  }
}

// ---------------------- Init ----------------------
window.addEventListener("DOMContentLoaded", () => {
  const renderer = new CanvasRenderer(document.getElementById("sheet"));
  const generator = new ChallengeGenerator();
  const session = new SessionManager();
  const inputHandler = new InputHandler(renderer, generator, session);

  Keyboard.create("keyboard", n => inputHandler.handleInput(n));

  // Clean up potential duplicate debugs
  const existingDebug = document.getElementById("debug");
  if (existingDebug) existingDebug.remove();

  inputHandler.startNext();
});
