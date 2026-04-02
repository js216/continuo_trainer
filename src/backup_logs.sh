#!/bin/bash
# Back up log/stats.log with a date-stamped filename

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SRC="$PROJECT_DIR/log/stats.log"
DEST="$PROJECT_DIR/log/stats_$(date +'%-d%b%y' | tr '[:upper:]' '[:lower:]').log"

if [ -f "$SRC" ]; then
    cp "$SRC" "$DEST"
else
    echo "backup_logs: $SRC not found" >&2
    exit 1
fi
