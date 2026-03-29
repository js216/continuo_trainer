#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# server.py --- minimal continuo trainer web backend
# Copyright (c) 2026 Jakob Kastelic
#
# DESCRIPTION
#     Serves the web/ directory as static files and provides two JSON
#     endpoints so multiple browser sessions can each persist their own
#     stats:
#
#         GET  /api/stats      Return stored stats for the current user.
#         PUT  /api/stats      Replace stored stats for the current user.
#         GET  /api/username   Return { "username": "<name>" }; auto-
#                              generates a name on first visit and stores
#                              it in a cookie (Max-Age one year).
#
#     Each user's stats are stored in log/<username>.json.  The username
#     is carried in a browser cookie named "username"; no subdirectories
#     are created.  /chn/ requests are served from the project-root chn/
#     directory (SVG score images live there, outside web/).
#
# USAGE
#     python3 web/server.py [port]   # default port: 8080

import json
import os
import re
import sys
import uuid
from http.server import HTTPServer, SimpleHTTPRequestHandler

_HERE    = os.path.dirname(os.path.abspath(__file__))
_ROOT    = os.path.join(_HERE, "..")
_LOG_DIR = os.path.join(_ROOT, "log")
WEB_DIR  = _HERE

_SAFE_NAME = re.compile(r'^[a-zA-Z0-9]+$')


def _cookie_username(headers):
    for part in headers.get("Cookie", "").split(";"):
        name, _, val = part.strip().partition("=")
        if name == "username":
            v = val.strip()
            return v if _SAFE_NAME.match(v) else None
    return None


def _stats_file(username):
    return os.path.join(_LOG_DIR, username + ".json")


def _load_stats(username):
    try:
        with open(_stats_file(username), encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _save_stats(username, data):
    with open(_stats_file(username), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def do_GET(self):
        if self.path == "/api/username":
            username = _cookie_username(self.headers)
            new_cookie = None
            if not username:
                username = "user" + uuid.uuid4().hex[:6]
                new_cookie = f"username={username}; Path=/; Max-Age=31536000"
            self._send_json({"username": username}, cookie=new_cookie)
        elif self.path == "/api/stats":
            username = _cookie_username(self.headers)
            self._send_json(_load_stats(username) if username else {})
        elif self.path.startswith("/chn/"):
            self._serve_file(os.path.join(_ROOT, "chn", self.path[5:]))
        else:
            super().do_GET()

    def do_POST(self):
        if self.path == "/api/username":
            n = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(n)
            try:
                data = json.loads(body)
            except json.JSONDecodeError:
                self.send_error(400, "Bad JSON")
                return
            username = data.get("username", "").strip()
            if not username or not _SAFE_NAME.match(username):
                self.send_error(400, "Invalid username")
                return
            new_cookie = f"username={username}; Path=/; Max-Age=31536000"
            self._send_json({"username": username}, cookie=new_cookie)
        else:
            self.send_error(404)

    def do_PUT(self):
        if self.path == "/api/stats":
            username = _cookie_username(self.headers)
            if not username:
                self.send_error(401, "No username cookie")
                return
            n = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(n)
            try:
                _save_stats(username, json.loads(body))
                self._send_json({"ok": True})
            except json.JSONDecodeError:
                self.send_error(400, "Bad JSON")
        else:
            self.send_error(404)

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

    def _send_json(self, data, cookie=None):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        if cookie:
            self.send_header("Set-Cookie", cookie)
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
