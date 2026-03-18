-- SPDX-License-Identifier: MIT
-- report.lua --- human-readable stats report (unified chunk table)
-- Copyright (c) 2026 Jakob Kastelic
--
-- DESCRIPTION
--      Reads a stats.log file from stdin, parses the Lua table it contains,
--      and prints a formatted summary: today's score and streak, then
--      a skills breakdown and a unified chunk table (all levels) sorted by
--      level then mastery.  Title and skill data are read from chn/<hash>.txt
--      files in the working directory (files that cannot be opened are skipped).
--
-- INPUT
--      A stats.log file (the format written by stats.lua) on stdin.
--      The file must be a valid Lua script that returns a table.  An empty
--      or unparseable file is handled gracefully with a diagnostic message.
--
-- OUTPUT
--      Plain-text report to stdout, all lines ≤ 80 characters.
--
-- USAGE
--      lua src/report.lua < log/stats.log

local content = io.read("*a")
if not content or content:match("^%s*$") then
	print("(empty stats file)")
	os.exit(0)
end

local loader = load(content)
if not loader then
	io.stderr:write("Error: cannot parse stats file\n")
	os.exit(1)
end

local data = loader()
if type(data) ~= "table" then
	io.stderr:write("Error: stats file did not return a table\n")
	os.exit(1)
end

local alg = data.algorithm or {}
local lessons = data.lessons or {}
local chunks = data.chunks or {}
local daily = data.daily or {}

-- ── helpers ───────────────────────────────────────────────────────────────────

local function normalize(raw, entry)
	local mg = entry.max_groups or 0
	if mg <= 0 then
		return 0
	end
	return (raw / (mg * 5.0)) * 100.0
end

local function calculate_power(entry)
	-- Uses effective mastery (max of direct and transitive) and the more recent
	-- of last_date / t_last_date, so transitive practice maintains power.
	local mastery = math.max(entry.mastery or 0, entry.t_mastery or 0)
	local ivl = entry.ivl or 0
	local eff_last = entry.last_date
	local t = entry.t_last_date
	if t and (not eff_last or t > eff_last) then
		eff_last = t
	end
	if not eff_last or ivl <= 0 then
		return 0
	end
	local y, m, d = eff_last:match("(%d+)-(%d+)-(%d+)")
	local last_ts = os.time({ year = y, month = m, day = d, hour = 12 })
	local days_elapsed = math.floor(os.difftime(os.time(), last_ts) / 86400)
	local half_life = alg.power_half_life or 0.693
	local stability = math.exp(-half_life * (math.max(0, days_elapsed) / ivl))
	return math.min(mastery, mastery * stability)
end

local function days_since(date_str)
	if not date_str then
		return nil
	end
	local y, m, d = date_str:match("(%d+)-(%d+)-(%d+)")
	local ts = os.time({ year = y, month = m, day = d, hour = 12 })
	return math.floor(os.difftime(os.time(), ts) / 86400)
end

local function calculate_streak()
	local streak = 0
	local current_ts = os.time()
	local today = os.date("%Y-%m-%d")
	local goal = alg.score_goal or 1000
	while true do
		local d_str = os.date("%Y-%m-%d", current_ts)
		local day_score = (daily[d_str] and daily[d_str].score) or 0
		if day_score >= goal then
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

local function fmt_dur(secs)
	if not secs or secs <= 0 then
		return "--"
	end
	local h = math.floor(secs / 3600)
	local min = math.floor((secs % 3600) / 60)
	local s = math.floor(secs % 60)
	if h > 0 then
		return string.format("%dh%02dm", h, min)
	elseif min > 0 then
		return string.format("%dm%02ds", min, s)
	else
		return string.format("%ds", s)
	end
end

local function bar(frac, width)
	local filled = math.floor((frac or 0) * width + 0.5)
	filled = math.max(0, math.min(filled, width))
	return string.rep("#", filled) .. string.rep(".", width - filled)
end

-- ── header ────────────────────────────────────────────────────────────────────

local today = os.date("%Y-%m-%d")
local today_data = daily[today] or { score = 0, duration = 0 }
local streak = calculate_streak()
local goal = alg.score_goal or 1000
local today_score = today_data.score or 0
local goal_frac = math.min(1.0, today_score / goal)

print(string.format("=== Continuo Trainer  %s ===", today))
print(
	string.format(
		"Today   score %6.1f / %-6.0f [%s] %d%%",
		today_score,
		goal,
		bar(goal_frac, 20),
		math.floor(goal_frac * 100)
	)
)
print(
	string.format(
		"        time  %-8s   streak: %d day%s",
		fmt_dur(today_data.duration or 0),
		streak,
		streak == 1 and "" or "s"
	)
)

local total_secs = 0
local n_chunks = 0
for _, c in pairs(chunks) do
	total_secs = total_secs + (c.total_duration or 0)
	n_chunks = n_chunks + 1
end
print(string.format("Total   chunks: %d   practice time: %s", n_chunks, fmt_dur(total_secs)))

-- ── algorithm parameters ──────────────────────────────────────────────────────

local param_order = {
	"score_goal",
	"ema_alpha",
	"pass_accuracy",
	"mastery_growth",
	"power_half_life",
	"mastery_points_per_pct",
	"power_points_per_pct",
	"bottleneck_thresh",
	"dominance_margin",
	"min_quality",
	"power_score_frac",
	"overlearn_min",
	"overlearn_max",
	"mistake_power_penalty",
	"mastery_decay_half_life",
	"weak_ema_thresh",
	"ease_initial",
	"ease_min",
	"ease_max",
	"ease_pass_delta",
	"ease_fail_delta",
	"ivl_first",
	"ivl_second",
	"ivl_max",
	"chunk_mastery_thresh",
	"chunk_power_thresh",
	"skill_order",
}

if next(alg) then
	print()
	print("ALGORITHM PARAMETERS")
	print(string.rep("-", 60))
	local in_order = {}
	for _, k in ipairs(param_order) do
		in_order[k] = true
		local v = alg[k]
		if v ~= nil then
			local vs = type(v) == "number"
					and (math.floor(v) == v and tostring(math.floor(v)) or string.format("%.4g", v))
				or tostring(v)
			print(string.format("  %-28s  %s", k, vs))
		end
	end
	local extra = {}
	for k in pairs(alg) do
		if not in_order[k] then
			extra[#extra + 1] = k
		end
	end
	table.sort(extra)
	for _, k in ipairs(extra) do
		local v = alg[k]
		local vs = type(v) == "number" and (math.floor(v) == v and tostring(math.floor(v)) or string.format("%.4g", v))
			or tostring(v)
		print(string.format("  %-28s  %s", k, vs))
	end
end

-- ── per-skill stats ───────────────────────────────────────────────────────────

local skill_stats = {}
for h, c in pairs(chunks) do
	local skills_str = chunks[h].skills
	if skills_str and skills_str ~= "" then
		local m_pct = normalize(c.mastery or 0, c)
		local p_pct = normalize(calculate_power(c), c)
		local practiced = (c.n_pass_tot or 0) + (c.n_fail_tot or 0) > 0
		for skill in skills_str:gmatch("%S+") do
			local ss = skill_stats[skill]
			if not ss then
				ss = {
					n_chunks = 0,
					n_practiced = 0,
					m_sum = 0,
					p_sum = 0,
					pass = 0,
					fail = 0,
					ema_sum = 0,
					ema_n = 0,
					ease_sum = 0,
					ease_n = 0,
					best_avg_sum = 0,
					best_avg_n = 0,
					dur_sum = 0,
				}
				skill_stats[skill] = ss
			end
			ss.n_chunks = ss.n_chunks + 1
			ss.m_sum = ss.m_sum + m_pct
			ss.p_sum = ss.p_sum + p_pct
			ss.pass = ss.pass + (c.n_pass_tot or 0)
			ss.fail = ss.fail + (c.n_fail_tot or 0)
			ss.dur_sum = ss.dur_sum + (c.total_duration or 0)
			if practiced then
				ss.n_practiced = ss.n_practiced + 1
			end
			if c.ema_pass then
				ss.ema_sum = ss.ema_sum + c.ema_pass
				ss.ema_n = ss.ema_n + 1
			end
			if c.ease then
				ss.ease_sum = ss.ease_sum + c.ease
				ss.ease_n = ss.ease_n + 1
			end
			if c.best_avg then
				ss.best_avg_sum = ss.best_avg_sum + c.best_avg
				ss.best_avg_n = ss.best_avg_n + 1
			end
		end
	end
end

local skill_names = {}
for s in pairs(skill_stats) do
	skill_names[#skill_names + 1] = s
end
local sk_rank, sk_n = {}, 0
for sk in (alg.skill_order or ""):gmatch("%S+") do
	sk_rank[sk] = sk_n
	sk_n = sk_n + 1
end
table.sort(skill_names, function(a, b)
	local ra = sk_rank[a] or sk_n
	local rb = sk_rank[b] or sk_n
	if ra ~= rb then
		return ra < rb
	end
	return a < b
end)

if #skill_names > 0 then
	print()
	print(string.format("SKILLS (%d)", #skill_names))
	print("  Chunks: practiced/total.  Mastery/Power: avg % across chunks.")
	print("  Rate: pass/(pass+fail) sessions.  EMA: avg smoothed pass rate.")
	print("  Ease: avg SRS multiplier.  Speed: avg personal-best transition time.  Time: total practice.")
	print()
	print(
		string.format(
			"  %-10s  %11s  %-7s  %-7s  %5s  %5s  %5s  %5s  %5s  %7s  %8s",
			"Skill",
			"Chunks",
			"Mastery",
			"Power",
			"Pass",
			"Fail",
			"Rate",
			"EMA",
			"Ease",
			"Speed",
			"Time"
		)
	)
	print(string.rep("-", 95))
	for _, sname in ipairs(skill_names) do
		local ss = skill_stats[sname]
		local total = ss.pass + ss.fail
		local rate = total > 0 and string.format("%4.0f%%", ss.pass / total * 100) or "  -- "
		local ema = ss.ema_n > 0 and string.format("%4.0f%%", ss.ema_sum / ss.ema_n * 100) or "  -- "
		local ease = ss.ease_n > 0 and string.format("%.2f", ss.ease_sum / ss.ease_n) or "  -- "
		local speed = ss.best_avg_n > 0 and string.format("%5.3fs", ss.best_avg_sum / ss.best_avg_n) or "    --"
		print(
			string.format(
				"  %-10s  %5d/%-5d  %6.1f%%  %6.1f%%  %5d  %5d  %s  %s  %s  %s  %s",
				sname,
				ss.n_practiced,
				ss.n_chunks,
				ss.m_sum / ss.n_chunks,
				ss.p_sum / ss.n_chunks,
				ss.pass,
				ss.fail,
				rate,
				ema,
				ease,
				speed,
				fmt_dur(ss.dur_sum)
			)
		)
	end
end

-- ── chunks ────────────────────────────────────────────────────────────────────

local chunk_hashes = {}
for h in pairs(chunks) do
	chunk_hashes[#chunk_hashes + 1] = h
end
table.sort(chunk_hashes, function(a, b)
	local la = chunks[a].level or 0
	local lb = chunks[b].level or 0
	if la ~= lb then
		return la < lb
	end
	return normalize(chunks[a].mastery or 0, chunks[a]) < normalize(chunks[b].mastery or 0, chunks[b])
end)

if #chunk_hashes > 0 then
	local ema_window = alg.ema_alpha and string.format("~%.0f", 1.0 / alg.ema_alpha) or "?"
	local ease_init = alg.ease_initial or 2.5
	local ease_min = alg.ease_min or 1.3
	local ease_max = alg.ease_max or 3.5

	print()
	print(string.format("CHUNKS (%d)", #chunk_hashes))
	print()
	print("  Column legend:")
	print("    Hash     First 8 hex chars of the chunk's SHA-1 ID")
	print("    L        Level: 0=full lesson, 1=3-bar drill, 2=1-bar drill")
	print("    Dir%     Direct mastery: scored from sessions on this chunk itself")
	print("    Trn%     Transitive mastery: inferred from practice of parent chunks")
	print("    Power    Retention: mastery decayed by time elapsed vs review interval")
	print("    Pass     Total all-correct sessions")
	print("    Fail     Total sessions with at least one error")
	print("    Rate     Pass / (Pass+Fail)")
	print(
		string.format(
			"    EMA      Exponential moving average of pass rate (%s-session window, alpha=%s)",
			ema_window,
			tostring(alg.ema_alpha or "?")
		)
	)
	print(
		string.format(
			"    Ease     SRS ease multiplier (initial=%.1f, min=%.1f, max=%.1f; falls on fail, rises on pass)",
			ease_init,
			ease_min,
			ease_max
		)
	)
	print("    Ivl      SRS review interval in days (* = review overdue)")
	print("    BestAvg  Fastest recorded average inter-note transition time")
	print("    PFac     Power factor: penalty for failed sub-ranges (100%=clean)")
	print("    Last     Date of most recent practice (direct or transitive)")
	print()
	print(
		string.format(
			"  %-8s  %1s  %-7s  %-7s  %-7s  %5s  %5s  %5s  %5s  %5s  %4s  %7s  %5s  %-10s",
			"Hash",
			"L",
			"Dir%",
			"Trn%",
			"Power",
			"Pass",
			"Fail",
			"Rate",
			"EMA",
			"Ease",
			"Ivl",
			"BestAvg",
			"PFac",
			"Last"
		)
	)
	print(string.rep("-", 100))
	for _, h in ipairs(chunk_hashes) do
		local c = chunks[h]
		local dir_pct = normalize(c.mastery or 0, c)
		local trn_pct = normalize(c.t_mastery or 0, c)
		local power_pct = normalize(calculate_power(c), c)
		local ivl = math.floor(c.ivl or 0)
		local eff_last = c.last_date
		local t = c.t_last_date
		if t and (not eff_last or t > eff_last) then
			eff_last = t
		end
		local elapsed = days_since(eff_last)
		local due = (ivl > 0 and elapsed and elapsed >= ivl) and "*" or " "
		local pass = c.n_pass_tot or 0
		local fail = c.n_fail_tot or 0
		local total = pass + fail
		local rate = total > 0 and string.format("%4.0f%%", pass / total * 100) or "  -- "
		local ema = c.ema_pass and string.format("%4.0f%%", c.ema_pass * 100) or "  -- "
		local ease = string.format("%.2f", c.ease or 0)
		local best_avg = c.best_avg and string.format("%5.3fs", c.best_avg) or "    --"
		local pfac = c.power_factor and string.format("%4.0f%%", c.power_factor * 100) or "  -- "
		print(
			string.format(
				"  %-8s  %1d  %6.1f%%  %6.1f%%  %6.1f%%  %5d  %5d  %s  %s  %s  %4d%s  %s  %s  %s",
				h:sub(1, 8),
				c.level or 0,
				dir_pct,
				trn_pct,
				power_pct,
				pass,
				fail,
				rate,
				ema,
				ease,
				ivl,
				due,
				best_avg,
				pfac,
				eff_last or "--"
			)
		)
	end
end
