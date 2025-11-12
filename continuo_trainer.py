#!/usr/bin/env python3
import json
import os
import random
import time
import mido
from music21 import note, chord, roman, pitch
import pygame

# --- SETTINGS ---------------------------------------------------------------

SETTINGS_FILE = "continuo_settings.json"

KEY = "C"        # global key
FIGURES = ["I", "V", "IV", "ii", "vi"]  # basic triads
MIDI_TIMEOUT = 10.0   # seconds to collect chord notes

NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F',
              'F#', 'G', 'G#', 'A', 'A#', 'B']

STAFF_HEIGHT = 100
NOTE_RADIUS = 8
WINDOW_WIDTH = 200
WINDOW_HEIGHT = 150
STAFF_Y = 70
NOTE_COLOR = (255, 0, 0)

NOTE_POSITIONS = {
    "C2": STAFF_Y + 20, "D2": STAFF_Y + 15, "E2": STAFF_Y + 10, "F2": STAFF_Y + 5,
    "G2": STAFF_Y, "A2": STAFF_Y - 5, "B2": STAFF_Y - 10,
    "C3": STAFF_Y - 15, "D3": STAFF_Y - 20, "E3": STAFF_Y - 25, "F3": STAFF_Y - 30,
    "G3": STAFF_Y - 35, "A3": STAFF_Y - 40, "B3": STAFF_Y - 45,
    "C4": STAFF_Y - 50, "D4": STAFF_Y - 55, "E4": STAFF_Y - 60,
    "F4": STAFF_Y - 65, "G4": STAFF_Y - 70, "A4": STAFF_Y - 75, "B4": STAFF_Y - 80
}

# Define the vertical position of the middle line of the bass staff (G2 line)
STAFF_MIDDLE_Y = 70  # adjust to your screen
LINES_SPACING = 10   # pixels between staff lines

# MIDI number of the reference note (e.g., G2 on the second line of bass staff)
REF_MIDI = 43  # G2

def note_number_to_name(note_number):
    octave = (note_number // 12) - 1
    name = NOTE_NAMES[note_number % 12]
    return f"{name}{octave}"

def load_settings():
    """Load settings from JSON file, return a dict."""
    if os.path.exists(SETTINGS_FILE):
        with open(SETTINGS_FILE, "r") as f:
            return json.load(f)
    return {}

def save_settings(settings):
    """Save settings dict to JSON file."""
    with open(SETTINGS_FILE, "w") as f:
        json.dump(settings, f, indent=2)

# --- MIDI SELECTION ---------------------------------------------------------

def choose_input_port():
    """List available MIDI inputs, remember selection in JSON, and return opened port."""
    settings = load_settings()
    inputs = mido.get_input_names()
    if not inputs:
        print("No MIDI input devices found.")
        exit(1)

    # If previously used port exists, use it
    if "midi_port" in settings and settings["midi_port"] in inputs:
        port_name = settings["midi_port"]
        print(f"Using previously selected MIDI input: {port_name}\n")
        return mido.open_input(port_name)

    # Otherwise, ask user
    print("Available MIDI inputs:")
    for i, name in enumerate(inputs):
        print(f"  [{i}] {name}")

    if len(inputs) == 1:
        choice = 0
    else:
        while True:
            try:
                choice = int(input("Select input number: "))
                if 0 <= choice < len(inputs):
                    break
                else:
                    print("Invalid choice.")
            except ValueError:
                print("Enter a valid number.")

    port_name = inputs[choice]
    print(f"\nOpening: {port_name}\n")

    # Save to JSON settings
    settings["midi_port"] = port_name
    save_settings(settings)

    return mido.open_input(port_name)

# --- HELPER FUNCTIONS -------------------------------------------------------

def get_random_exercise():
    """Return a bass note and expected RomanNumeral chord."""
    fig = random.choice(FIGURES)
    rn = roman.RomanNumeral(fig, KEY)
    bass_pitch = rn.bass().nameWithOctave
    return bass_pitch, rn

def get_chord_from_midi(inport, timeout=MIDI_TIMEOUT):
    """Collect notes played within a short window."""
    played = set()
    start = time.time()
    while time.time() - start < timeout:
        for msg in inport.iter_pending():
            if msg.type == "note_on" and msg.velocity > 0:
                played.add(msg.note)
        time.sleep(0.01)
    return chord.Chord([note_number_to_name(n) for n in played]) if played else None

# --- GUI --------------------------------------------------------------

def get_note_y(midi_note_number):
    """
    Return y-coordinate for a note on the staff.
    Middle reference note is REF_MIDI at STAFF_MIDDLE_Y.
    Each staff line or space is LINES_SPACING / 2 pixels.
    """
    semitone_diff = midi_note_number - REF_MIDI
    # On a staff, one line/space = 1 semitone
    y = STAFF_MIDDLE_Y - semitone_diff * (LINES_SPACING / 2)
    return int(y)

def init_staff_window():
    pygame.init()
    screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
    pygame.display.set_caption("Continuo Trainer")
    screen.fill((255, 255, 255))
    # Draw staff lines
    for i in range(5):
        y = STAFF_Y - i*10
        pygame.draw.line(screen, (0,0,0), (20, y), (WINDOW_WIDTH-20, y), 2)
    pygame.display.flip()
    return screen

def display_bass(screen, bass_note_str):
    """Draw the bass note on the staff dynamically."""
    screen.fill((255,255,255))
    # Redraw staff lines
    for i in range(5):
        y = STAFF_Y - i*LINES_SPACING
        pygame.draw.line(screen, (0,0,0), (20, y), (WINDOW_WIDTH-20, y), 2)

    # Convert note string to MIDI number
    p = pitch.Pitch(bass_note_str)
    y = get_note_y(p.midi)

    # Draw the note
    pygame.draw.circle(screen, NOTE_COLOR, (WINDOW_WIDTH//2, y), NOTE_RADIUS)
    pygame.display.flip()

# --- MAIN LOOP --------------------------------------------------------------

def main():
    inport = choose_input_port()
    screen = init_staff_window()  # Initialize the staff display

    print("Welcome to the Continuo Trainer (Stage 1)")
    print("Play a correct realization for the displayed bass note.\n")

    try:
        while True:
            bass, expected = get_random_exercise()
            display_bass(screen, bass)  # Show bass note on staff

            print(f"Exercise: bass note {bass}, expecting {expected.figure} in {KEY} major")
            print(f"Play a chord now (you have {MIDI_TIMEOUT:.1f}s)...")

            played_chord = get_chord_from_midi(inport)
            if not played_chord:
                print("No notes received. Try again.\n")
                continue

            print("You played:", played_chord.pitchNames)

            # Compare roots
            if expected.root().name == played_chord.root().name:
                print("✅ Correct root!")
            else:
                print(f"❌ Expected root {expected.root().name}, got {played_chord.root().name}")

            print("-" * 40)
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting...")
        pygame.quit()


# --- ENTRY POINT ------------------------------------------------------------

if __name__ == "__main__":
    main()

