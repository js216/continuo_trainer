#!/bin/sh

# test/run_tests.sh --- run regression tests for unix_continuo
#
# This script discovers and runs program regression tests located in
# test/.  A valid test consists of a pair of files:
#
#     progname_N_in.txt
#     progname_N_out.txt
#
# where:
#   - progname matches an executable in ../bin/
#   - N is a positive integer test number
#
# For each valid test, the script feeds the _in.txt file to
# ../bin/progname via stdin and compares stdout with the corresponding
# _out.txt file using diff.
#
# Tests are executed in deterministic order:
#   1. Alphabetically by program name
#   2. Numerically by test number
#
# If output matches, the script prints:
#     OK progname_N     (in green)
#
# If output differs, it prints:
#     FAIL progname_N   (in red)
#
# Files in test/ that do not begin with the name of an executable in
# ../bin/ are ignored.  If a file begins with a valid program name but
# does not follow the required naming convention, the script exits with
# an error.
#
# The script returns nonzero if any test fails or if a malformed test
# filename is encountered.

set -eu

BIN_DIR="$(dirname "$0")/../bin"
TEST_DIR="$(dirname "$0")"

RED=$(printf '\033[31m')
GREEN=$(printf '\033[32m')
RESET=$(printf '\033[0m')

status=0
cases_tmp=$(mktemp)

cleanup()
{
    rm -f "$cases_tmp"
}

die()
{
    echo "$1" >&2
    cleanup
    exit 1
}

is_known_prog()
{
    [ -x "$BIN_DIR/$1" ]
}

extract_number()
{
    # $1 = filename
    # strips: prog_  and  _in.txt
    name=$1
    tmp=${name#*_}
    num=${tmp%_in.txt}
    printf '%s' "$num"
}

collect_case()
{
    name=$1
    prog=${name%%_*}

    is_known_prog "$prog" || return 0

    case "$name" in
        ${prog}_[0-9]*_in.txt)
            num=$(extract_number "$name")
            printf '%s|%s\n' "$prog" "$num" >> "$cases_tmp"
            ;;
        *)
            die "Malformed test filename: $name"
            ;;
    esac
}

collect_cases()
{
    for f in "$TEST_DIR"/*; do
        name=$(basename "$f")
        case "$name" in
            *_in.txt)
                collect_case "$name"
                ;;
        esac
    done
}

run_case()
{
    prog=$1
    num=$2

    in="$TEST_DIR/${prog}_${num}_in.txt"
    out="$TEST_DIR/${prog}_${num}_out.txt"

    if [ ! -f "$out" ]; then
        printf '%sFAIL%s: Missing output file: %s\n' \
            "$RED" "$RESET" "$(basename "$out")" >&2
        status=1
        return
    fi

    if "$BIN_DIR/$prog" < "$in" | diff -u - "$out" >/dev/null 2>&1; then
        printf '%sOK%s %s_%s\n' "$GREEN" "$RESET" "$prog" "$num"
    else
        printf '%sFAIL%s %s_%s\n' "$RED" "$RESET" "$prog" "$num"
        status=1
    fi
}

run_cases()
{
    sorted=$(mktemp)
    sort -t '|' -k1,1 -k2,2n "$cases_tmp" > "$sorted"

    while IFS='|' read -r prog num; do
        run_case "$prog" "$num"
    done < "$sorted"

    rm -f "$sorted"
}

main()
{
    collect_cases
    run_cases
}

trap cleanup EXIT
main
exit "$status"
