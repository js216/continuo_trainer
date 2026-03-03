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

-------------------------------------------------------------------------------

io.stdout:setvbuf("no") -- Force unbuffered output

-- ANSI Color Constants
local GREEN = "\27[32m"
local RED = "\27[31m"
local RESET = "\27[0m"
local GLYPH = "■"

for line in io.lines() do
	-- Handle LESSON lines
	if line:find("^LESSON") then
		io.write("\n" .. line .. "\n")

	-- Handle RESULT lines (Individual exercise items)
	elseif line:find("^RESULT") then
		local status = line:match("RESULT %d+ TIME:%d+ .-([%a]+)\27%[0m")

		local color = (status == "mOK") and GREEN or RED
		io.write(color .. GLYPH .. RESET .. " ")
		io.stdout:flush()

	-- Handle SCORE lines (Session performance summary)
	elseif line:find("^SCORE") then
		local acc = line:match("accuracy=([%d%.]+)")
		local slow = line:match("slowest=([%d%.]+)")

		if acc and slow then
			local acc_val = tonumber(acc)
			io.write(string.format(" | %g%% %.1fs", acc_val, tonumber(slow)))
			io.stdout:flush()
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

			io.write(string.format(" pts=%d", pts))

			-- Display achievement message ONLY when goal is reached
			if total >= goal and goal > 0 then
				io.write(" " .. GREEN .. "streak=" .. streak .. RESET .. "\n")
			end
			io.stdout:flush()
		end
	end
end
