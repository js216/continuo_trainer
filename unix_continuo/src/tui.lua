-- SPDX-License-Identifier: MIT
-- tui.lua --- terminal UI filter for result visualization
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      tui.lua is a command-line filter that transforms raw log data into
--      a readable, color-coded terminal interface. It visualizes individual
--      test results as colored glyphs, formats performance scores with
--      conditional coloring, and highlights goal achievements from daily stats.
--
-- INPUT FORMAT
--      The program expects lines in the following formats:
--      RESULT <id> TIME:<t> <OK|FAIL> [error message]
--      SCORE time=<t> accuracy=<n> slowest=<s.ss> ...
--      STATS time=<t> total_today=<n.nn> goal=<n.nn> ...

local OVERWRITE_MODE = true -- All intermediate lines visible if false
local PREFIX = "MSG "

io.stdout:setvbuf("line") -- Ensure output flushes on every newline

-- ANSI Escape Codes
local GREEN = "\27[32m"
local RED = "\27[31m"
local RESET = "\27[0m"
local PREVIOUS_LINE = "\27[F"
local ERASE_LINE = "\27[K"
local GLYPH = "■"

local line_buffer = ""
local is_first_update = true

-- Helper to "overwrite" the line or print a new one based on OVERWRITE_MODE
local function redraw()
    local control_chars = ""

    if OVERWRITE_MODE and not is_first_update then
        -- Move up one line and clear it before printing the update
        control_chars = PREVIOUS_LINE .. ERASE_LINE
    end

    -- Every output begins with the prefix and ends with a newline to flush the buffer
    io.write(control_chars .. PREFIX .. line_buffer .. "\n")
    is_first_update = false
end

for line in io.lines() do
    -- Handle LESSON lines: Reset state for a fresh section
    if line:find("^LESSON") then
        line_buffer = ""
        is_first_update = true
        -- Print LESSON lines as standard output (not prefixed)
        io.write("\n\n" .. line .. "\n")

    -- Handle RESULT lines (Individual exercise items)
    elseif line:find("^RESULT") then
        local status = line:match("RESULT %d+ TIME:%d+ .-([%a]+)\27%[0m")
        local color = (status == "mOK") and GREEN or RED

        line_buffer = line_buffer .. color .. GLYPH .. RESET .. " "
        redraw()

    -- Handle SCORE lines (Session performance summary)
    elseif line:find("^SCORE") then
        local acc = line:match("accuracy=([%d%.]+)")
        local slow = line:match("slowest=([%d%.]+)")

        if acc and slow then
            local acc_val = tonumber(acc)
            line_buffer = line_buffer .. string.format("| %g%% %.1fs ", acc_val, tonumber(slow))
            redraw()
        end

    -- Handle STATS lines (Daily progress)
    elseif line:find("^STATS") then
        local total_str = line:match("total_today=([%d%.]+)")
        local goal_str = line:match("goal=([%d%.]+)")
        local streak = line:match("streak=([%d]+)")

        if total_str then
            local total = tonumber(total_str)
            local goal = tonumber(goal_str) or 0
            local pts = math.floor(total + 0.5)

            line_buffer = line_buffer .. string.format("pts=%d", pts)

            if total >= goal and goal > 0 then
                line_buffer = line_buffer .. " " .. GREEN .. "streak=" .. streak .. RESET
            end

            redraw()

            -- Reset for the next session
            line_buffer = ""
            is_first_update = true
        end
    end
end
