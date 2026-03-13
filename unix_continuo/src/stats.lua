-- SPDX-License-Identifier: MIT
-- stats.lua --- performance scoring, progress tracking and spaced repetition
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      Accumulates RESULT lines for the current lesson in memory.  When the
--      last RESULT arrives (or the lesson is abandoned), it computes accuracy,
--      score and timing, then updates Mastery, Power and SRS fields for both
--      the lesson and every chunk whose heart falls inside that lesson.
--
--      During a CHUNK_SESSION the same pipeline runs, but only stats.chunks
--      is updated; stats.lessons is left untouched.
--
--      Mastery growth is gated by a quality score:
--          quality = ema_pass × speed_factor × evenness_factor
--      ema_pass:        EMA of pass/fail over ~10 sessions (alpha=0.18).
--      speed_factor:    session average relative to the personal best; ≤1.
--      evenness_factor: fastest/slowest note ratio; 1 = perfectly even.
--      Mastery grows only when accuracy ≥ 80% and score exceeds mastery.
--
-- ARGUMENTS
--      arg[1]: Path to the stats file (e.g. "log/stats.log").  A valid Lua
--              script returning a table; created on first write if absent.
--
-- INPUT
--      LESSON <n> <key> <time> <title>          from bin/load
--          Starts a new lesson accumulator.  During a CHUNK_SESSION, <n>
--          is the chunk hash; the accumulator is flagged is_chunk=true and
--          stats.lessons is not updated on completion.
--      BASSNOTE <i>: <token> [passing]          from bin/load
--      RESULT <i> TIME:<ms> OK|FAIL [msg]       from lua src/rules.lua
--      CHUNK LESSON:<n> HEART:<s>-<e> LEN:<l> HASH:<h> KEY:<k>
--            SKILLS:<s> BASSLINE:<b> FIGURE:<f> MELODY:<m>
--                                               from lua src/chunk.lua
--          Registers chunk metadata for the current lesson; used to update
--          per-chunk stats when the parent lesson is finalised.
--      CHUNK_SESSION <hash>                     from bin/load
--          Signals that the next LESSON is a chunk replay; suppresses
--          routes finalisation to stats.chunks.
--      LESSON_NAME <n>                          from lua src/all.lua
--          Ensures stats.lessons[n] exists with defaults; saves if new.
--          Records n as a known live lesson for ALL_SCANNED validation.
--      CHUNK_NAME <hash>                        from lua src/all.lua
--          Ensures stats.chunks[hash] exists with defaults; saves if new.
--          Records hash as a known live chunk for ALL_SCANNED validation.
--      ALL_SCANNED                              from lua src/all.lua
--          Verifies that every entry in stats.lessons and stats.chunks was
--          announced via LESSON_NAME / CHUNK_NAME.  Any unannounced (stale)
--          entry is reported to stderr and the process exits with code 1.
--      LOAD_LESSON <n>                          from bin/gui
--          Emits a STATS line for lesson n (no scoring).
--      QUERY_STATS                              from bin/gui
--          Emits a STATS line for the current daily totals.
--      SUGGEST_LESSON                           from bin/gui
--          Selects and emits the best item to practice: first checks all
--          known chunks for any that are weak or never played; falls back
--          to lesson selection if all chunks are mastered.
--
-- OUTPUT
--      After every completed lesson:
--      STATS time=<t> total_today=<n.nn> goal=<n.nn>
--            total_duration_today=<s.sss> streak=<n>
--            lesson=<id>[ivl=<n>,ease=<n.nn>,tot_dur=<s.sss>,
--                        n_pass_tot=<n>,n_fail_tot=<n>,
--                        mastery=<n.nn>,power=<n.nn>]
--            [suggestion=<token>]
--
--      After every completed chunk session:
--      STATS time=<t> total_today=<n.nn> goal=<n.nn>
--            total_duration_today=<s.sss> streak=<n>
--            chunk=<hash>[ivl=<n>,ease=<n.nn>,mastery=<n.nn>,power=<n.nn>]
--            [suggestion=<token>]
--
--      In response to QUERY_STATS or LOAD_LESSON:
--      STATS time=<t> total_today=<n.nn> goal=<n.nn>
--            total_duration_today=<s.sss> streak=<n> [lesson=<id>[...]]
--
--      In response to SUGGEST_LESSON:
--      SUGGESTION chunk=<hash> lesson=<id> skills=<s>
--                 reason=<weak_chunk|new_chunk>
--      SUGGESTION lesson=<id> reason=<overdue|needs_work|new_lesson>
--      SUGGESTION none reason=<all_up_to_date|no_lessons_available>
--
--      mastery and power are normalised to [0,100] as
--      (raw / (max_groups × 5.0)) × 100.

-- Any single transition longer than MAX_DURATION seconds is clamped to that
-- value and counted as a failure.
local MAX_DURATION = 60

io.stdout:setvbuf("line") -- flush after every STATS/SUGGESTION line

-- Algorithm parameter defaults.  Written into the stats log under the
-- "algorithm" key on first run (or when a key is absent), so they can be
-- tuned by editing the log file directly.
local ALGORITHM_DEFAULTS = {
	score_goal = 1000, -- daily point target
	ema_alpha = 0.18, -- EMA smoothing for pass-rate (~10-session window)
	pass_accuracy = 80, -- minimum accuracy % to count as a pass
	mastery_growth = 0.20, -- fraction of (score - mastery) gap closed per session
	power_half_life = 0.693, -- ln(2): power reaches 50% of mastery at days_elapsed == ivl
	mastery_points_per_pct = 2.5, -- points awarded per 1% normalised mastery gain
	power_points_per_pct = 1.0,  -- points awarded per 1% normalised power gain
	bottleneck_thresh = 0.6, -- factor value below which it is considered a bottleneck
	dominance_margin = 0.2, -- how much lower the bottleneck must be than the others
	min_quality = 0.1, -- quality below this triggers a "raise quality" suggestion
	mastery_score_frac = 0.9, -- score must be >= this fraction of mastery to say "already mastered"
	max_consecutive = 10, -- consecutive attempts before "try_another_lesson"
	weak_ema_thresh = 0.8, -- ema_pass below this marks a lesson as "needs_work"
	ease_initial = 2.5, -- starting ease factor for new lessons
	ease_min = 1.3, -- minimum ease factor
	ease_max = 3.5, -- maximum ease factor
	ease_pass_delta = 0.1, -- ease increase on a perfect pass
	ease_fail_delta = 0.2, -- ease decrease on a fail
	ivl_first = 1, -- interval (days) after first perfect pass
	ivl_second = 6, -- interval (days) after second consecutive perfect pass
	ivl_max = 365, -- maximum interval (days); caps runaway SRS growth
	chunk_mastery_thresh = 60, -- normalised % below which a chunk needs practice
	chunk_power_thresh = 40, -- normalised % below which a chunk needs practice
}

local stats_file = arg[1]

if not stats_file then
	io.write("Usage: lua stats.lua <stats_file.lua>\n")
	os.exit(1)
end

-------------------------------------------------------------------------------
-- UTILITIES & METRICS
-------------------------------------------------------------------------------

local function get_date_str()
	return os.date("%Y-%m-%d")
end

local function calculate_power(l_data, alg)
	local mastery = l_data.mastery or 0
	local ivl = l_data.ivl or 0
	local last_date = l_data.last_date

	if not last_date or ivl <= 0 then
		return 0
	end

	local y, m, d = last_date:match("(%d+)-(%d+)-(%d+)")
	local last_ts = os.time({ year = y, month = m, day = d, hour = 12 })
	local days_elapsed = math.floor(os.difftime(os.time(), last_ts) / 86400)

	local stability = math.exp(-alg.power_half_life * (math.max(0, days_elapsed) / ivl))
	return math.min(mastery, mastery * stability)
end

local function calculate_streak(data)
	local streak = 0
	local current_ts = os.time()
	local today = get_date_str()

	while true do
		local d_str = os.date("%Y-%m-%d", current_ts)
		local day_score = (data.daily[d_str] and data.daily[d_str].score) or 0
		if day_score >= data.algorithm.score_goal then
			streak = streak + 1
			current_ts = current_ts - 86400
		else
			if d_str == today and streak == 0 then
				current_ts = current_ts - 86400
			else
				break
			end
		end
	end
	return streak
end

local function normalize(raw, l_data)
	local max_groups = l_data.max_groups or 0
	if max_groups <= 0 then
		return raw
	end
	return (raw / (max_groups * 5.0)) * 100.0
end

-------------------------------------------------------------------------------
-- DATA PERSISTENCE
-------------------------------------------------------------------------------

local function load_stats(path)
	local function with_defaults(data)
		data.algorithm = data.algorithm or {}
		for k, v in pairs(ALGORITHM_DEFAULTS) do
			if data.algorithm[k] == nil then
				data.algorithm[k] = v
			end
		end
		data.daily = data.daily or {}
		data.lessons = data.lessons or {}
		data.chunks = data.chunks or {}
		return data
	end

	local f = io.open(path, "r")
	if not f then
		return with_defaults({})
	end
	f:close()
	local chunk = loadfile(path)
	if not chunk then
		return with_defaults({})
	end
	return with_defaults(chunk())
end

local function save_stats(path, data)
	local f = io.open(path, "w")
	if not f then
		return
	end
	local function serialize(t, indent, depth)
		depth = depth or 0
		local s = "{\n"
		local next_indent = indent .. "  "
		local keys = {}
		for k in pairs(t) do
			table.insert(keys, k)
		end
		table.sort(keys, function(a, b)
			return tostring(a) < tostring(b)
		end)
		for _, k in ipairs(keys) do
			local v = t[k]
			s = s .. next_indent .. "[" .. (type(k) == "string" and string.format("%q", k) or k) .. "] = "
			if type(v) == "table" then
				if depth == 1 and (v.score or v.duration) and not v.ease then
					s = s .. '{["score"] = ' .. (v.score or 0) .. ', ["duration"] = ' .. (v.duration or 0) .. "},\n"
				else
					s = s .. serialize(v, next_indent, depth + 1) .. ",\n"
				end
			elseif type(v) == "string" then
				s = s .. string.format("%q", v) .. ",\n"
			else
				s = s .. tostring(v) .. ",\n"
			end
		end
		return s .. indent .. "}"
	end
	f:write("return " .. serialize(data, "", 0) .. "\n")
	f:close()
end

-------------------------------------------------------------------------------
-- SCORE COMPUTATION  (logic from score.lua)
-------------------------------------------------------------------------------

-- Compute accuracy, score and timing for results[s..e] (0-based, inclusive).
-- Returns nil if there are no results in range.
local function compute_score_data(results, s, e)
	local first_idx = nil
	for i = s, e do
		if results[i] then
			first_idx = i
			break
		end
	end
	if not first_idx then
		return nil
	end

	local total_groups = e - s + 1
	local ok_count = 0
	local min_delta = math.huge
	local max_delta = 0
	local sum_delta = 0
	local transition_count = 0
	local total_duration = 0

	for i = s, e do
		if results[i] then
			local status = results[i].status
			if i > s and results[i - 1] then
				local delta_sec = (results[i].time - results[i - 1].time) / 1000
				if delta_sec > MAX_DURATION then
					status = "FAIL"
					delta_sec = MAX_DURATION
				end
				local delta_ms = delta_sec * 1000
				if delta_ms < min_delta then
					min_delta = delta_ms
				end
				if delta_ms > max_delta then
					max_delta = delta_ms
				end
				sum_delta = sum_delta + delta_ms
				transition_count = transition_count + 1
				total_duration = total_duration + delta_sec
			end
			if status == "OK" then
				ok_count = ok_count + 1
			end
		end
	end

	local accuracy = (ok_count / total_groups) * 100
	local score = 0.0
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

	return {
		time = results[first_idx].time,
		accuracy = accuracy,
		score = score,
		duration = total_duration,
		slowest = max_delta / 1000,
		fastest = (min_delta == math.huge) and 0 or (min_delta / 1000),
		average = (transition_count > 0) and (sum_delta / transition_count / 1000) or 0,
		groups = total_groups,
	}
end

-------------------------------------------------------------------------------
-- ENTRY UPDATE  (shared by lesson and chunk entries)
-------------------------------------------------------------------------------

-- Update a lesson or chunk entry with new score data sd.
-- Returns a table of computed factors for the caller's use.
local function update_entry(entry, sd, alg)
	local today = get_date_str()

	if sd.groups > (entry.max_groups or 0) then
		entry.max_groups = sd.groups
	end

	local old_mastery = entry.mastery or 0
	local old_power = calculate_power(entry, alg)

	-- EMA pass rate
	local pass_this_session = (sd.accuracy >= alg.pass_accuracy) and 1.0 or 0.0
	entry.ema_pass = entry.ema_pass and (alg.ema_alpha * pass_this_session + (1 - alg.ema_alpha) * entry.ema_pass)
		or pass_this_session

	-- Speed factor
	local speed_factor
	if sd.average > 0 then
		if not entry.best_avg or sd.average < entry.best_avg then
			entry.best_avg = sd.average
		end
		speed_factor = math.min(1.0, entry.best_avg / sd.average)
	else
		-- No timing transitions (e.g. single-note heart): speed unmeasurable,
		-- do not penalise.
		speed_factor = 1.0
	end

	-- Evenness factor
	local evenness_factor
	if sd.slowest > 0 and sd.fastest > 0 then
		evenness_factor = math.min(1.0, sd.fastest / sd.slowest)
	else
		-- Single transition or no transitions: evenness unmeasurable, do not penalise.
		evenness_factor = 1.0
	end

	local quality = entry.ema_pass * speed_factor * evenness_factor

	-- Mastery
	if sd.accuracy >= alg.pass_accuracy and sd.score > old_mastery then
		entry.mastery = old_mastery + (sd.score - old_mastery) * alg.mastery_growth * quality
	end

	-- SRS
	if sd.accuracy == 100 then
		entry.n_pass = (entry.n_pass or 0) + 1
		entry.n_fail = 0
		entry.n_pass_tot = (entry.n_pass_tot or 0) + 1
		if entry.n_pass == 1 then
			entry.ivl = alg.ivl_first
		elseif entry.n_pass == 2 then
			entry.ivl = alg.ivl_second
		else
			entry.ivl = math.min(alg.ivl_max, math.ceil((entry.ivl or 1) * (entry.ease or alg.ease_initial)))
		end
		entry.ease = math.min(alg.ease_max, (entry.ease or alg.ease_initial) + alg.ease_pass_delta)
	else
		entry.n_fail = (entry.n_fail or 0) + 1
		entry.n_pass = 0
		entry.n_fail_tot = (entry.n_fail_tot or 0) + 1
		entry.ivl = 1
		entry.ease = math.max(alg.ease_min, (entry.ease or alg.ease_initial) - alg.ease_fail_delta)
	end

	entry.total_duration = (entry.total_duration or 0) + sd.duration
	entry.last_date = today

	local new_power = calculate_power(entry, alg)
	return {
		old_mastery = old_mastery,
		m_delta = math.max(0, (entry.mastery or 0) - old_mastery),
		p_delta = math.max(0, new_power - old_power),
		quality = quality,
		speed_factor = speed_factor,
		evenness_factor = evenness_factor,
	}
end

-------------------------------------------------------------------------------
-- OUTPUT HELPER
-------------------------------------------------------------------------------

local function print_stats_line(stats, l_id, timestamp, suggestion)
	local alg = stats.algorithm
	local today = get_date_str()
	local d = stats.daily[today] or { score = 0, duration = 0 }
	local streak = calculate_streak(stats)

	local output = string.format(
		"STATS time=%.0f total_today=%.2f goal=%.2f total_duration_today=%.3f streak=%d",
		timestamp or os.time(),
		d.score,
		alg.score_goal,
		d.duration,
		streak
	)

	if l_id and stats.lessons[l_id] then
		local l = stats.lessons[l_id]
		local power = calculate_power(l, alg)
		output = output
			.. string.format(
				" lesson=%s[ivl=%d,ease=%.2f,tot_dur=%.3f,n_pass_tot=%d,n_fail_tot=%d,mastery=%.2f,power=%.2f]",
				l_id,
				math.tointeger(math.floor(l.ivl or 0)) or 0,
				l.ease or alg.ease_initial,
				l.total_duration or 0,
				math.tointeger(l.n_pass_tot or 0) or 0,
				math.tointeger(l.n_fail_tot or 0) or 0,
				normalize(l.mastery or 0, l),
				normalize(power, l)
			)
	end
	if suggestion then
		output = output .. " suggestion=" .. suggestion
	end
	io.write(output .. "\n")
end

-- Forward declaration: populated as CHUNK lines arrive, consumed by handle_suggest.
local lesson_chunks_mem = {} -- { [l_id] = { {hash, heart_s, heart_e, len, skills}, ... } }

-------------------------------------------------------------------------------
-- COMMAND HANDLERS
-------------------------------------------------------------------------------

local function handle_suggest(stats)
	local alg = stats.algorithm

	-- First: scan all lesson chunks for any that are weak or never played.
	local weakest_hash, weakest_skills, weakest_lesson = nil, nil, nil
	local weakest_score = math.huge
	local new_hash, new_skills, new_lesson = nil, nil, nil

	for l_id, chunks in pairs(lesson_chunks_mem) do
		for _, ci in ipairs(chunks) do
			local c = stats.chunks[ci.hash]
			if not c or not c.last_date then
				if not new_hash then
					new_hash, new_skills, new_lesson = ci.hash, ci.skills or "?", l_id
				end
			else
				local m_pct = normalize(c.mastery or 0, c)
				local p_pct = normalize(calculate_power(c, alg), c)
				if m_pct < alg.chunk_mastery_thresh or p_pct < alg.chunk_power_thresh then
					local score = m_pct + p_pct
					if score < weakest_score then
						weakest_score = score
						weakest_hash, weakest_skills, weakest_lesson = ci.hash, ci.skills or "?", l_id
					end
				end
			end
		end
	end

	if weakest_hash then
		io.write(string.format(
			"SUGGESTION chunk=%s lesson=%s skills=%s reason=weak_chunk\n",
			weakest_hash, weakest_lesson, weakest_skills
		))
		return
	end
	if new_hash then
		io.write(string.format(
			"SUGGESTION chunk=%s lesson=%s skills=%s reason=new_chunk\n",
			new_hash, new_lesson, new_skills
		))
		return
	end

	-- No weak/new chunks: fall through to lesson selection.
	local available = {}
	for id in pairs(stats.lessons) do
		if tonumber(id) then table.insert(available, id) end
	end
	table.sort(available, function(a, b) return tonumber(a) < tonumber(b) end)

	if #available == 0 then
		io.write("SUGGESTION none reason=no_lessons_available\n")
		return
	end

	local known = {}
	local new_lessons = {}
	for _, id in ipairs(available) do
		local l = stats.lessons[id]
		if l and l.last_date then
			table.insert(known, id)
		else
			table.insert(new_lessons, id)
		end
	end

	local best_id = nil
	local best_score = -math.huge
	local best_type = nil

	for _, id in ipairs(known) do
		local l = stats.lessons[id]
		local ivl = l.ivl or 0
		local last_date = l.last_date
		local days_elapsed = 0
		if last_date then
			local y, m, d = last_date:match("(%d+)-(%d+)-(%d+)")
			local last_ts = os.time({ year = y, month = m, day = d, hour = 12 })
			days_elapsed = math.floor(os.difftime(os.time(), last_ts) / 86400)
		end

		if ivl > 0 and days_elapsed >= ivl then
			local score = days_elapsed / ivl
			if best_type ~= "overdue" or score > best_score then
				best_type = "overdue"
				best_score = score
				best_id = id
			end
		elseif best_type ~= "overdue" then
			local ep = l.ema_pass or 0
			if ep < alg.weak_ema_thresh then
				local score = 1.0 - ep
				if score > best_score then
					best_score = score
					best_id = id
				end
			end
		end
	end

	if best_id then
		local reason = (best_type == "overdue") and "overdue" or "needs_work"
		io.write("SUGGESTION lesson=" .. best_id .. " reason=" .. reason .. "\n")
		return
	end

	if #new_lessons > 0 then
		io.write("SUGGESTION lesson=" .. new_lessons[1] .. " reason=new_lesson\n")
		return
	end

	io.write("SUGGESTION none reason=all_up_to_date\n")
end

-------------------------------------------------------------------------------
-- IN-MEMORY LESSON ACCUMULATOR
-------------------------------------------------------------------------------

local current = nil -- { id, max_bass_id, results={[i]={status,time}} }

local function handle_chunk(line)
	local n, s, e, h = line:match("^CHUNK LESSON:(%d+) HEART:(%d+)-(%d+) %S+ HASH:(%x+)")
	if not (n and s and e and h) then
		return
	end
	local len = tonumber(line:match("LEN:(%d+)")) or 1
	local skills = line:match("SKILLS:(.-) BASSLINE:") or "?"
	local l_id = tostring(tonumber(n))
	if not lesson_chunks_mem[l_id] then
		lesson_chunks_mem[l_id] = {}
	end
	for _, ci in ipairs(lesson_chunks_mem[l_id]) do
		if ci.hash == h then
			return
		end
	end
	lesson_chunks_mem[l_id][#lesson_chunks_mem[l_id] + 1] = {
		hash = h,
		heart_s = tonumber(s),
		heart_e = tonumber(e),
		len = len,
		skills = skills,
	}
end

-------------------------------------------------------------------------------
-- LESSON FINALISATION
-------------------------------------------------------------------------------

local function finalize(stats)
	if not current or current.max_bass_id < 0 then
		current = nil
		return
	end

	local sd = compute_score_data(current.results, 0, current.max_bass_id)
	if not sd then
		current = nil
		return
	end

	local alg = stats.algorithm
	local today = get_date_str()

	-- ── chunk session: update stats.chunks only ───────────────────────────────
	if current.is_chunk then
		local hash = tostring(current.id)
		local c = stats.chunks[hash]
			or { ease = alg.ease_initial, ivl = 0, mastery = 0, total_duration = 0, max_groups = 0 }
		if alg.last_lesson_scored ~= hash then
			c.n_consecutive = 1
		else
			c.n_consecutive = (c.n_consecutive or 0) + 1
		end
		alg.last_lesson_scored = hash
		local res = update_entry(c, sd, alg)
		stats.chunks[hash] = c
		local points = normalize(res.m_delta, c) * alg.mastery_points_per_pct
		            + normalize(res.p_delta, c) * alg.power_points_per_pct
		stats.daily[today] = stats.daily[today] or { score = 0, duration = 0 }
		stats.daily[today].score = stats.daily[today].score + points
		stats.daily[today].duration = stats.daily[today].duration + sd.duration
		save_stats(stats_file, stats)
		local power = calculate_power(c, alg)
		local d = stats.daily[today]
		local streak = calculate_streak(stats)
		local suggestion = c.n_consecutive >= alg.max_consecutive and "try_another_lesson" or nil
		io.write(string.format(
			"STATS time=%.0f total_today=%.2f goal=%.2f total_duration_today=%.3f streak=%d"
				.. " chunk=%s[ivl=%d,ease=%.2f,mastery=%.2f,power=%.2f]%s\n",
			sd.time, d.score, alg.score_goal, d.duration, streak,
			hash, math.floor(c.ivl or 0), c.ease or alg.ease_initial,
			normalize(c.mastery or 0, c), normalize(power, c),
			suggestion and (" suggestion=" .. suggestion) or ""
		))
		current = nil
		return
	end

	local l_id = tostring(current.id)

	-- ── lesson entry ──────────────────────────────────────────────────────────
	local l = stats.lessons[l_id]
		or { ease = alg.ease_initial, ivl = 0, mastery = 0, total_duration = 0, max_groups = 0 }

	if alg.last_lesson_scored ~= l_id then
		l.n_consecutive = 1
	else
		l.n_consecutive = (l.n_consecutive or 0) + 1
	end
	alg.last_lesson_scored = l_id

	local res = update_entry(l, sd, alg)
	stats.lessons[l_id] = l

	-- ── suggestion (lesson only) ──────────────────────────────────────────────
	local suggestion = nil
	if sd.accuracy < alg.pass_accuracy then
		suggestion = "try_again"
	else
		local factors = {
			{ name = "be_more_consistent", val = l.ema_pass },
			{ name = "play_faster", val = res.speed_factor },
			{ name = "play_more_evenly", val = res.evenness_factor },
		}
		local min_val, min_idx = math.huge, nil
		for i, f in ipairs(factors) do
			if f.val < min_val then
				min_val = f.val
				min_idx = i
			end
		end
		if min_idx and min_val < alg.bottleneck_thresh then
			local dominant = true
			for i, f in ipairs(factors) do
				if i ~= min_idx and (f.val - min_val) < alg.dominance_margin then
					dominant = false
					break
				end
			end
			if dominant then
				suggestion = factors[min_idx].name
			end
		end
		if suggestion == nil then
			if sd.score <= res.old_mastery then
				if res.old_mastery > 0 and sd.score >= res.old_mastery * alg.mastery_score_frac then
					suggestion = "already_mastered"
				end
			elseif res.quality < alg.min_quality then
				local weakest, wval = "be_more_consistent", l.ema_pass
				if res.speed_factor < wval then
					weakest, wval = "play_faster", res.speed_factor
				end
				if res.evenness_factor < wval then
					weakest = "play_more_evenly"
				end
				suggestion = "raise_quality_" .. weakest
			end
		end
	end
	if l.n_consecutive >= alg.max_consecutive then
		suggestion = "try_another_lesson"
	end

	-- ── daily totals (lesson only) ────────────────────────────────────────────
	local points = normalize(res.m_delta, l) * alg.mastery_points_per_pct
	            + normalize(res.p_delta, l) * alg.power_points_per_pct
	stats.daily[today] = stats.daily[today] or { score = 0, duration = 0 }
	stats.daily[today].score = stats.daily[today].score + points
	stats.daily[today].duration = stats.daily[today].duration + sd.duration

	-- ── chunk entries ─────────────────────────────────────────────────────────
	for _, ci in ipairs(lesson_chunks_mem[l_id] or {}) do
		local c = stats.chunks[ci.hash]
			or { ease = alg.ease_initial, ivl = 0, mastery = 0, total_duration = 0, max_groups = ci.len }
		local chunk_sd = compute_score_data(current.results, ci.heart_s, ci.heart_e)
		if chunk_sd then
			update_entry(c, chunk_sd, alg)
		end
		stats.chunks[ci.hash] = c
	end

	save_stats(stats_file, stats)
	print_stats_line(stats, l_id, sd.time, suggestion)

	current = nil
end

-------------------------------------------------------------------------------
-- MAIN LOOP
-------------------------------------------------------------------------------

local in_chunk_session = false
local pending_suggest = false -- defer SUGGEST_LESSON until first CHUNK arrives
local scanned_lessons = {} -- lessons announced via LESSON_NAME this session
local scanned_chunks  = {} -- chunks announced via CHUNK_NAME this session

for line in io.lines() do
	line = line:gsub("\r", "")
	-- LESSON_NAME / CHUNK_NAME: emitted by all.lua; initialise entries if absent
	if line:match("^LESSON_NAME ") then
		local n = line:match("^LESSON_NAME (%S+)")
		scanned_lessons[n] = true
		local stats = load_stats(stats_file)
		if not stats.lessons[n] then
			stats.lessons[n] = { ease = stats.algorithm.ease_initial, ivl = 0,
			                     mastery = 0, total_duration = 0, max_groups = 0 }
			save_stats(stats_file, stats)
		end
	elseif line:match("^CHUNK_NAME ") then
		local h = line:match("^CHUNK_NAME (%S+)")
		scanned_chunks[h] = true
		local stats = load_stats(stats_file)
		if not stats.chunks[h] then
			stats.chunks[h] = { ease = stats.algorithm.ease_initial, ivl = 0,
			                    mastery = 0, total_duration = 0, max_groups = 0 }
			save_stats(stats_file, stats)
		end

	-- CHUNK: register chunk metadata in memory (no disk I/O)
	elseif line:match("^CHUNK ") then
		handle_chunk(line)
		if pending_suggest then
			pending_suggest = false
			local stats = load_stats(stats_file)
			handle_suggest(stats)
		end

	-- CHUNK_SESSION: the next LESSON is a chunk replay, not a real lesson
	elseif line:match("^CHUNK_SESSION ") then
		in_chunk_session = true

	-- LESSON: new lesson starting; finalise any abandoned in-progress lesson
	elseif line:match("^LESSON ") then
		if in_chunk_session then
			-- chunk replay: track notes for RESULT scoring only
			current = { id = line:match("^LESSON (%S+)"), max_bass_id = -1, results = {}, is_chunk = true }
			in_chunk_session = false
		else
			if current and current.max_bass_id >= 0 and not current.results[current.max_bass_id] then
				local stats = load_stats(stats_file)
				finalize(stats)
			end
			local n = line:match("^LESSON (%S+)")
			current = { id = n, max_bass_id = -1, results = {} }
		end

	-- BASSNOTE: track highest index to know when the lesson is complete
	elseif line:match("^BASSNOTE ") then
		if current then
			local bid = tonumber(line:match("^BASSNOTE (%d+):"))
			if bid and bid > current.max_bass_id then
				current.max_bass_id = bid
			end
		end

	-- RESULT: accumulate; finalise on last expected result
	elseif line:match("^RESULT ") then
		if current then
			local rid, rtime = line:match("^RESULT (%d+) TIME:(%d+)")
			if rid then
				local id_val = tonumber(rid)
				current.results[id_val] = {
					status = line:find("OK") and "OK" or "FAIL",
					time = tonumber(rtime),
				}
				if id_val == current.max_bass_id then
					local stats = load_stats(stats_file)
					finalize(stats)
				end
			end
		end

	-- GUI commands
	elseif line:match("^QUERY_STATS") then
		local stats = load_stats(stats_file)
		print_stats_line(stats)
	elseif line:match("^LOAD_LESSON") then
		local stats = load_stats(stats_file)
		print_stats_line(stats, line:match("LOAD_LESSON (%S+)"))
	elseif line:match("^SUGGEST_LESSON") then
		-- If no chunks registered yet (chunk.lua still running), defer until
		-- the first CHUNK line arrives.
		local has_chunks = next(lesson_chunks_mem) ~= nil
		if has_chunks then
			local stats = load_stats(stats_file)
			handle_suggest(stats)
		else
			pending_suggest = true
		end
	elseif line:match("^ALL_SCANNED") then
		local stats = load_stats(stats_file)
		local stale = {}
		for id in pairs(stats.lessons) do
			if tonumber(id) and not scanned_lessons[tostring(id)] then
				stale[#stale + 1] = "lesson:" .. id
			end
		end
		for h in pairs(stats.chunks) do
			if not scanned_chunks[h] then
				stale[#stale + 1] = "chunk:" .. h
			end
		end
		if #stale > 0 then
			table.sort(stale)
			io.stderr:write("Error: stale entries in stats (not present in current lesson set):\n")
			for _, s in ipairs(stale) do
				io.stderr:write("  " .. s .. "\n")
			end
			os.exit(1)
		end
	end
end

-- Handle a lesson that was still in progress at EOF
if current and current.max_bass_id >= 0 and not current.results[current.max_bass_id] then
	local stats = load_stats(stats_file)
	finalize(stats)
end
