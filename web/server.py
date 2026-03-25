#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# server.py --- minimal continuo trainer web backend
# Copyright (c) 2026 Jakob Kastelic
#
# DESCRIPTION
#     Serves the web/ directory as static files and provides two JSON
#     endpoints so the browser can persist stats across sessions:
#
#         GET  /api/stats      Return stored stats (JSON object).
#         PUT  /api/stats      Replace stored stats with request body.
#         GET  /api/username   Return { "username": "<name>" }; auto-
#                              generates a short name on first run and
#                              writes it to username.txt for re-use.
#
#     All other paths are served from the same directory as this script.
#     Stats are written to stats.json next to this script whenever the
#     browser calls PUT /api/stats.
#
# USAGE
#     python3 web/server.py [port]   # default port: 8080

import json
import os
import sys
import uuid
from http.server import HTTPServer, SimpleHTTPRequestHandler

_HERE         = os.path.dirname(os.path.abspath(__file__))
_ROOT         = os.path.join(_HERE, "..")
_LOG_DIR      = os.path.join(_ROOT, "log")
STATS_FILE    = os.path.join(_LOG_DIR, "stats.json")
USERNAME_FILE = os.path.join(_LOG_DIR, "username.txt")
WEB_DIR       = _HERE


def _load_stats():
    try:
        with open(STATS_FILE, encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _save_stats(data):
    with open(STATS_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def _get_username():
    if os.path.exists(USERNAME_FILE):
        return open(USERNAME_FILE, encoding="utf-8").read().strip()
    name = "user-" + uuid.uuid4().hex[:6]
    with open(USERNAME_FILE, "w", encoding="utf-8") as f:
        f.write(name)
    return name


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def do_GET(self):
        if self.path == "/api/stats":
            self._send_json(_load_stats())
        elif self.path == "/api/username":
            self._send_json({"username": _get_username()})
        elif self.path.startswith("/chn/"):
            self._serve_file(os.path.join(_ROOT, "chn", self.path[5:]))
        else:
            super().do_GET()

    def _serve_file(self, path):
        path = os.path.normpath(path)
        if not os.path.isfile(path):
            self.send_error(404)
            return
        ext = os.path.splitext(path)[1]
        ctype = {".svg": "image/svg+xml", ".txt": "text/plain",
                 ".png": "image/png"}.get(ext, "application/octet-stream")
        with open(path, "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_PUT(self):
        if self.path == "/api/stats":
            n = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(n)
            try:
                _save_stats(json.loads(body))
                self._send_json({"ok": True})
            except json.JSONDecodeError:
                self.send_error(400, "Bad JSON")
        else:
            self.send_error(404)

    def _send_json(self, data):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        pass  # suppress per-request logging


if __name__ == "__main__":
    os.makedirs(_LOG_DIR, exist_ok=True)
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    server = HTTPServer(("", port), Handler)
    print(f"http://localhost:{port}", flush=True)
    server.serve_forever()
