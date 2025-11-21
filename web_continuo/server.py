import http.server
import socketserver
import sqlite3
import json
import os
import time

PORT = 8000
DB_FILE = "continuo.db"

def init_db():
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    # Users table (minimal)
    c.execute('''CREATE TABLE IF NOT EXISTS users
                 (user_id TEXT PRIMARY KEY, created_at INTEGER)''')
    
    # Logs table: stores the raw JSON blob of events for a session
    c.execute('''CREATE TABLE IF NOT EXISTS logs
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  user_id TEXT,
                  lesson_id TEXT,
                  timestamp INTEGER,
                  duration_ms INTEGER,
                  score INTEGER,
                  event_data TEXT)''')
    conn.commit()
    conn.close()

class ContinuoHandler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        # Parse endpoint
        if self.path == '/api/log':
            self.handle_log()
        elif self.path == '/api/login':
            self.handle_login()
        else:
            self.send_error(404, "Endpoint not found")

    def handle_login(self):
        data = self._read_json()
        user_id = data.get('userId')
        
        # Simple logic: If ID exists, acknowledge. If not, create.
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        c.execute("SELECT user_id FROM users WHERE user_id=?", (user_id,))
        exists = c.fetchone()
        
        if not exists:
            c.execute("INSERT INTO users (user_id, created_at) VALUES (?, ?)", 
                      (user_id, int(time.time())))
            conn.commit()
            print(f"New user created: {user_id}")
        else:
            print(f"User logged in: {user_id}")
            
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
        
        print(f"Log saved for user {user_id} on lesson {lesson_id}")
        self._send_json({'status': 'saved'})

    def _read_json(self):
        length = int(self.headers.get('content-length', 0))
        if length == 0:
            return {}
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
