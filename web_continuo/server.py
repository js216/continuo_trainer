import http.server
import socketserver
import sqlite3
import json
import os
import time
import uuid
import random

PORT = 8000
DB_FILE = "continuo.db"

STANDARD_LESSONS = [
   {
      "id": "std-1",
      "name": "Lesson 1: Root Position Triads",
      "description": "Play the 3rd and 5th above the bass.",
      "defaultKey": 0,
      "timeSignature": [4, 4],
      "anacrusisBeats": 0,
      "tempo": 160,
      "sequence": [
         { "bass": "C3", "figure": "6/3", "duration": 2, "correctAnswer": ["E4", "G4"] },
         { "bass": "G3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "E3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "A3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "F3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "C4", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "A3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "F3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "G3", "figure": "6/3", "duration": 2, "correctAnswer": ["B4", "D5"] },
         { "bass": "A3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "F3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "G3", "figure": "", "duration": 2, "correctAnswer": [] },
         { "bass": "C3", "figure": "", "duration": 1, "correctAnswer": [] }
      ]
   },
   {
      "id": "std-2",
      "name": "Lesson 2: Full Measure Triads",
      "description": "Play the 3rd and 5th above the bass.",
      "defaultKey": 1,
      "timeSignature": [4, 4],
      "anacrusisBeats": 0,
      "tempo": 72,
      "sequence": [
         { "bass": "G2", "figure": "", "duration": 4, "correctAnswer": ["B3", "D4"] },
         { "bass": "G3", "figure": "", "duration": 4, "correctAnswer": ["B3", "D4"] },
         { "bass": "E3", "figure": "", "duration": 4, "correctAnswer": ["G3", "B3"] },
         { "bass": "C3", "figure": "", "duration": 4, "correctAnswer": ["E3", "G3"] },
         { "bass": "D3", "figure": "", "duration": 4, "correctAnswer": ["F#3", "A3"] }
      ]
   },
   {
      "id": "std-3",
      "name": "Lesson 3: Eighth Notes",
      "description": "Play the 3rd and 5th above the bass. (Note the beams)",
      "defaultKey": -1,
      "timeSignature": [4, 4],
      "anacrusisBeats": 0,
      "tempo": 90,
      "sequence": [
         { "bass": "F3", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "G3", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "F3", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "E3", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "D3", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "A3", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "D4", "figure": "", "duration": 8, "correctAnswer": [] },
         { "bass": "C4", "figure": "", "duration": 8, "correctAnswer": ["E4", "G4"] }
      ]
   },
   {
      "id": "std-4",
      "name": "Lesson 4: Half notes",
      "description": "The figure '#' raises the 7th note.",
      "defaultKey": -1,
      "timeSignature": [4, 4],
      "anacrusisBeats": 0,
      "tempo": 60,
      "sequence": [
         { "bass": "C3", "figure": "", "duration": 2, "correctAnswer": ["E3", "G3"] },
         { "bass": "G2", "figure": "7", "duration": 2, "correctAnswer": ["B2", "D3", "F3"] },
         { "bass": "C3", "figure": "", "duration": 1.5, "correctAnswer": ["E3", "G3"] },
         { "bass": "C3", "figure": "", "duration": 1, "correctAnswer": [] }
      ]
   }
]

def init_db():
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()

    # Users table
    c.execute('''CREATE TABLE IF NOT EXISTS users
                 (user_id TEXT PRIMARY KEY, created_at INTEGER)''')

    # Logs table
    c.execute('''CREATE TABLE IF NOT EXISTS logs
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  user_id TEXT,
                  lesson_id TEXT,
                  timestamp INTEGER,
                  duration_ms INTEGER,
                  score INTEGER,
                  event_data TEXT)''')

    # Lessons table
    # content stores the full JSON of the lesson object
    c.execute('''CREATE TABLE IF NOT EXISTS lessons
                 (lesson_id TEXT PRIMARY KEY,
                  user_id TEXT,
                  name TEXT,
                  content TEXT)''')

    # Seed Standard Lessons
    for l in STANDARD_LESSONS:
        c.execute("SELECT lesson_id FROM lessons WHERE lesson_id=?", (l['id'],))
        if not c.fetchone():
            c.execute("INSERT INTO lessons (lesson_id, user_id, name, content) VALUES (?, ?, ?, ?)",
                      (l['id'], 'system', l['name'], json.dumps(l)))

    conn.commit()
    conn.close()

class ContinuoHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/api/lessons':
            self.handle_get_lessons()
        else:
            # Default static file serving
            super().do_GET()

    def do_POST(self):
        if self.path == '/api/log':
            self.handle_log()
        elif self.path == '/api/login':
            self.handle_login()
        elif self.path == '/api/practice':
            self.handle_practice_gen()
        else:
            self.send_error(404, "Endpoint not found")

    # --- HANDLERS ---

    def handle_get_lessons(self):
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        # Fetch system lessons + any specific to user (not fully used in JS yet but good for future)
        c.execute("SELECT content FROM lessons WHERE user_id = 'system' ORDER BY name")
        rows = c.fetchall()
        conn.close()

        lessons = [json.loads(row[0]) for row in rows]
        self._send_json(lessons)

    def handle_login(self):
        data = self._read_json()
        user_id = data.get('userId')
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        c.execute("SELECT user_id FROM users WHERE user_id=?", (user_id,))
        exists = c.fetchone()
        if not exists:
            c.execute("INSERT INTO users (user_id, created_at) VALUES (?, ?)",
                      (user_id, int(time.time())))
            conn.commit()
        conn.close()
        self._send_json({'status': 'ok', 'userId': user_id})

    def handle_log(self):
        data = self._read_json()
        user_id = data.get('userId')
        lesson_id = data.get('lessonId')
        score = data.get('score', 0)
        duration = data.get('totalDuration', 0)
        events = json.dumps(data.get('events', []))

        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        c.execute('''INSERT INTO logs (user_id, lesson_id, timestamp, duration_ms, score, event_data)
                     VALUES (?, ?, ?, ?, ?, ?)''',
                     (user_id, lesson_id, int(time.time()), duration, score, events))
        conn.commit()
        conn.close()
        self._send_json({'status': 'saved'})

    def handle_practice_gen(self):
        data = self._read_json()
        user_id = data.get('userId')

        new_lesson = self._generate_targeted_lesson(user_id)

        # Save this generated lesson to DB so logging works for it later
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        c.execute("INSERT INTO lessons (lesson_id, user_id, name, content) VALUES (?, ?, ?, ?)",
                  (new_lesson['id'], user_id, new_lesson['name'], json.dumps(new_lesson)))
        conn.commit()
        conn.close()

        self._send_json(new_lesson)

    def _generate_targeted_lesson(self, user_id):
        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        # 1. Fetch recent logs for this user to find mistakes
        # Limit to last 50 logs to be relevant
        c.execute('''SELECT lesson_id, event_data FROM logs
                     WHERE user_id=? ORDER BY timestamp DESC LIMIT 50''', (user_id,))
        rows = c.fetchall()

        weak_bass_notes = []

        for row in rows:
            lesson_id = row['lesson_id']
            events = json.loads(row['event_data'])

            # Find the lesson content to map stepIndex -> bass note
            c.execute("SELECT content FROM lessons WHERE lesson_id=?", (lesson_id,))
            l_row = c.fetchone()
            if not l_row: continue

            lesson_content = json.loads(l_row['content'])
            seq = lesson_content.get('sequence', [])

            for e in events:
                if e['type'] == 'submit':
                    # Heuristic: If scoreDelta is low (<= 5), consider it a struggle
                    # (Recall: Perfect = 15, Correct Note = 10, Timing Off = -5)
                    if e.get('scoreDelta', 0) <= 5:
                        step_idx = e.get('stepIndex')
                        if step_idx is not None and step_idx < len(seq):
                             bass = seq[step_idx].get('bass')
                             if bass:
                                 weak_bass_notes.append(bass)

        conn.close()

        # 2. Logic to build the lesson
        if not weak_bass_notes:
            # Fallback: No history or perfect history -> Random Drill
            target_notes = ['C3', 'G3', 'F3', 'D3']
            description = "General Drill (No weakness data found)"
        else:
            # Pick the most common mistake, or random from the top 3
            from collections import Counter
            counts = Counter(weak_bass_notes)
            common = counts.most_common(3)
            target_notes = [n[0] for n in common]
            description = f"Focusing on your trouble spots: {', '.join(target_notes)}"

        # 3. Construct Sequence
        # Pattern: Anchor (C3) -> Weak Note -> Anchor -> Weak Note
        sequence = []
        anchor = "C3"

        # Simple Triad Answer (Root pos 3rd/5th) generator
        def get_answer(note):
            # Very dumb major triad logic for MVP
            # If C3 -> E3, G3. If G3 -> B3, D4.
            # Ideally this requires music theory logic, keeping it hardcoded for a few common notes
            # or returning empty to just practice bass reading.

            # Let's make it an input drill.
            # We will calculate a major 3rd and perfect 5th roughly for MVP display
            # Actually, let's keep it simple: The user just has to play the bass note to advance?
            # No, the engine expects chords.

            # MVP Hack: Standard Major Triads for C, G, F, D, A, E
            offsets = {'C':0, 'D':2, 'E':4, 'F':5, 'G':7, 'A':9, 'B':11}
            letter = note[0]
            octave = int(note[-1])
            base_val = offsets.get(letter, 0)

            # 3rd is +4 semitones, 5th is +7 semitones
            # This is complex to generate strings like "F#3" without a library.
            # For MVP Targeted Practice, we will make them "Bass only" drills (no input required except advancing?)
            # OR better: Reuse the logic from Lesson 1 (Root Position) implies 3rd and 5th.

            # Let's stick to generating valid drill steps, but empty correctAnswer implies just listening/playing along?
            # No, `continuo.js` requires correctAnswer to be present for input.

            # Map for common keys in C Major
            answers = {
                "C3": ["E3", "G3"], "C4": ["E4", "G4"],
                "G3": ["B3", "D4"], "G2": ["B2", "D3"],
                "F3": ["A3", "C4"], "F2": ["A2", "C3"],
                "D3": ["F#3", "A3"],
                "A3": ["C#4", "E4"],
                "E3": ["G#3", "B3"]
            }
            return answers.get(note, []) # Return empty if not in map (just a passive note)

        # Build 8 measures
        for _ in range(4):
            target = random.choice(target_notes)

            # Measure 1: Anchor
            sequence.append({
                "bass": anchor, "figure": "", "duration": 2,
                "correctAnswer": get_answer(anchor)
            })
            # Measure 2: Target
            sequence.append({
                "bass": target, "figure": "Target", "duration": 2,
                "correctAnswer": get_answer(target)
            })

        # Final note
        sequence.append({"bass": "C3", "figure": "", "duration": 1, "correctAnswer": []})

        lesson_id = f"gen-{uuid.uuid4().hex[:8]}"

        return {
            "id": lesson_id,
            "name": "Targeted Practice",
            "description": description,
            "defaultKey": 0,
            "timeSignature": [4, 4],
            "anacrusisBeats": 0,
            "tempo": 100,
            "sequence": sequence
        }

    def _read_json(self):
        length = int(self.headers.get('content-length', 0))
        if length == 0: return {}
        return json.loads(self.rfile.read(length))

    def _send_json(self, data):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

if __name__ == "__main__":
    init_db()
    print(f"Server started at http://localhost:{PORT}")
    with socketserver.TCPServer(("", PORT), ContinuoHandler) as httpd:
        httpd.serve_forever()
