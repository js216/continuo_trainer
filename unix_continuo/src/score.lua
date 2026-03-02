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
--      Any individual transition duration longer than MAX_DURATION between
--      groups causes that group to be counted as a failure. For statistics,
--      latencies longer than MAX_DURATION are truncated to MAX_DURATION.
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
--      - Accuracy < 100%: Missing results, "FAIL" status, or transitions
--        exceeding 60s count against accuracy. The score is 0 unless 100%
--        accuracy is achieved.

local MAX_DURATION = 60 -- Seconds
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
	local total_duration = 0

	-- Sequence processing
	for i = 0, max_bass_id do
		if results[i] then
			local current_status = results[i].status

			-- Check for timeout failure and calculate latency
			if i > 0 and results[i - 1] then
				local delta = results[i].time - results[i - 1].time
				local delta_sec = delta / 1000

				-- If duration exceeds MAX_DURATION, treat as a failed group and truncate
				if delta_sec > MAX_DURATION then
					current_status = "FAIL"
					delta_sec = MAX_DURATION
				end

				if delta_sec * 1000 < min_delta then
					min_delta = delta_sec * 1000
				end
				if delta_sec * 1000 > max_delta then
					max_delta = delta_sec * 1000
				end
				sum_delta = sum_delta + (delta_sec * 1000)
				transition_count = transition_count + 1
				total_duration = total_duration + delta_sec
			end

			if current_status == "OK" then
				ok_count = ok_count + 1
			end
		end
	end

	local accuracy = (ok_count / total_groups) * 100
	local score = 0.0

	-- Speed logic based on the SLOWEST group change (max_delta is already truncated)
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

	local acc_fmt = (accuracy % 1 == 0) and string.format("%d", accuracy) or string.format("%.1f", accuracy)

	print(
		string.format(
			"SCORE time=%d lesson=%s accuracy=%s score=%.2f duration=%.3f slowest=%.3f fastest=%.3f average=%.3f",
			results[first_idx].time,
			tostring(lesson_id),
			acc_fmt,
			score,
			total_duration,
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
