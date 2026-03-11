-- SPDX-License-Identifier: MIT
-- report.lua --- print a human-readable report from a stats file on stdin
-- Copyright (c) 2026 Jakob Kastelic
--
-- Usage:  lua report.lua < log/stats.log

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
	local mastery = entry.mastery or 0
	local ivl = entry.ivl or 0
	local last_date = entry.last_date
	if not last_date or ivl <= 0 then
		return 0
	end
	local y, m, d = last_date:match("(%d+)-(%d+)-(%d+)")
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

-- total time across all lessons
local total_secs = 0
for _, l in pairs(lessons) do
	total_secs = total_secs + (l.total_duration or 0)
end
local n_lessons = 0
for _ in pairs(lessons) do
	n_lessons = n_lessons + 1
end
local n_chunks = 0
for _ in pairs(chunks) do
	n_chunks = n_chunks + 1
end
print(string.format("Total   lessons: %d   chunks: %d   practice time: %s", n_lessons, n_chunks, fmt_dur(total_secs)))

-- ── lessons ───────────────────────────────────────────────────────────────────

local lesson_ids = {}
for id in pairs(lessons) do
	table.insert(lesson_ids, id)
end
table.sort(lesson_ids, function(a, b)
	return (tonumber(a) or 0) < (tonumber(b) or 0)
end)

if #lesson_ids > 0 then
	print()
	print(string.format("LESSONS (%d)", #lesson_ids))
	-- header row
	print(
		string.format(
			"  %-4s  %-8s  %-8s  %-4s  %-5s  %-4s  %-5s  %-5s  %-8s  %s",
			"ID",
			"Mastery",
			"Power",
			"Ivl",
			"Ease",
			"EMA",
			"Pass",
			"Fail",
			"Time",
			"Last"
		)
	)
	print(string.rep("-", 74))
	for _, id in ipairs(lesson_ids) do
		local l = lessons[id]
		local mastery_pct = normalize(l.mastery or 0, l)
		local power = calculate_power(l)
		local power_pct = normalize(power, l)
		local elapsed = days_since(l.last_date)
		local ivl = math.floor(l.ivl or 0)
		-- mark overdue lessons
		local due = ""
		if ivl > 0 and elapsed and elapsed >= ivl then
			due = " *"
		end
		print(
			string.format(
				"  %-4s  %6.1f%%   %6.1f%%   %4d  %5.2f  %.2f  %5d  %5d  %8s  %s%s",
				id,
				mastery_pct,
				power_pct,
				ivl,
				l.ease or (alg.ease_initial or 2.5),
				l.ema_pass or 0,
				l.n_pass_tot or 0,
				l.n_fail_tot or 0,
				fmt_dur(l.total_duration or 0),
				l.last_date or "--",
				due
			)
		)
	end
end

-- ── chunks ────────────────────────────────────────────────────────────────────

local chunk_hashes = {}
for h in pairs(chunks) do
	table.insert(chunk_hashes, h)
end
-- sort by last_date descending, then hash
table.sort(chunk_hashes, function(a, b)
	local da = chunks[a].last_date or ""
	local db = chunks[b].last_date or ""
	if da ~= db then
		return da > db
	end
	return a < b
end)

if #chunk_hashes > 0 then
	print()
	print(string.format("CHUNKS (%d)", #chunk_hashes))
	print(
		string.format(
			"  %-10s  %-3s  %-8s  %-8s  %-4s  %-5s  %-4s  %-5s  %-5s  %s",
			"Hash",
			"Grp",
			"Mastery",
			"Power",
			"Ivl",
			"Ease",
			"EMA",
			"Pass",
			"Fail",
			"Last"
		)
	)
	print(string.rep("-", 74))
	for _, h in ipairs(chunk_hashes) do
		local c = chunks[h]
		local mastery_pct = normalize(c.mastery or 0, c)
		local power = calculate_power(c)
		local power_pct = normalize(power, c)
		local ivl = math.floor(c.ivl or 0)
		local elapsed = days_since(c.last_date)
		local due = ""
		if ivl > 0 and elapsed and elapsed >= ivl then
			due = " *"
		end
		print(
			string.format(
				"  %-10s  %3d  %6.1f%%   %6.1f%%   %4d  %5.2f  %.2f  %5d  %5d  %s%s",
				h:sub(1, 10),
				c.max_groups or 0,
				mastery_pct,
				power_pct,
				ivl,
				c.ease or (alg.ease_initial or 2.5),
				c.ema_pass or 0,
				c.n_pass_tot or 0,
				c.n_fail_tot or 0,
				c.last_date or "--",
				due
			)
		)
	end
end
