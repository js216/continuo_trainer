import http.server
import socketserver
import sqlite3
import json
import time
import uuid
import random
from collections import Counter

PORT = 8000
DB_FILE = "continuo.db"
LESSON_FILE = "lessons.json"

# --- DOMAIN LAYER: MUSIC THEORY & LESSON GENERATION ---

class ContinuoTheoryEngine:
    """
    Handles music logic: parsing performance logs to find weaknesses
    and generating new musical content.
    """
    
    def analyze_weaknesses(self, logs, lesson_cache):
        """
        analyzes user logs to find bass notes associated with low scores.
        
        :param logs: list of sqlite3.Row objects from the logs table
        :param lesson_cache: dict mapping lesson_id -> lesson_json_content
        :return: list of weak bass note strings (e.g. ['C3', 'F3'])
        """
        weak_bass_notes = []

        for row in logs:
            lesson_id = row['lesson_id']
            try:
                events = json.loads(row['event_data'])
            except (json.JSONDecodeError, TypeError):
                continue

            if lesson_id not in lesson_cache:
                continue

            lesson_content = lesson_cache[lesson_id]
            seq = lesson_content.get('sequence', [])

            for e in events:
                if e.get('type') == 'submit':
                    # Heuristic: Score delta <= 5 indicates a struggle
                    # (Perfect = 15, Correct Note = 10, Timing Off = -5)
                    if e.get('scoreDelta', 0) <= 5:
                        step_idx = e.get('stepIndex')
                        if step_idx is not None and step_idx < len(seq):
                            bass = seq[step_idx].get('bass')
                            if bass:
                                weak_bass_notes.append(bass)
        
        return weak_bass_notes

    def generate_targeted_lesson(self, weak_notes):
        """
        Generates a new lesson structure based on identified weak notes.
        """
        if not weak_notes:
            # Fallback: No history or perfect history -> Random Drill
            target_notes = ['C3', 'G3', 'F3', 'D3']
            description = "General Drill (No weakness data found)"
        else:
            # Pick most common mistake, or random from top 3
            counts = Counter(weak_notes)
            common = counts.most_common(3)
            target_notes = [n[0] for n in common]
            description = f"Focusing on your trouble spots: {', '.join(target_notes)}"

        sequence = []
        anchor = "C3"
        
        # Build 8 measures: Anchor -> Target -> Anchor -> Target
        for _ in range(4):
            target = random.choice(target_notes)

            # Measure 1: Anchor
            sequence.append({
                "bass": anchor, 
                "figure": "", 
                "duration": 2,
                "correctAnswer": self._calculate_triad(anchor)
            })
            # Measure 2: Target
            sequence.append({
                "bass": target, 
                "figure": "Target", 
                "duration": 2,
                "correctAnswer": self._calculate_triad(target)
            })

        # Final note
        sequence.append({
            "bass": "C3", 
            "figure": "", 
            "duration": 1, 
            "correctAnswer": []
        })

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

    def _calculate_triad(self, note):
        """
        Returns the major triad (3rd and 5th) for a given bass note.
        Currently hardcoded for common keys for MVP safety.
        """
        answers = {
            "C3": ["E3", "G3"], "C4": ["E4", "G4"],
            "G3": ["B3", "D4"], "G2": ["B2", "D3"],
            "F3": ["A3", "C4"], "F2": ["A2", "C3"],
            "D3": ["F#3", "A3"],
            "A3": ["C#4", "E4"],
            "E3": ["G#3", "B3"]
        }
        return answers.get(note, [])

# --- INFRASTRUCTURE LAYER: WEB HANDLER & DB ---

class ContinuoRequestHandler(http.server.SimpleHTTPRequestHandler):
    
    theory_engine = ContinuoTheoryEngine()

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

    # --- API HANDLERS ---

    def handle_get_lessons(self):
        with sqlite3.connect(DB_FILE) as conn:
            c = conn.cursor()
            c.execute("SELECT content FROM lessons WHERE user_id = 'system' ORDER BY name")
            rows = c.fetchall()

        lessons = [json.loads(row[0]) for row in rows]
        self._send_json(lessons)

    def handle_login(self):
        data = self._read_json()
        user_id = data.get('userId')
        
        with sqlite3.connect(DB_FILE) as conn:
            c = conn.cursor()
            c.execute("SELECT user_id FROM users WHERE user_id=?", (user_id,))
            exists = c.fetchone()
            if not exists:
                c.execute("INSERT INTO users (user_id, created_at) VALUES (?, ?)",
                          (user_id, int(time.time())))
                conn.commit()
        
        self._send_json({'status': 'ok', 'userId': user_id})

    def handle_log(self):
        data = self._read_json()
        user_id = data.get('userId')
        lesson_id = data.get('lessonId')
        score = data.get('score', 0)
        duration = data.get('totalDuration', 0)
        events = json.dumps(data.get('events', []))

        with sqlite3.connect(DB_FILE) as conn:
            c = conn.cursor()
            c.execute('''INSERT INTO logs (user_id, lesson_id, timestamp, duration_ms, score, event_data)
                         VALUES (?, ?, ?, ?, ?, ?)''',
                         (user_id, lesson_id, int(time.time()), duration, score, events))
            conn.commit()
        
        self._send_json({'status': 'saved'})

    def handle_practice_gen(self):
        data = self._read_json()
        user_id = data.get('userId')

        # 1. Fetch raw data required for analysis
        logs, lesson_cache = self._fetch_user_history(user_id)

        # 2. Use Domain Engine to process logic
        weak_notes = self.theory_engine.analyze_weaknesses(logs, lesson_cache)
        new_lesson = self.theory_engine.generate_targeted_lesson(weak_notes)

        # 3. Persist result
        with sqlite3.connect(DB_FILE) as conn:
            c = conn.cursor()
            c.execute("INSERT INTO lessons (lesson_id, user_id, name, content) VALUES (?, ?, ?, ?)",
                      (new_lesson['id'], user_id, new_lesson['name'], json.dumps(new_lesson)))
            conn.commit()

        self._send_json(new_lesson)

    # --- HELPERS ---

    def _fetch_user_history(self, user_id):
        """
        Retrieves logs and relevant lesson content from DB to pass to the engine.
        """
        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()
        
        # Fetch logs
        c.execute('''SELECT lesson_id, event_data FROM logs
                     WHERE user_id=? ORDER BY timestamp DESC LIMIT 50''', (user_id,))
        logs = c.fetchall()
        
        # Fetch referenced lessons to decode step indices
        lesson_ids = {row['lesson_id'] for row in logs}
        lesson_cache = {}
        
        if lesson_ids:
            placeholders = ','.join('?' * len(lesson_ids))
            c.execute(f"SELECT lesson_id, content FROM lessons WHERE lesson_id IN ({placeholders})", 
                      list(lesson_ids))
            rows = c.fetchall()
            for r in rows:
                lesson_cache[r['lesson_id']] = json.loads(r['content'])
        
        conn.close()
        return logs, lesson_cache

    def _read_json(self):
        length = int(self.headers.get('content-length', 0))
        if length == 0: return {}
        return json.loads(self.rfile.read(length))

    def _send_json(self, data):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

def init_db():
    """Initializes DB tables and loads standard lessons from JSON file."""
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()

    # Tables
    c.execute('CREATE TABLE IF NOT EXISTS users (user_id TEXT PRIMARY KEY, created_at INTEGER)')
    c.execute('''CREATE TABLE IF NOT EXISTS logs
                 (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id TEXT, lesson_id TEXT,
                  timestamp INTEGER, duration_ms INTEGER, score INTEGER, event_data TEXT)''')
    c.execute('''CREATE TABLE IF NOT EXISTS lessons
                 (lesson_id TEXT PRIMARY KEY, user_id TEXT, name TEXT, content TEXT)''')

    # Load Standard Lessons from File
    try:
        with open(LESSON_FILE, 'r') as f:
            standard_lessons = json.load(f)
            
        for l in standard_lessons:
            c.execute("SELECT lesson_id FROM lessons WHERE lesson_id=?", (l['id'],))
            if not c.fetchone():
                print(f"Seeding lesson: {l['name']}")
                c.execute("INSERT INTO lessons (lesson_id, user_id, name, content) VALUES (?, ?, ?, ?)",
                          (l['id'], 'system', l['name'], json.dumps(l)))
    except FileNotFoundError:
        print(f"Warning: {LESSON_FILE} not found. Database initialized without standard lessons.")
    except json.JSONDecodeError:
        print(f"Error: {LESSON_FILE} contains invalid JSON.")

    conn.commit()
    conn.close()

if __name__ == "__main__":
    init_db()
    print(f"Server started at http://localhost:{PORT}")
    # Allow address reuse to prevent 'Address already in use' errors during restarts
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), ContinuoRequestHandler) as httpd:
        httpd.serve_forever()
