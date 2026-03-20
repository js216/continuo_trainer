// SPDX-License-Identifier: MIT
// rules.js --- trivial rule checker: always passes
// Copyright (c) 2026 Jakob Kastelic
//
// Returns a RESULT line for the given GROUP line.
// All rules OK everything for now; extend later with logic from rules.lua.

function evaluateGroup(groupLine) {
    const idM   = groupLine.match(/\bID:(\d+)\b/);
    const timeM = groupLine.match(/\bTIME:(\d+)\b/);
    const id   = idM   ? idM[1]   : "0";
    const time = timeM ? timeM[1] : String(Date.now());
    return `RESULT ${id} TIME:${time} OK`;
}
