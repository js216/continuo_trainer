#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# web/server.py --- HTTP server for the continuo trainer web UI
# Copyright (c) 2026 Jakob Kastelic
#
# Run from project root: python3 web/server.py
#
# All on port 8080:
#   GET /             → web/index.html
#   GET /<file>       → web/<file>
#   GET /chn/<h>.png  → chn/<h>.png
#   GET /chn/<h>.txt  → chn/<h>.txt
#   GET /events       → SSE stream from all.lua + stats.lua
#   POST /cmd         → line(s) forwarded to all.lua + stats.lua stdin

import os
import queue
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # project root
WEB  = Path(__file__).resolve().parent         # web/

# One Queue per active SSE client
_clients: set = set()
_clients_lock = threading.Lock()

all_proc   = None
stats_proc = None

# ── pipeline ───────────────────────────────────────────────────────────────────

def start_pipeline():
    global all_proc, stats_proc
    all_proc = subprocess.Popen(
        ["lua", "src/all.lua"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=sys.stderr, cwd=ROOT,
    )
    stats_proc = subprocess.Popen(
        ["lua", "src/stats.lua", "log/stats.log"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=sys.stderr, cwd=ROOT,
    )
    threading.Thread(target=_reader, args=(all_proc,   stats_proc), daemon=True).start()
    threading.Thread(target=_reader, args=(stats_proc, all_proc),   daemon=True).start()


def _reader(src, peer):
    """Read src stdout, tee to peer stdin and all SSE clients."""
    for raw in src.stdout:
        try:
            peer.stdin.write(raw)
            peer.stdin.flush()
        except Exception:
            pass
        _broadcast(raw.decode().rstrip("\n"))


def _broadcast(line: str):
    with _clients_lock:
        dead = set()
        for q in _clients:
            try:
                q.put_nowait(line)
            except queue.Full:
                dead.add(q)
        _clients.difference_update(dead)


def _send(line: str):
    raw = (line.strip() + "\n").encode()
    for proc in (all_proc, stats_proc):
        try:
            proc.stdin.write(raw)
            proc.stdin.flush()
        except Exception:
            pass

# ── HTTP handler ───────────────────────────────────────────────────────────────

MIME = {
    ".html": "text/html; charset=utf-8",
    ".js":   "application/javascript; charset=utf-8",
    ".css":  "text/css; charset=utf-8",
    ".png":  "image/png",
    ".txt":  "text/plain; charset=utf-8",
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args): pass

    def do_GET(self):
        path = self.path.split("?")[0]

        if path == "/events":
            self._sse()
            return

        if path in ("/", "/index.html"):
            fpath = WEB / "index.html"
        elif path.startswith("/chn/"):
            fpath = ROOT / path.lstrip("/")
        else:
            fpath = WEB / path.lstrip("/")

        if not fpath.is_file():
            self.send_error(404)
            return

        ct   = MIME.get(fpath.suffix.lower(), "application/octet-stream")
        body = fpath.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", ct)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if self.path != "/cmd":
            self.send_error(404)
            return
        n    = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(n).decode()
        for line in body.splitlines():
            _send(line)
        self.send_response(204)
        self.end_headers()

    def _sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

        q = queue.Queue(maxsize=256)
        with _clients_lock:
            _clients.add(q)
        try:
            while True:
                try:
                    line = q.get(timeout=15)
                    self.wfile.write(f"data: {line}\n\n".encode())
                    self.wfile.flush()
                except queue.Empty:
                    # keepalive so the connection stays open
                    self.wfile.write(b": keepalive\n\n")
                    self.wfile.flush()
        except Exception:
            pass
        finally:
            with _clients_lock:
                _clients.discard(q)

# ── main ───────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    os.makedirs(ROOT / "log", exist_ok=True)
    start_pipeline()
    srv = ThreadingHTTPServer(("localhost", 8080), Handler)
    print("Open  http://localhost:8080/")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
