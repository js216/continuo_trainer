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

io.stdout:setvbuf("line")

for line in io.lines() do
	local lesson_id = line:match("lesson=(%d+)")

	if lesson_id then
		print("LOAD_LESSON " .. lesson_id)
	end
end
