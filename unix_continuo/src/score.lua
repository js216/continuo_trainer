-- SPDX-License-Identifier: MIT
-- score.lua --- performance analytics for figured bass exercises
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      score.lua is a post-processor that reads a stream of lesson data and
--      validation results from standard input. It calculates a performance
--      score based on accuracy and the latency between chord realizations.
--
--      The script detects abandoned lessons (if a new LESSON starts before
--      the previous one is finished) and calculates the score for the
--      realized portion. Accuracy is calculated against the total expected
--      groups; unrealized groups count as incorrect.
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
--          <STATUS> contains "OK" or "FAIL".
--
-- OUTPUT FORMAT
--      Once a lesson is completed or abandoned, a single line is written:
--
--      SCORE time=<time> lesson=<id> accuracy=<n> score=<n.nn> duration=<s.sss> slowest=<s.sss> fastest=<s.sss> average=<s.sss>
--
--      - time: Unix time in milliseconds of the first group played.
--      - accuracy: Percentage of "OK" results out of total expected groups.
--      - score: 0 if accuracy < 100%, otherwise (correct_groups * speed_factor).
--      - duration: Total time between first and last result in seconds.
--      - slowest/fastest/average: Latency between consecutive results in seconds.
--
-- ERROR HANDLING
--      - Abandoned Lessons: If a LESSON keyword appears while a lesson is
--        underway, the script processes the partial results before resetting.
--      - Accuracy < 100%: Missing results or "FAIL" status count against
--        accuracy. The score is 0 unless 100% accuracy is achieved.

local lesson_id = nil
local max_bass_id = -1
local results = {}

local function calculate_score()
	-- Guard against empty lessons or lessons with no results
	local first_idx = nil
	local last_idx = nil
	for i = 0, max_bass_id do
		if results[i] then
			if not first_idx then
				first_idx = i
			end
			last_idx = i
		end
	end

	if max_bass_id < 0 or not first_idx then
		return
	end

	local total_groups = max_bass_id + 1
	local ok_count = 0
	local min_delta = math.huge
	local max_delta = 0
	local sum_delta = 0
	local transition_count = 0

	-- Sequence processing
	for i = 0, max_bass_id do
		if results[i] then
			if results[i].status == "OK" then
				ok_count = ok_count + 1
			end

			-- Only calculate latency if the previous result also exists
			if i > 0 and results[i - 1] then
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
			speed_factor = 5.0 - (4.0 * (max_delta - 50) / (3000 - 50))
		end
		score = total_groups * speed_factor
	end

	-- Timing statistics in seconds
	local slowest = max_delta / 1000
	local fastest = (min_delta == math.huge) and 0 or (min_delta / 1000)
	local average = (transition_count > 0) and (sum_delta / transition_count / 1000) or 0
	local duration = (results[last_idx].time - results[first_idx].time) / 1000

	local acc_fmt = (accuracy % 1 == 0) and string.format("%d", accuracy) or string.format("%.1f", accuracy)

	print(
		string.format(
			"SCORE time=%d lesson=%s accuracy=%s score=%.2f duration=%.3f slowest=%.3f fastest=%.3f average=%.3f",
			results[first_idx].time,
			tostring(lesson_id),
			acc_fmt,
			score,
			duration,
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
		-- Detect abandoned lesson: if we have a lesson_id but didn't finish the last max_bass_id
		if lesson_id and not results[max_bass_id] then
			calculate_score()
		end

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

		-- Standard completion
		if id_val == max_bass_id then
			calculate_score()
			-- Reset to avoid double-reporting if EOF is reached immediately after
			lesson_id = nil
		end
	end
end

-- Handle case where the very last lesson in the stream was abandoned
if lesson_id and not results[max_bass_id] then
	calculate_score()
end
