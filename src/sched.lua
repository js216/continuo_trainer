-- SPDX-License-Identifier: MIT
-- sched.lua --- automatic lesson loader bridge
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      sched watches the stats pipeline output for STATS lines that carry a
--      lesson ID and automatically reloads that lesson.  It is intended as a
--      bridge between stats.lua and bin/load in graphs where the GUI is
--      absent.
--
-- INPUT
--      Any stream of lines.  Two patterns are matched:
--          lesson=<id>   (numeric) — remembered as the current lesson.
--          STATS         — triggers a LOAD_LESSON for the last seen ID.
--      All other lines are silently ignored.
--
-- OUTPUT
--      LOAD_LESSON <id>    Emitted each time a STATS line is received and a
--                          lesson ID has previously been seen.

io.stdout:setvbuf("no")

local current_lesson_id = nil

for line in io.lines() do
	line = line:gsub("\r", "")
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
