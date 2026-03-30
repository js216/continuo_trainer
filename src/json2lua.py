#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# json2lua --- convert JSON back to continuo_trainer stats.log (Lua) format
# Copyright (c) 2026 Jakob Kastelic
#
# Usage: json2lua <input.json> [output.lua]
#   If output.lua is omitted, writes to stdout.
#
# Produces the same Lua table format as save_stats() in stats.lua:
#   return {
#     ["key"] = value,
#     ...
#   }
# Keys are always written in ["key"] bracket form (strings quoted, integers bare).
# Tables are recursively serialized; lists become tables with integer keys.

import json
import sys


def _lua_escape(s: str) -> str:
    """Escape a string for Lua double-quoted literals."""
    return (s
            .replace('\\', '\\\\')
            .replace('"', '\\"')
            .replace('\n', '\\n')
            .replace('\r', '\\r')
            .replace('\t', '\\t'))


def _serialize(value, indent: str, depth: int) -> str:
    next_indent = indent + "  "

    if isinstance(value, bool):
        return "true" if value else "false"

    if value is None:
        return "nil"

    if isinstance(value, int):
        return str(value)

    if isinstance(value, float):
        # Always preserve decimal point so the value round-trips as float
        if value == int(value) and abs(value) < 1e15:
            return str(int(value)) + ".0"
        return repr(value)

    if isinstance(value, str):
        return '"' + _lua_escape(value) + '"'

    if isinstance(value, list):
        # JSON array → Lua table with integer keys [1], [2], ...
        s = "{\n"
        for i, item in enumerate(value, start=1):
            s += next_indent + f"[{i}] = " + _serialize(item, next_indent, depth + 1) + ",\n"
        return s + indent + "}"

    if isinstance(value, dict):
        s = "{\n"
        # Sort keys the same way stats.lua does: tostring(a) < tostring(b)
        # For mixed int/string keys from JSON all keys will be strings.
        keys = sorted(value.keys(), key=str)
        for k in keys:
            v = value[k]
            # Determine key representation
            try:
                ik = int(k)
                key_repr = str(ik)
            except (ValueError, TypeError):
                key_repr = '["' + _lua_escape(str(k)) + '"]'

            # Inline compact form for daily-score entries (depth==1, score+duration, no ease)
            if (depth == 1
                    and isinstance(v, dict)
                    and 'score' in v
                    and 'duration' in v
                    and 'ease' not in v):
                score = v.get('score', 0)
                duration = v.get('duration', 0)
                score_s = (str(int(score)) + ".0") if isinstance(score, float) and score == int(score) else repr(score) if isinstance(score, float) else str(score)
                dur_s = (str(int(duration)) + ".0") if isinstance(duration, float) and duration == int(duration) else repr(duration) if isinstance(duration, float) else str(duration)
                s += next_indent + key_repr + ' = {["score"] = ' + score_s + ', ["duration"] = ' + dur_s + '},\n'
            else:
                s += next_indent + key_repr + " = " + _serialize(v, next_indent, depth + 1) + ",\n"
        return s + indent + "}"

    raise TypeError(f"Unsupported type: {type(value)}")


def json_to_lua(data) -> str:
    return "return " + _serialize(data, "", 0) + "\n"


def main():
    if len(sys.argv) == 1 or sys.argv[1] == '-':
        data = json.load(sys.stdin)
    else:
        with open(sys.argv[1], encoding='utf-8') as f:
            data = json.load(f)

    output = json_to_lua(data)

    if len(sys.argv) >= 3:
        with open(sys.argv[2], 'w', encoding='utf-8') as f:
            f.write(output)
    else:
        sys.stdout.write(output)


if __name__ == '__main__':
    main()
