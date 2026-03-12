#!/usr/bin/env python3
"""Create DOS-ending copies of all test input files in tst/.

For each test progname_N_*.txt, creates progname_(N+max)_*.txt where
max is the highest existing test number for that program.  Output files
(_out.txt, _arg_out.txt) are copied unchanged (Unix endings).  All
other files (_in.txt, _arg_in.txt, _arg.txt) are converted to CRLF.
"""

import os
import re
import shutil

TST_DIR = "tst"

# Collect all test files
pattern = re.compile(r'^([a-z0-9_]+?)_(\d+)_(.+)$')

by_prog = {}  # prog -> {n -> [filename, ...]}
for name in os.listdir(TST_DIR):
    m = pattern.match(name)
    if m:
        prog, n, rest = m.group(1), int(m.group(2)), m.group(3)
        by_prog.setdefault(prog, {}).setdefault(n, []).append(name)

for prog, tests in sorted(by_prog.items()):
    max_n = max(tests.keys())
    for n, files in sorted(tests.items()):
        for name in files:
            m = pattern.match(name)
            rest = m.group(3)
            new_n = n + max_n
            new_name = f"{prog}_{new_n}_{rest}"
            src = os.path.join(TST_DIR, name)
            dst = os.path.join(TST_DIR, new_name)
            is_output = rest.endswith("_out.txt") or rest == "out.txt"
            with open(src, "rb") as f:
                data = f.read()
            if not is_output:
                data = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
                data = data.replace(b"\n", b"\r\n")
            with open(dst, "wb") as f:
                f.write(data)
            print(f"  {name} -> {new_name}" + ("" if is_output else " [CRLF]"))
