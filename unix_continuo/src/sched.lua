-- SPDX-License-Identifier: MIT
-- lesson_filter.lua --- bridge for automatic lesson loading
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      lesson_filter.lua is a stdin/stdout bridge that extracts lesson
--      IDs from a performance stream to trigger lesson loading.
--
-- INPUT FORMAT
--      Scans for the pattern "lesson=<id>" where <id> is numeric.
--
-- OUTPUT FORMAT
--      Writes "LOAD_LESSON <id>" to stdout immediately upon match.

io.stdout:setvbuf("no")

local current_lesson_id = nil

for line in io.lines() do
	-- 1. Try to update the ID whenever a line contains "lesson="
	local found_id = line:match("lesson=(%d+)")
	if found_id then
		current_lesson_id = found_id
	end

	-- 2. When "STATS" is found, use the last known ID
	if line:find("STATS") then
		if current_lesson_id then
			print("LOAD_LESSON " .. current_lesson_id)
			-- Optional: reset to nil if you only want to load it once per session
			-- current_lesson_id = nil
		else
			-- Debugging: Useful if STATS arrives before any lesson ID is ever set
			io.stderr:write("tui.lua: STATS received but no lesson_id seen yet\n")
		end
	end
end
