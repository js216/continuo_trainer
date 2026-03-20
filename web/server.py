#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# web/server.py --- static file server for the continuo trainer web UI
# Copyright (c) 2026 Jakob Kastelic
#
# Run from project root:  python3 web/server.py
# Then open:              http://localhost:8080/web/
#
# Serves the entire project root as static files so that both
# web/ (HTML/JS) and chn/ (chunk .txt/.png/.json) are reachable.
# Equivalent to:  python3 -m http.server 8080  (from project root)

import os
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # project root


class Handler(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args): pass


if __name__ == "__main__":
    os.chdir(ROOT)
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    srv = ThreadingHTTPServer(("localhost", port), Handler)
    print(f"Open  http://localhost:{port}/web/")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
