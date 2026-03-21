#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# lua_to_json --- convert continuo_trainer stats.log (Lua) to JSON
# Copyright (c) 2026 Jakob Kastelic
#
# Usage: lua_to_json <stats.log> [output.json]
#   If output.json is omitted, writes to stdout.

import re
import json
import sys


class LuaParser:
    def __init__(self, text):
        self.text = text
        self.pos = 0

    def _skip_ws(self):
        while self.pos < len(self.text) and self.text[self.pos] in ' \t\n\r':
            self.pos += 1

    def _peek(self):
        self._skip_ws()
        return self.text[self.pos] if self.pos < len(self.text) else None

    def parse(self):
        self._skip_ws()
        if self.text[self.pos:self.pos + 7] == 'return ':
            self.pos += 7
        return self._parse_value()

    def _parse_value(self):
        c = self._peek()
        if c is None:
            raise ValueError(f"Unexpected end of input at pos {self.pos}")
        if c == '{':
            return self._parse_table()
        if c == '"':
            return self._parse_string()
        if c in '0123456789' or c == '-':
            return self._parse_number()
        if self.text[self.pos:self.pos + 4] == 'true':
            self.pos += 4
            return True
        if self.text[self.pos:self.pos + 5] == 'false':
            self.pos += 5
            return False
        if self.text[self.pos:self.pos + 3] == 'nil':
            self.pos += 3
            return None
        raise ValueError(f"Unexpected character {c!r} at pos {self.pos}")

    def _parse_string(self):
        self._skip_ws()
        assert self.text[self.pos] == '"', f"Expected '\"' at pos {self.pos}"
        self.pos += 1
        parts = []
        while self.pos < len(self.text):
            c = self.text[self.pos]
            if c == '\\':
                self.pos += 1
                nc = self.text[self.pos]
                parts.append({'n': '\n', 't': '\t', 'r': '\r',
                               '"': '"', '\\': '\\'}.get(nc, nc))
                self.pos += 1
            elif c == '"':
                self.pos += 1
                return ''.join(parts)
            else:
                parts.append(c)
                self.pos += 1
        raise ValueError("Unterminated string literal")

    def _parse_number(self):
        self._skip_ws()
        m = re.match(r'-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?',
                     self.text[self.pos:])
        if not m:
            raise ValueError(f"Expected number at pos {self.pos}")
        s = m.group(0)
        self.pos += len(s)
        return float(s) if ('.' in s or 'e' in s.lower()) else int(s)

    def _parse_table(self):
        self._skip_ws()
        assert self.text[self.pos] == '{', f"Expected '{{' at pos {self.pos}"
        self.pos += 1

        str_entries = {}   # string-keyed entries
        int_entries = {}   # integer-keyed entries (Lua arrays)

        while True:
            self._skip_ws()
            if self.pos >= len(self.text):
                raise ValueError("Unterminated table")
            c = self.text[self.pos]

            if c == '}':
                self.pos += 1
                break
            if c == ',':
                self.pos += 1
                continue
            if c == '[':
                # [key] = value
                self.pos += 1
                key = self._parse_value()
                self._skip_ws()
                assert self.text[self.pos] == ']', \
                    f"Expected ']' at pos {self.pos}"
                self.pos += 1
                self._skip_ws()
                assert self.text[self.pos] == '=', \
                    f"Expected '=' at pos {self.pos}"
                self.pos += 1
                value = self._parse_value()
                if isinstance(key, int):
                    int_entries[key] = value
                else:
                    str_entries[str(key)] = value
            else:
                # bare identifier key
                m = re.match(r'[a-zA-Z_][a-zA-Z0-9_]*', self.text[self.pos:])
                if m:
                    key = m.group(0)
                    self.pos += len(key)
                    self._skip_ws()
                    assert self.text[self.pos] == '=', \
                        f"Expected '=' after key {key!r} at pos {self.pos}"
                    self.pos += 1
                    value = self._parse_value()
                    str_entries[key] = value
                else:
                    raise ValueError(
                        f"Unexpected character {c!r} at pos {self.pos}")

        # Pure integer-keyed table → JSON array
        if int_entries and not str_entries:
            return [int_entries[k] for k in sorted(int_entries)]

        # Mixed or string-only → JSON object
        # (integer keys get merged in as string keys — unusual but safe)
        for k, v in int_entries.items():
            str_entries[str(k)] = v
        return str_entries


def main():
    if len(sys.argv) < 2:
        print("Usage: lua_to_json <stats.log> [output.json]", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], encoding='utf-8') as f:
        text = f.read()

    data = LuaParser(text).parse()
    output = json.dumps(data, indent=2)

    if len(sys.argv) >= 3:
        with open(sys.argv[2], 'w', encoding='utf-8') as f:
            f.write(output)
        print(f"Written to {sys.argv[2]}", file=sys.stderr)
    else:
        print(output)


if __name__ == '__main__':
    main()
