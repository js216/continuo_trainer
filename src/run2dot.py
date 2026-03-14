#!/usr/bin/env python3

import sys


def dot_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main() -> None:
    print("digraph G {")
    print("    rankdir=LR;")

    seen = set()

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        if line.endswith(";"):
            line = line[:-1]

        parts = [p.strip() for p in line.split("->")]

        # Declare nodes
        for p in parts:
            if p not in seen:
                seen.add(p)
                esc = dot_escape(p)
                print(f'    "{esc}" [label="{esc}"];')

        # Emit edges
        for a, b in zip(parts, parts[1:]):
            a_esc = dot_escape(a)
            b_esc = dot_escape(b)
            print(f'    "{a_esc}" -> "{b_esc}";')

    print("}")


if __name__ == "__main__":
    main()
