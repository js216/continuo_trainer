-- SPDX-License-Identifier: MIT
-- score.lua --- performance analytics for figured bass exercises
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      score.lua is a post-processor that reads a stream of lesson data and
--      validation results from standard input. It calculates a performance
--      score based on accuracy and the latency between chord realizations.
--
-- INPUT FORMAT
--      The program expects three types of lines, typically piped from a
--      validation tool:
--
--      LESSON <id> <key> <signature> <description>
--          Resets the internal counters and sets the current lesson ID.
--
--      BASSNOTE <id>: <pitch> [comment]
--          Used to determine the total length of the exercise. The highest
--          <id> encountered defines the expected "last" result.
--
--      RESULT <id> TIME:<ms> <STATUS>
--          The evaluation result. <ms> is Unix time in milliseconds.
--          <STATUS> contains "OK" or "FAIL" (may include ANSI color codes).
--
-- OUTPUT FORMAT
--      Once the result for the final BASSNOTE ID is received, a single line
--      is written to stdout:
--
--      SCORE time=<time> lesson=<id> accuracy=<n> score=<n.nn> slowest=<s.sss> fastest=<s.sss> average=<s.sss>
--
--      - time: Unix time in milliseconds of the first group played in this lesson.
--      - accuracy: Percentage of "OK" results.
--      - score: 0 if accuracy < 100%, otherwise (correct_groups * speed_factor).
--      - speed_factor: 1.0 (slow) to 5.0 (fast) based on the slowest transition.
--      - slowest/fastest/average: Latency between consecutive results in seconds.
--
-- ERROR HANDLING
--      - Non-sequential IDs: The program waits until all IDs from 0 to the
--        maximum BASSNOTE ID have been received before calculating.
--      - Missing LESSON: If results arrive before a LESSON line, they are
--        ignored as the context is unknown.
--      - Accuracy < 100%: The score is automatically set to 0 regardless of speed.

local lesson_id = nil
local max_bass_id = -1
local results = {}

local function calculate_score()
	if max_bass_id < 0 then
		return
	end

	local total_groups = max_bass_id + 1
	local ok_count = 0
	local min_delta = math.huge
	local max_delta = 0
	local sum_delta = 0
	local transition_count = 0

	-- Ensure sequence completeness
	for i = 0, max_bass_id do
		if not results[i] then
			return
		end
		if results[i].status == "OK" then
			ok_count = ok_count + 1
		end

		if i > 0 then
			local delta = results[i].time - results[i - 1].time
			if delta < min_delta then
				min_delta = delta
			end
			if delta > max_delta then
				max_delta = delta
			end
			sum_delta = sum_delta + delta
			transition_count = transition_count + 1
		end
	end

	local accuracy = (ok_count / total_groups) * 100
	local score = 0.0

	-- Speed logic based on the SLOWEST group change
	if accuracy == 100 then
		local speed_factor
		if max_delta <= 50 then
			speed_factor = 5.0
		elseif max_delta >= 3000 then
			speed_factor = 1.0
		else
			-- Linear interpolation between 50ms and 3000ms
			speed_factor = 5.0 - (4.0 * (max_delta - 50) / (3000 - 50))
		end
		score = total_groups * speed_factor
	end

	-- Timing statistics in seconds
	local slowest = max_delta / 1000
	local fastest = (min_delta == math.huge) and 0 or (min_delta / 1000)
	local average = (transition_count > 0) and (sum_delta / transition_count / 1000) or 0

	local acc_fmt = (accuracy % 1 == 0) and string.format("%d", accuracy) or string.format("%.1f", accuracy)

	print(
		string.format(
			"SCORE time=%d lesson=%s accuracy=%s score=%.2f slowest=%.3f fastest=%.3f average=%.3f",
			results[0].time,
			tostring(lesson_id),
			acc_fmt,
			score,
			slowest,
			fastest,
			average
		)
	)
end

for line in io.lines() do
	-- Parse LESSON
	local lid = line:match("^LESSON%s+(%S+)")
	if lid then
		lesson_id = lid
		max_bass_id = -1
		results = {}
	end

	-- Parse BASSNOTE to find the finish line
	local bid = line:match("^BASSNOTE%s+(%d+):")
	if bid then
		local id_val = tonumber(bid)
		if id_val > max_bass_id then
			max_bass_id = id_val
		end
	end

	-- Parse RESULT
	local rid, rtime = line:match("^RESULT%s+(%d+)%s+TIME:(%d+)")
	if rid and rtime then
		local id_val = tonumber(rid)
		local status = line:find("OK") and "OK" or "FAIL"

		results[id_val] = {
			status = status,
			time = tonumber(rtime),
		}

		-- Check if this is the last expected result
		if id_val == max_bass_id then
			calculate_score()
		end
	end
end
