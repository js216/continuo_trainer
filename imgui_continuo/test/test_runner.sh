#!/usr/bin/env bash
set -euo pipefail

# Usage: test_runner.sh program1 program2 ...

if [ "$#" -eq 0 ]; then
   echo "Usage: $0 <program> [program ...]"
   exit 1
fi

for prog in "$@"; do
   # Golden file is the program path + ".txt"
   golden_file="${prog}.txt"

    # Check if golden file exists
    if [ ! -f "$golden_file" ]; then
       echo "Error: golden file '$golden_file' does not exist for '$prog'"
       exit 1
    fi

    # Compare with golden file
    if ! diff -u <("$prog") "$golden_file"; then
       echo "Test failed: $prog output differs"
       exit 1
    fi
done
