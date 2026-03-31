-- SPDX-License-Identifier: MIT
-- stats.lua --- performance scoring, progress tracking and spaced repetition
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      Accumulates RESULT lines for the current session in memory.  When the
--      last RESULT arrives (or the session is abandoned), it computes accuracy,
--      score and timing, then updates Mastery, Power and SRS fields for the
--      chunk in stats.chunks.  Every session is a chunk session keyed by hash.
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
--      LESSON <hash> <key> <time> <bpm> <bar>   from lua src/all.lua
--          Starts a new session accumulator keyed by <hash>.
--      BASSNOTE <i>: <token> [passing]           from lua src/all.lua
--      RESULT <i> TIME:<ms> OK|FAIL [msg]        from lua src/rules.lua
--      CHUNK_NAME <hash> <level>                 from lua src/all.lua
--          Ensures stats.chunks[hash] exists with level field; saves if new.
--          Records hash as a known live chunk for ALL_SCANNED validation.
--      ALL_SCANNED                               from lua src/all.lua
--          Verifies every entry in stats.chunks was announced via CHUNK_NAME.
--          Any unannounced (stale) entry is reported to stderr and exits 1.
--      QUERY_STATS                               from bin/gui
--          Emits a STATS line for the current daily totals.
--      SUGGEST_LESSON                            from bin/gui
--          Selects and emits the best chunk to practice next.
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
--      SUGGESTION chunk=<hash> skills=<s> reason=<weak_chunk|new_chunk>
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
	power_points_per_pct = 1.0, -- points awarded per 1% normalised power gain
	bottleneck_thresh = 0.6, -- factor value below which it is considered a bottleneck
	dominance_margin = 0.2, -- how much lower the bottleneck must be than the others
	min_quality = 0.1, -- quality below this triggers a "raise quality" suggestion
	power_score_frac = 0.7, -- skip suggesting a chunk whose power is already >= this fraction of its mastery
	overlearn_min = 5, -- min consecutive attempts before suggesting another chunk (ema_pass = 1)
	overlearn_max = 15, -- max consecutive attempts before suggesting another chunk (ema_pass = 0)
	mistake_power_penalty = 0.15, -- power_factor multiplied by (1-penalty) per failed session
	mastery_decay_half_life = 90, -- days for mastery to halve without a mastery-improving session
	skill_order = "root 6 4-3_sus 6/4 7 7-6_sus", -- space-separated skill names; lower index = introduced earlier
	weak_ema_thresh = 0.8, -- ema_pass below this marks a lesson as "needs_work"
	ease_initial = 2.5, -- starting ease factor for new lessons
	ease_min = 1.3, -- minimum ease factor
	ease_max = 3.5, -- maximum ease factor
	ease_pass_delta = 0.1, -- ease increase on a perfect pass
	ease_fail_delta = 0.2, -- ease decrease on a fail
	ivl_first = 1, -- interval (days) after first perfect pass
	ivl_second = 6, -- interval (days) after second consecutive perfect pass
	ivl_max = 365, -- maximum interval (days); caps runaway SRS growth
	chunk_mastery_thresh = 80, -- normalised % below which a chunk needs practice
	chunk_power_thresh = 70, -- normalised % below which a chunk needs practice
	midnight_time = 0, -- hour (UTC) at which the practice day resets (0 = midnight)

	-- EMA tracking for graduation criteria
	ema_evenness_alpha = 0.18, -- same window as ema_bpm (~10 attempts)

	-- Practice badges (sequential: P -> S -> E)
	badge_power_thresh = 40, -- power % to earn P badge
	badge_speed_thresh = 60, -- ema_bpm to earn S badge (after P)
	badge_evenness_thresh = 0.80, -- ema_evenness to earn E badge (after S)

	-- Performance badges (sequential: P' -> S' -> E', higher bar)
	perf_badge_power_thresh = 60, -- power % to earn perf P badge
	perf_badge_speed_thresh = 120, -- ema perf BPM to earn perf S badge
	perf_badge_evenness_thresh = 0.85, -- ema perf evenness to earn perf E badge

	-- Badge bonus points
	badge_power_bonus = 25, -- points awarded on earning P / P'
	badge_speed_bonus = 25, -- points awarded on earning S / S'
	badge_evenness_bonus = 25, -- points awarded on earning E / E'
	badge_graduate_bonus = 50, -- extra points on completing practice set
	badge_mastered_bonus = 100, -- extra points on completing all 6 badges

	-- Performance-mode BPM schedule
	perf_bpm_discount = 0.75, -- initial perf BPM = ema_bpm * this
	perf_bpm_increment = 2, -- BPM added after each successful attempt
	perf_bpm_max = 160, -- absolute BPM ceiling

	-- Performance-mode fail-out thresholds
	perf_fail_accuracy = 70, -- timing accuracy % below which attempt fails
	perf_fail_timing_tol = 0.20, -- max |actual-expected|/expected per beat
	perf_fail_streak = 3, -- consecutive fails to suggest practice

	-- Interleaved practice (anti-blocked-practice)
	rotation_pool_size = 5, -- keep this many chunks in active rotation
	min_away = 2, -- min attempts on other chunks before returning
}

local stats_file = arg[1]

if not stats_file then
	io.write("Usage: lua stats.lua <stats_file.lua>\n")
	os.exit(1)
end

-------------------------------------------------------------------------------
-- UTILITIES & METRICS
-------------------------------------------------------------------------------

local function check_hash(h)
	if type(h) ~= "string" or #h ~= 40 or not h:match("^[0-9a-f]+$") then
		error(string.format("invalid SHA1 hash: %q", tostring(h)), 2)
	end
end

local function get_date_str(midnight_time)
	local t = os.time() - (midnight_time or 0) * 3600
	return os.date("%Y-%m-%d", t)
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
	return math.min(mastery, mastery * stability) * (l_data.power_factor or 1.0)
end

-- Like calculate_power but uses effective (max of direct and transitive) mastery
-- and last_date.  Used for scheduling decisions.
local function calculate_effective_power(l_data, alg)
	local mastery = math.max(l_data.mastery or 0, l_data.t_mastery or 0)
	local ivl = l_data.ivl or 0
	local eff_last = l_data.last_date
	local t = l_data.t_last_date
	if t and (not eff_last or t > eff_last) then
		eff_last = t
	end
	if not eff_last or ivl <= 0 then
		return 0
	end
	local y, m, d = eff_last:match("(%d+)-(%d+)-(%d+)")
	local last_ts = os.time({ year = y, month = m, day = d, hour = 12 })
	local days_elapsed = math.floor(os.difftime(os.time(), last_ts) / 86400)
	local stability = math.exp(-alg.power_half_life * (math.max(0, days_elapsed) / ivl))
	return math.min(mastery, mastery * stability) * (l_data.power_factor or 1.0)
end

local function calculate_streak(data)
	local streak = 0
	local mt = data.algorithm.midnight_time or 0
	local current_ts = os.time() - mt * 3600
	local today = get_date_str(mt)

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
	local today = get_date_str(alg.midnight_time)

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

	-- Update EMA evenness (used for badge progression)
	entry.ema_evenness = alg.ema_evenness_alpha * evenness_factor
		+ (1 - alg.ema_evenness_alpha) * (entry.ema_evenness or evenness_factor)

	local quality = entry.ema_pass * speed_factor * evenness_factor

	-- Mastery
	if sd.accuracy >= alg.pass_accuracy and sd.score > old_mastery then
		entry.mastery = old_mastery + (sd.score - old_mastery) * alg.mastery_growth * quality
		entry.last_updated_mastery = today
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

-- Transitive update: only touches t_mastery, t_last_date, and max_groups.
-- Called when this chunk's material is covered as part of a parent session.
-- Independent SRS fields (ease, ivl, n_pass, n_fail, ema_pass, best_avg) are
-- left untouched.
local function update_entry_transitive(entry, sd, alg)
	local today = get_date_str(alg.midnight_time)
	if sd.groups > (entry.max_groups or 0) then
		entry.max_groups = sd.groups
	end
	local old_t = entry.t_mastery or 0
	if sd.accuracy >= alg.pass_accuracy and sd.score > old_t then
		entry.t_mastery = old_t + (sd.score - old_t) * alg.mastery_growth
		entry.last_updated_mastery = today
	end
	entry.t_last_date = today
end

-------------------------------------------------------------------------------
-- STARTUP MASTERY DECAY
-------------------------------------------------------------------------------

local function apply_mastery_decay(stats)
	local alg = stats.algorithm
	local today = get_date_str(alg.midnight_time)
	local half_life = alg.mastery_decay_half_life
	local changed = false
	for _, c in pairs(stats.chunks) do
		local lum = c.last_updated_mastery
		if lum and half_life > 0 then
			local y, mo, d = lum:match("(%d+)-(%d+)-(%d+)")
			local last_ts = os.time({ year = y, month = mo, day = d, hour = 12 })
			local days = math.max(0, math.floor(os.difftime(os.time(), last_ts) / 86400))
			if days > 0 then
				local factor = math.exp(-0.693 * days / half_life)
				if (c.mastery or 0) > 0 then
					c.mastery = (c.mastery or 0) * factor
					changed = true
				end
				if (c.t_mastery or 0) > 0 then
					c.t_mastery = (c.t_mastery or 0) * factor
					changed = true
				end
			end
		end
		-- Always refresh the date so the next startup measures from today.
		if lum then
			c.last_updated_mastery = today
			changed = true
		end
	end
	if changed then
		save_stats(stats_file, stats)
	end
end

-------------------------------------------------------------------------------
-- OUTPUT HELPER
-------------------------------------------------------------------------------

local function print_stats_line(stats, timestamp, chunk_hash)
	local alg = stats.algorithm
	local today = get_date_str(alg.midnight_time)
	local d = stats.daily[today] or { score = 0, duration = 0 }
	local streak = calculate_streak(stats)
	local chunk_str = ""
	if chunk_hash then
		local c = stats.chunks[chunk_hash]
		if c then
			local power = calculate_power(c, alg)
			local mastery = c.mastery or 0
			chunk_str = string.format(
				" chunk=%s[ivl=%d,ease=%.2f,mastery=%.2f,power=%.2f]",
				chunk_hash,
				math.floor(c.ivl or 0),
				c.ease or alg.ease_initial,
				normalize(mastery, c),
				normalize(power, c)
			)
		end
	end
	io.write(
		string.format(
			"STATS time=%.0f total_today=%.2f goal=%.2f total_duration_today=%.3f streak=%d mastery_thresh=%d power_thresh=%d%s\n",
			timestamp or os.time(),
			d.score,
			alg.score_goal,
			d.duration,
			streak,
			alg.chunk_mastery_thresh,
			alg.chunk_power_thresh,
			chunk_str
		)
	)
end

-- Forward declarations: populated as lines arrive, consumed by handle_suggest/finalize.
local chunk_skills = {} -- { [hash] = skills_str }  populated from CHUNK_NAME line (level-1 only)
local scanned_chunks = {} -- chunks announced via CHUNK_NAME this session
local current = nil -- { id, max_bass_id, results={[i]={status,time}} }
local current_loaded = nil -- hash most recently loaded via LOAD_CHUNK
local current_mode = "practice" -- "practice" or "performance"; set by MODE command
local rotation_pool = {} -- ordered list of chunk hashes in active rotation
local attempts_since = {} -- { [hash] = count } attempts on other chunks since last played
local beat_timestamps = {} -- { [beat_number] = timestamp_ms } from karaoke BEAT messages
local perf_aborted = false -- true if current performance attempt was aborted
local pending_children = {} -- [hash] = {results, abs_s, abs_e}; awaiting CHILDREN response
local children_of = {} -- [hash] = {child_hash, ...} built from CHILDREN responses
local pending_child_queries = 0 -- outstanding startup QUERY_CHILDREN from ALL_SCANNED
local all_scanned_received = false

-- Emit one SKILL_STATS line per skill in skill_order showing aggregate mastery.
-- For each skill, averages the normalised max(mastery, t_mastery) across all
-- chunks that contain that skill.  Result is in [0, 100].
local function emit_skill_stats(stats)
	local alg = stats.algorithm
	local order = alg.skill_order or ""
	local skills = {}
	for sk in order:gmatch("%S+") do
		skills[#skills + 1] = sk
	end
	if #skills == 0 then return end

	local sum   = {} -- skill -> sum of normalised mastery
	local count = {} -- skill -> number of chunks contributing
	for _, sk in ipairs(skills) do
		sum[sk]   = 0
		count[sk] = 0
	end

	for hash, c in pairs(stats.chunks) do
		local sk_str = chunk_skills[hash] or (c.skills or "")
		if sk_str ~= "" then
			local m = normalize(math.max(c.mastery or 0, c.t_mastery or 0), c)
			for _, sk in ipairs(skills) do
				if (" " .. sk_str .. " "):find(" " .. sk .. " ", 1, true) then
					sum[sk]   = sum[sk]   + m
					count[sk] = count[sk] + 1
				end
			end
		end
	end

	for _, sk in ipairs(skills) do
		local m = count[sk] > 0 and (sum[sk] / count[sk]) or 0
		io.write(string.format("SKILL_STATS skill=%s mastery=%.2f\n", sk, m))
	end
end

-------------------------------------------------------------------------------
-- COMMAND HANDLERS
-------------------------------------------------------------------------------

-- Returns the skill rank of a chunk: the highest index of any of its skills in
-- the skill_order string.  Chunks with no skills (level-0) get rank -1 so they
-- are suggested last within the same level group.  Unknown skills get rank
-- equal to the length of the ordering (appended at validation time).
local function compute_skill_rank(hash, skill_order_str)
	local skills = chunk_skills[hash]
	if not skills or skills == "" then
		return -1
	end
	local order = {}
	local n = 0
	for sk in skill_order_str:gmatch("%S+") do
		order[sk] = n
		n = n + 1
	end
	local rank = -1
	for sk in skills:gmatch("%S+") do
		local r = order[sk]
		if r == nil then
			r = n
		end -- unknown: sort last
		if r > rank then
			rank = r
		end
	end
	return rank
end

-- Returns the hash of the best chunk to suggest, or nil. No output.
local function suggest_best_hash(stats)
	local alg = stats.algorithm
	local best_unmastered = nil
	local best_new = nil
	local all_practiced_mastered = true

	for h, c in pairs(stats.chunks) do
		local is_new = not c.last_date and not c.t_last_date
		local m_pct = normalize(math.max(c.mastery or 0, c.t_mastery or 0), c)
		local level = c.level or 0

		if is_new then
			local sr = compute_skill_rank(h, alg.skill_order)
			if
				not best_new
				or level < best_new.level
				or (level == best_new.level and sr < best_new.skill_rank)
				or (level == best_new.level and sr == best_new.skill_rank and h < best_new.hash)
			then
				best_new = { hash = h, level = level, skill_rank = sr }
			end
		else
			if m_pct < alg.chunk_mastery_thresh then
				all_practiced_mastered = false
				local p = calculate_effective_power(c, alg)
				if
					not best_unmastered
					or level > best_unmastered.level
					or (level == best_unmastered.level and p < best_unmastered.p)
					or (level == best_unmastered.level and p == best_unmastered.p and h < best_unmastered.hash)
				then
					best_unmastered = { hash = h, level = level, p = p }
				end
			end
		end
	end

	if not all_practiced_mastered then
		return best_unmastered and best_unmastered.hash or nil
	elseif best_new then
		return best_new.hash
	end
	return nil
end

local function handle_suggest(stats)
	local best_hash = suggest_best_hash(stats)
	if best_hash then
		local c = stats.chunks[best_hash]
		local is_new = not c.last_date and not c.t_last_date
		io.write(
			string.format(
				"SUGGESTION chunk=%s level=%d skills=%s reason=%s\n",
				best_hash,
				c.level or 0,
				chunk_skills[best_hash] or "?",
				is_new and "new_chunk" or "weak_chunk"
			)
		)
		return
	end
	io.write("SUGGESTION none reason=all_up_to_date\n")
end

-------------------------------------------------------------------------------
-- BADGE PROGRESSION
-------------------------------------------------------------------------------

-- Check and award badges for a chunk after an attempt.
-- mode: "practice" or "performance"
-- Returns total bonus points earned this check.
local function check_badges(c, alg, mode)
	local bonus = 0

	if mode == "practice" then
		local power_pct = normalize(calculate_power(c, alg), c)
		-- P badge: power threshold
		if not c.badge_p and power_pct >= alg.badge_power_thresh then
			c.badge_p = true
			bonus = bonus + alg.badge_power_bonus
			io.write(string.format("BADGE P %d\n", alg.badge_power_bonus))
		end
		-- S badge: ema_bpm threshold (requires P)
		if c.badge_p and not c.badge_s and (c.ema_bpm or 0) >= alg.badge_speed_thresh then
			c.badge_s = true
			bonus = bonus + alg.badge_speed_bonus
			io.write(string.format("BADGE S %d\n", alg.badge_speed_bonus))
		end
		-- E badge: ema_evenness threshold (requires S)
		if c.badge_s and not c.badge_e and (c.ema_evenness or 0) >= alg.badge_evenness_thresh then
			c.badge_e = true
			bonus = bonus + alg.badge_evenness_bonus
			io.write(string.format("BADGE E %d\n", alg.badge_evenness_bonus))
		end
		-- All three practice badges: graduate bonus
		if c.badge_p and c.badge_s and c.badge_e and not c.badge_graduated then
			c.badge_graduated = true
			bonus = bonus + alg.badge_graduate_bonus
			io.write(string.format("BADGE READY %d\n", alg.badge_graduate_bonus))
			io.write("MODE_SUGGEST performance\n")
		end
	elseif mode == "performance" then
		local power_pct = normalize(calculate_power(c, alg), c)
		-- P' badge: perf power threshold
		if not c.perf_badge_p and power_pct >= alg.perf_badge_power_thresh then
			c.perf_badge_p = true
			bonus = bonus + alg.badge_power_bonus
			io.write(string.format("BADGE PP %d\n", alg.badge_power_bonus))
		end
		-- S' badge: perf_ema_bpm threshold (requires P')
		if c.perf_badge_p and not c.perf_badge_s and (c.perf_ema_bpm or 0) >= alg.perf_badge_speed_thresh then
			c.perf_badge_s = true
			bonus = bonus + alg.badge_speed_bonus
			io.write(string.format("BADGE PS %d\n", alg.badge_speed_bonus))
		end
		-- E' badge: perf_ema_evenness threshold (requires S')
		if c.perf_badge_s and not c.perf_badge_e and (c.perf_ema_evenness or 0) >= alg.perf_badge_evenness_thresh then
			c.perf_badge_e = true
			bonus = bonus + alg.badge_evenness_bonus
			io.write(string.format("BADGE PE %d\n", alg.badge_evenness_bonus))
		end
		-- All six badges: mastered bonus
		if c.badge_p and c.badge_s and c.badge_e
			and c.perf_badge_p and c.perf_badge_s and c.perf_badge_e
			and not c.badge_mastered then
			c.badge_mastered = true
			bonus = bonus + alg.badge_mastered_bonus
			io.write(string.format("BADGE MASTERED %d\n", alg.badge_mastered_bonus))
		end
	end

	return bonus
end

-- Compute badge progress for the next unearned badge.
-- Returns tag, current, target (or nil if all earned).
local function badge_progress(c, alg, mode)
	if mode == "practice" then
		if not c.badge_p then
			local power_pct = normalize(calculate_power(c, alg), c)
			return "P", power_pct, alg.badge_power_thresh
		end
		if not c.badge_s then
			return "S", c.ema_bpm or 0, alg.badge_speed_thresh
		end
		if not c.badge_e then
			return "E", c.ema_evenness or 0, alg.badge_evenness_thresh
		end
	elseif mode == "performance" then
		if not c.perf_badge_p then
			local power_pct = normalize(calculate_power(c, alg), c)
			return "PP", power_pct, alg.perf_badge_power_thresh
		end
		if not c.perf_badge_s then
			return "PS", c.perf_ema_bpm or 0, alg.perf_badge_speed_thresh
		end
		if not c.perf_badge_e then
			return "PE", c.perf_ema_evenness or 0, alg.perf_badge_evenness_thresh
		end
	end
	return nil
end

-------------------------------------------------------------------------------
-- LESSON FINALISATION
-------------------------------------------------------------------------------

-- Update per-note EMA for notes in results[from..to], keyed 0-relative.
local function update_note_emas(c, results, from, to, alg)
	if not c.notes then c.notes = {} end
	for i = from, to do
		local r = results[i]
		if r then
			local key = tostring(i - from)
			local note = c.notes[key] or {}
			local pass = r.status ~= "FAIL" and 1.0 or 0.0
			note.ema = alg.ema_alpha * pass + (1.0 - alg.ema_alpha) * (note.ema or pass)
			c.notes[key] = note
		end
	end
end

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
	local today = get_date_str(alg.midnight_time)
	local hash = tostring(current.id)

	local ref_bpm = current.ref_bpm or 120.0
	local time_denom = current.time_denom or 4

	local c = stats.chunks[hash]
		or { ease = alg.ease_initial, ivl = 0, mastery = 0, total_duration = 0, max_groups = 0 }
	if alg.last_lesson_scored ~= hash then
		c.n_consecutive = 1
	else
		c.n_consecutive = (c.n_consecutive or 0) + 1
	end
	alg.last_lesson_scored = hash
	local res = update_entry(c, sd, alg)
	-- Collect failed group IDs and apply power_factor penalty.
	local failed_groups = {}
	for i, r in pairs(current.results) do
		if r.status == "FAIL" then
			failed_groups[i] = true
		end
	end
	if sd.accuracy == 100 then
		c.power_factor = 1.0
	else
		c.power_factor = math.max(0, (c.power_factor or 1.0) * (1.0 - alg.mistake_power_penalty))
	end
	update_note_emas(c, current.results, 0, current.max_bass_id, alg)
	-- Update EMA BPM from actual play duration this session.
	-- Sum beats only for transitions that were actually timed (both adjacent
	-- results present), matching what compute_score_data counted in sd.duration.
	local bpm_beats = 0
	for i = 1, current.max_bass_id do
		if current.results[i] and current.results[i - 1] then
			bpm_beats = bpm_beats + (current.beats[i - 1] or 0)
		end
	end
	if sd.duration > 0 and bpm_beats > 0 then
		local actual_bpm = bpm_beats * (time_denom / 4.0) * 60.0 / sd.duration
		c.ema_bpm = alg.ema_alpha * actual_bpm + (1.0 - alg.ema_alpha) * (c.ema_bpm or ref_bpm)
	end
	stats.chunks[hash] = c
	local points = normalize(res.m_delta, c) * alg.mastery_points_per_pct
		+ normalize(res.p_delta, c) * alg.power_points_per_pct
	-- Badge progression (practice mode only here; performance mode uses perf scoring)
	local badge_pts = 0
	if current_mode == "practice" then
		badge_pts = check_badges(c, alg, "practice")
		points = points + badge_pts
	end
	stats.daily[today] = stats.daily[today] or { score = 0, duration = 0 }
	stats.daily[today].score = stats.daily[today].score + points
	stats.daily[today].duration = stats.daily[today].duration + sd.duration
	save_stats(stats_file, stats)
	local power = calculate_power(c, alg)
	local d = stats.daily[today]
	local streak = calculate_streak(stats)
	-- Interleaved practice: track attempts on other chunks and rotation pool
	for h, _ in pairs(attempts_since) do
		if h ~= hash then
			attempts_since[h] = (attempts_since[h] or 0) + 1
		end
	end
	attempts_since[hash] = 0

	local ema = c.ema_pass or 1.0
	local required = alg.overlearn_min + (alg.overlearn_max - alg.overlearn_min) * (1.0 - ema)
	local suggestion = nil
	if c.n_consecutive >= required then
		-- Pick from rotation pool: find a chunk with enough min_away distance
		local interleave_hash = nil
		for _, rh in ipairs(rotation_pool) do
			if rh ~= hash and (attempts_since[rh] or alg.min_away) >= alg.min_away then
				interleave_hash = rh
				break
			end
		end
		-- Refill pool from suggest_best_hash if needed
		if not interleave_hash then
			local best = suggest_best_hash(stats)
			if best and best ~= hash then
				interleave_hash = best
				-- Add to rotation pool if not already there
				local in_pool = false
				for _, rh in ipairs(rotation_pool) do
					if rh == best then in_pool = true; break end
				end
				if not in_pool then
					rotation_pool[#rotation_pool + 1] = best
					-- Trim pool to rotation_pool_size
					while #rotation_pool > alg.rotation_pool_size do
						table.remove(rotation_pool, 1)
					end
				end
			end
		end
		if interleave_hash then
			suggestion = "try_another_lesson"
			io.write(string.format("INTERLEAVE %s\n", interleave_hash))
		end
	end
	io.write(
		string.format(
			"STATS time=%.0f total_today=%.2f goal=%.2f total_duration_today=%.3f streak=%d"
				.. " mastery_thresh=%d power_thresh=%d"
				.. " chunk=%s[ivl=%d,ease=%.2f,mastery=%.2f,power=%.2f]%s\n",
			sd.time,
			d.score,
			alg.score_goal,
			d.duration,
			streak,
			alg.chunk_mastery_thresh,
			alg.chunk_power_thresh,
			hash,
			math.floor(c.ivl or 0),
			c.ease or alg.ease_initial,
			normalize(c.mastery or 0, c),
			normalize(power, c),
			suggestion and (" suggestion=" .. suggestion) or ""
		)
	)
	emit_skill_stats(stats)
	local saved_results = current.results
	local saved_max_bass_id = current.max_bass_id
	current = nil
	pending_children[hash] =
		{ results = saved_results, abs_s = 0, abs_e = saved_max_bass_id, failed_groups = failed_groups }
	io.write("QUERY_CHILDREN " .. hash .. "\n")
end

-------------------------------------------------------------------------------
-- MAIN LOOP
-------------------------------------------------------------------------------

local pending_suggest = false -- defer SUGGEST_LESSON until children_of map is ready

local init_stats = load_stats(stats_file)
apply_mastery_decay(init_stats)

-- Fast startup: pre-populate from stats file; no scan or QUERY_CHILDREN needed.
for h, c in pairs(init_stats.chunks) do
	if c.skills and c.skills ~= "" then
		chunk_skills[h] = c.skills
	end
	if c.children then
		children_of[h] = c.children
	end
end
all_scanned_received = true

for line in io.lines() do
	line = line:gsub("\r", "")

	-- CHUNK_NAME: emitted by all.lua; initialise entry if absent; persist skills
	if line:match("^CHUNK_NAME ") then
		local h, lv = line:match("^CHUNK_NAME (%S+) (%d+)")
		if not h then
			goto continue
		end
		check_hash(h)
		local level = tonumber(lv) or 0
		scanned_chunks[h] = true
		local skills_str = line:match("^CHUNK_NAME %S+ %d+ (.+)$")
		if skills_str and not chunk_skills[h] then
			chunk_skills[h] = skills_str
		end
		local stats = load_stats(stats_file)
		local needs_save = false
		if not stats.chunks[h] then
			stats.chunks[h] = {
				level = level,
				ease = stats.algorithm.ease_initial,
				ivl = 0,
				mastery = 0,
				total_duration = 0,
				max_groups = 0,
			}
			needs_save = true
		elseif stats.chunks[h].level == nil then
			stats.chunks[h].level = level
			needs_save = true
		end
		-- Persist skills so startup can read them without a rescan.
		if skills_str and stats.chunks[h].skills ~= skills_str then
			stats.chunks[h].skills = skills_str
			needs_save = true
		end
		if needs_save then
			save_stats(stats_file, stats)
		end
	-- LOAD_CHUNK: track which chunk is loaded; emit its current stats immediately
	elseif line:match("^LOAD_CHUNK ") then
		local h = line:match("^LOAD_CHUNK (%S+)")
		if h then
			current_loaded = h
			local stats = load_stats(stats_file)
			print_stats_line(stats, nil, current_loaded)
			emit_skill_stats(stats)
			-- Emit persisted badge state so GUI can reconstruct on chunk load
			local c = stats.chunks[h]
			if c then
				if c.badge_p then io.write("BADGE P 0\n") end
				if c.badge_s then io.write("BADGE S 0\n") end
				if c.badge_e then io.write("BADGE E 0\n") end
				if c.badge_graduated then io.write("BADGE READY 0\n") end
				if c.perf_badge_p then io.write("BADGE PP 0\n") end
				if c.perf_badge_s then io.write("BADGE PS 0\n") end
				if c.perf_badge_e then io.write("BADGE PE 0\n") end
				if c.badge_mastered then io.write("BADGE MASTERED 0\n") end
				-- Emit perf BPM if available
				if c.perf_bpm then
					io.write(string.format("PERF_BPM %.2f\n", c.perf_bpm))
				end
			end
		end

	-- LESSON: new session starting; finalise any abandoned in-progress session
	elseif line:match("^LESSON ") then
		local chunk_id = line:match("^LESSON (%S+)")
		check_hash(chunk_id)
		if current and current.max_bass_id >= 0 and not current.results[current.max_bass_id] then
			finalize(load_stats(stats_file))
		end
		-- LESSON <hash> <key> <time> <bpm> <bar>
		-- File BPM is already in denominator-beat units (e.g. 120 in 3/2 = 120 half notes/min).
		local ref_bpm = tonumber(line:match("^LESSON %S+ %S+ %S+ (%S+)")) or 120.0
		local time_str = line:match("^LESSON %S+ %S+ (%S+)")
		local time_denom = tonumber(time_str and time_str:match("/(%d+)")) or 4
		current = { id = chunk_id, max_bass_id = -1, results = {}, ref_bpm = ref_bpm, time_denom = time_denom, beats = {} }
		-- Issue BPM for karaoke: use EMA if available, otherwise the reference BPM
		local bpm_stats = load_stats(stats_file)
		local bpm_chunk = bpm_stats.chunks[chunk_id]
		local bpm_out = (bpm_chunk and bpm_chunk.ema_bpm) or ref_bpm
		io.write(string.format("BPM %.2f\n", bpm_out))

	-- BASSNOTE: track highest index and accumulate beats for BPM tracking
	elseif line:match("^BASSNOTE ") then
		if current then
			local bid = tonumber(line:match("^BASSNOTE (%d+):"))
			if bid and bid > current.max_bass_id then
				current.max_bass_id = bid
			end
			-- Store note duration in quarter-note beats, indexed by bass-note ID.
			-- Token format: <note>[accidentals][octave]<dur>[dots], e.g. g2, fis2., b,4
			local tok = line:match("^BASSNOTE %d+: (%S+)")
			if tok then
				tok = tok:gsub("p$", "") -- strip passing-chord marker
				local dur_str, dots_str = tok:match("(%d+)(%.*)$")
				if dur_str then
					local dur = tonumber(dur_str) or 4
					local beats = 4.0 / dur
					for _ in dots_str:gmatch("%.") do
						beats = beats * 1.5
					end
					current.beats[bid] = beats
				end
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

	-- CHILDREN: response to QUERY_CHILDREN; handles both startup map-building
	--           and downward finalize after a completed parent session.
	elseif line:match("^CHILDREN ") then
		local phash = line:match("^CHILDREN (%S+)")
		if not phash then
			goto continue
		end

		-- Parse child descriptors from the line.
		local suffix = line:sub(#"CHILDREN " + #phash + 1)
		local child_list = {}
		for child_entry in suffix:gmatch("%S+") do
			local ch, rs, re = child_entry:match("^(%x+):(%d+):(%d+)$")
			if ch then
				child_list[#child_list + 1] = { hash = ch, s = tonumber(rs), e = tonumber(re) }
			end
		end

		-- Always update the children_of map.
		children_of[phash] = {}
		for _, ci in ipairs(child_list) do
			children_of[phash][#children_of[phash] + 1] = ci.hash
		end

		local pending = pending_children[phash]
		pending_children[phash] = nil

		if not pending then
			-- Proactive CHILDREN from all.lua during scan: persist child hashes
			-- so startup can rebuild children_of without any QUERY_CHILDREN.
			if #child_list > 0 then
				local stats = load_stats(stats_file)
				if stats.chunks[phash] then
					local arr = {}
					for _, ci in ipairs(child_list) do
						arr[#arr + 1] = ci.hash
					end
					stats.chunks[phash].children = arr
					save_stats(stats_file, stats)
				end
			end
			if pending_child_queries > 0 then
				pending_child_queries = pending_child_queries - 1
			end
			if pending_child_queries == 0 and pending_suggest then
				pending_suggest = false
				handle_suggest(load_stats(stats_file))
			end
			goto continue
		end

		-- Finalize query: transitively update each child's stats entry.
		local stats = load_stats(stats_file)
		local failed_groups = pending.failed_groups or {}
		for _, ci in ipairs(child_list) do
			local abs_s = pending.abs_s + ci.s
			local abs_e = pending.abs_s + ci.e
			local sd = compute_score_data(pending.results, abs_s, abs_e)
			local alg = stats.algorithm
			local c = stats.chunks[ci.hash]
				or { ease = alg.ease_initial, ivl = 0, mastery = 0, total_duration = 0, max_groups = 0 }
			if sd then
				update_entry_transitive(c, sd, alg)
			end
			-- Apply power_factor: penalise if any failed group falls in this child's range,
			-- reset to 1.0 if the sub-range was clean.
			local child_has_fail = false
			for gi in pairs(failed_groups) do
				if gi >= abs_s and gi <= abs_e then
					child_has_fail = true
					break
				end
			end
			if child_has_fail then
				c.power_factor = math.max(0, (c.power_factor or 1.0) * (1.0 - alg.mistake_power_penalty))
			else
				c.power_factor = 1.0
			end
			-- Update SRS scheduling from the parent session result so that the
			-- scheduling algorithm works uniformly across all levels: a clean
			-- sub-range advances the review interval (child won't surface soon),
			-- a failed sub-range resets it to 1 day (child surfaces next session).
			if sd then
				if not child_has_fail and sd.accuracy == 100 then
					if (c.ivl or 0) == 0 then
						c.ivl = alg.ivl_first
					else
						c.ivl = math.min(alg.ivl_max, math.ceil(c.ivl * (c.ease or alg.ease_initial)))
					end
					c.ease = math.min(alg.ease_max, (c.ease or alg.ease_initial) + alg.ease_pass_delta)
				elseif child_has_fail then
					c.ivl = 1
					c.ease = math.max(alg.ease_min, (c.ease or alg.ease_initial) - alg.ease_fail_delta)
				end
			end
			update_note_emas(c, pending.results, abs_s, abs_e, alg)
			stats.chunks[ci.hash] = c
			pending_children[ci.hash] =
				{ results = pending.results, abs_s = abs_s, abs_e = abs_e, failed_groups = failed_groups }
			io.write("QUERY_CHILDREN " .. ci.hash .. "\n")
		end
		save_stats(stats_file, stats)

	-- MODE: user declares current mode (from GUI Tab key)
	elseif line:match("^MODE ") then
		local m = line:match("^MODE (%S+)")
		if m == "practice" or m == "performance" then
			current_mode = m
			-- On entering performance mode, emit suggested BPM
			if m == "performance" and current_loaded then
				local stats = load_stats(stats_file)
				local c = stats.chunks[current_loaded]
				if c then
					local alg = stats.algorithm
					local bpm = c.perf_bpm
					if not bpm then
						-- First entry: discount from ema_bpm
						bpm = (c.ema_bpm or 60) * alg.perf_bpm_discount
						bpm = math.max(alg.badge_speed_thresh, math.min(alg.perf_bpm_max, bpm))
						c.perf_bpm = bpm
						stats.chunks[current_loaded] = c
						save_stats(stats_file, stats)
					end
					io.write(string.format("PERF_BPM %.2f\n", bpm))
				end
			end
		end

	-- BEAT: karaoke beat timestamp (from karaoke.rs in performance mode)
	elseif line:match("^BEAT ") then
		local n, ts = line:match("^BEAT (%d+) (%d+)")
		if n and ts then
			beat_timestamps[tonumber(n)] = tonumber(ts)
		end

	-- KARAOKE_ABORT: user stopped karaoke mid-attempt
	elseif line:match("^KARAOKE_ABORT") then
		perf_aborted = true
		beat_timestamps = {}

	-- KARAOKE_DONE: performance attempt completed; score it
	elseif line:match("^KARAOKE_DONE") then
		if current_mode == "performance" and current_loaded and not perf_aborted then
			local stats = load_stats(stats_file)
			local c = stats.chunks[current_loaded]
			local alg = stats.algorithm
			if c then
				-- Score performance attempt using beat timestamps
				local total_beats = 0
				local passed_beats = 0
				local sorted_beats = {}
				for n in pairs(beat_timestamps) do
					sorted_beats[#sorted_beats + 1] = n
				end
				table.sort(sorted_beats)

				if #sorted_beats >= 2 then
					local actual_bpm = c.perf_bpm or 60
					local expected_interval = 60000.0 / actual_bpm -- ms per beat
					local timing_errors = {}
					for i = 2, #sorted_beats do
						local actual_interval = beat_timestamps[sorted_beats[i]] - beat_timestamps[sorted_beats[i - 1]]
						local err = math.abs(actual_interval - expected_interval) / expected_interval
						timing_errors[#timing_errors + 1] = err
						total_beats = total_beats + 1
						if err <= alg.perf_fail_timing_tol then
							passed_beats = passed_beats + 1
						end
					end

					local accuracy = (total_beats > 0) and (passed_beats / total_beats * 100) or 0
					local passed = accuracy >= alg.perf_fail_accuracy

					-- Timing evenness: min/max interval ratio
					local min_ivl, max_ivl = math.huge, 0
					for i = 2, #sorted_beats do
						local ivl = beat_timestamps[sorted_beats[i]] - beat_timestamps[sorted_beats[i - 1]]
						if ivl < min_ivl then min_ivl = ivl end
						if ivl > max_ivl then max_ivl = ivl end
					end
					local timing_evenness = (max_ivl > 0) and (min_ivl / max_ivl) or 1.0

					if passed then
						c.perf_pass_streak = (c.perf_pass_streak or 0) + 1
						c.perf_fail_streak = 0
						c.perf_best_bpm = math.max(c.perf_best_bpm or 0, actual_bpm)
						c.perf_ema_bpm = alg.ema_evenness_alpha * actual_bpm
							+ (1 - alg.ema_evenness_alpha) * (c.perf_ema_bpm or actual_bpm)
						c.perf_ema_evenness = alg.ema_evenness_alpha * timing_evenness
							+ (1 - alg.ema_evenness_alpha) * (c.perf_ema_evenness or timing_evenness)
						c.perf_bpm = math.min(actual_bpm + alg.perf_bpm_increment, alg.perf_bpm_max)
						-- Mastery bonus multiplier for performance passes
						local perf_points = (normalize(c.mastery or 0, c) * alg.power_points_per_pct) * 0.25
						local today = get_date_str(alg.midnight_time)
						stats.daily[today] = stats.daily[today] or { score = 0, duration = 0 }
						stats.daily[today].score = stats.daily[today].score + perf_points
						-- Badge check for performance mode
						local badge_pts = check_badges(c, alg, "performance")
						stats.daily[today].score = stats.daily[today].score + badge_pts
						io.write(string.format("PERF_STATUS pass %.1f\n", accuracy))
					else
						c.perf_fail_streak = (c.perf_fail_streak or 0) + 1
						c.perf_pass_streak = 0
						c.perf_bpm = math.max(actual_bpm - alg.perf_bpm_increment, alg.badge_speed_thresh)
						c.power_factor = math.max(0, (c.power_factor or 1.0) * (1.0 - alg.mistake_power_penalty))
						io.write(string.format("PERF_STATUS fail %.1f\n", accuracy))
						-- Suggest practice if too many consecutive fails
						if (c.perf_fail_streak or 0) >= alg.perf_fail_streak then
							io.write("MODE_SUGGEST practice\n")
						end
					end
					io.write(string.format("PERF_BPM %.2f\n", c.perf_bpm))
				else
					io.write("PERF_STATUS fail 0.0\n")
				end
				stats.chunks[current_loaded] = c
				save_stats(stats_file, stats)
				print_stats_line(stats, nil, current_loaded)
			end
		end
		beat_timestamps = {}
		perf_aborted = false

	-- QUERY_ALG: emit all algorithm parameters as key=value pairs
	elseif line:match("^QUERY_ALG") then
		local stats = load_stats(stats_file)
		local parts = {}
		local keys = {}
		for k in pairs(stats.algorithm) do keys[#keys + 1] = k end
		table.sort(keys)
		for _, k in ipairs(keys) do
			local v = stats.algorithm[k]
			if type(v) == "number" then
				parts[#parts + 1] = string.format("%s=%.6g", k, v)
			elseif type(v) == "string" then
				parts[#parts + 1] = string.format("%s=%s", k, v)
			end
		end
		io.write("ALG_PARAMS " .. table.concat(parts, "\t") .. "\n")

	-- SET_ALG <key> <value>: update a single algorithm parameter
	elseif line:match("^SET_ALG ") then
		local k, v_str = line:match("^SET_ALG (%S+) (.+)$")
		if k and v_str then
			local stats = load_stats(stats_file)
			local old = stats.algorithm[k]
			if old ~= nil then
				if type(old) == "number" then
					local n = tonumber(v_str)
					if n then
						stats.algorithm[k] = n
						save_stats(stats_file, stats)
					end
				elseif type(old) == "string" then
					stats.algorithm[k] = v_str
					save_stats(stats_file, stats)
				end
			end
		end

	-- GUI commands
	elseif line:match("^QUERY_STATS") then
		local stats = load_stats(stats_file)
		print_stats_line(stats, nil, current_loaded)
		emit_skill_stats(stats)
	elseif line:match("^SUGGEST_LESSON") then
		-- Defer until ALL_SCANNED received and children_of map is fully built.
		if all_scanned_received and pending_child_queries == 0 then
			local stats = load_stats(stats_file)
			handle_suggest(stats)
		else
			pending_suggest = true
		end
	elseif line:match("^ALL_SCANNED") then
		local stats = load_stats(stats_file)
		local stale = {}
		for h in pairs(stats.chunks) do
			if not scanned_chunks[h] then
				stale[#stale + 1] = h
			end
		end
		if #stale > 0 then
			table.sort(stale)
			io.stderr:write("Warning: stale chunks in stats (not in current lesson set):\n")
			for _, h in ipairs(stale) do
				io.stderr:write("  chunk:" .. h .. "\n")
				stats.chunks[h] = nil
				chunk_skills[h] = nil
				children_of[h] = nil
			end
			save_stats(stats_file, stats)
		end
		-- Validate skill_order: every skill used by any chunk must appear in it.
		local order_set = {}
		for sk in stats.algorithm.skill_order:gmatch("%S+") do
			order_set[sk] = true
		end
		local missing = {}
		for _, sk_str in pairs(chunk_skills) do
			for sk in sk_str:gmatch("%S+") do
				if not order_set[sk] then
					order_set[sk] = true -- avoid duplicates
					missing[#missing + 1] = sk
				end
			end
		end
		if #missing > 0 then
			table.sort(missing)
			io.stderr:write("Warning: skills not in skill_order, appending: " .. table.concat(missing, " ") .. "\n")
			local sep = stats.algorithm.skill_order == "" and "" or " "
			stats.algorithm.skill_order = stats.algorithm.skill_order .. sep .. table.concat(missing, " ")
			save_stats(stats_file, stats)
		end
		all_scanned_received = true
		-- all.lua emits CHILDREN proactively during scan; no QUERY_CHILDREN needed.
		if pending_child_queries == 0 and pending_suggest then
			pending_suggest = false
			handle_suggest(stats)
		end
	end
	::continue::
end

-- Handle a lesson that was still in progress at EOF
if current and current.max_bass_id >= 0 and not current.results[current.max_bass_id] then
	local stats = load_stats(stats_file)
	finalize(stats)
end
