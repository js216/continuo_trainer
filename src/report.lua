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

local total_secs = 0
local n_chunks = 0
for _, c in pairs(chunks) do
	total_secs = total_secs + (c.total_duration or 0)
	n_chunks = n_chunks + 1
end
print(string.format("Total   chunks: %d   practice time: %s", n_chunks, fmt_dur(total_secs)))

-- ── read chunk metadata (title + skills) in one pass ─────────────────────────

local chunk_meta = {} -- [hash] = {title, skills}
for h in pairs(chunks) do
	local title, skills
	local f = io.open("chn/" .. h .. ".txt", "r")
	if f then
		for line in f:lines() do
			if not title then
				title = line:match("^title:%s*(.-)%s*$")
				if title == "" then
					title = nil
				end
			end
			if not skills then
				skills = line:match("^skills:%s*(.+)$")
			end
			if title and skills then
				break
			end
		end
		f:close()
	end
	chunk_meta[h] = { title = title or "", skills = skills or "" }
end

-- ── skills ────────────────────────────────────────────────────────────────────
-- Level-1 chunks have a skills: field; level-0 do not.

local skill_stats = {} -- [skill] = {n, m_sum, p_sum, pass, fail}
for h, c in pairs(chunks) do
	local skills_str = chunk_meta[h].skills
	if skills_str ~= "" then
		local m_pct = normalize(c.mastery or 0, c)
		local p_pct = normalize(calculate_power(c), c)
		for skill in skills_str:gmatch("%S+") do
			local ss = skill_stats[skill]
			if not ss then
				ss = { n = 0, m_sum = 0, p_sum = 0, pass = 0, fail = 0 }
				skill_stats[skill] = ss
			end
			ss.n = ss.n + 1
			ss.m_sum = ss.m_sum + m_pct
			ss.p_sum = ss.p_sum + p_pct
			ss.pass = ss.pass + (c.n_pass_tot or 0)
			ss.fail = ss.fail + (c.n_fail_tot or 0)
		end
	end
end

local skill_names = {}
for s in pairs(skill_stats) do
	table.insert(skill_names, s)
end
-- Weakest first (lowest average mastery).
table.sort(skill_names, function(a, b)
	return (skill_stats[a].m_sum / skill_stats[a].n) < (skill_stats[b].m_sum / skill_stats[b].n)
end)

-- Column widths: 2+10+2+4+2+7+2+7+2+5+2+5 = 50
if #skill_names > 0 then
	print()
	print(string.format("SKILLS (%d)", #skill_names))
	print(string.format("  %-10s  %4s  %-7s  %-7s  %-5s  %s", "Skill", "N", "Mastery", "Power", "Pass", "Fail"))
	print(string.rep("-", 50))
	for _, sname in ipairs(skill_names) do
		local ss = skill_stats[sname]
		print(
			string.format(
				"  %-10s  %4d  %6.1f%%  %6.1f%%  %5d  %5d",
				sname,
				ss.n,
				ss.m_sum / ss.n,
				ss.p_sum / ss.n,
				ss.pass,
				ss.fail
			)
		)
	end
end

-- ── chunks ────────────────────────────────────────────────────────────────────
-- Unified table: all levels together, sorted by level then mastery ascending.

local chunk_hashes = {}
for h in pairs(chunks) do
	table.insert(chunk_hashes, h)
end
table.sort(chunk_hashes, function(a, b)
	local la = chunks[a].level or 0
	local lb = chunks[b].level or 0
	if la ~= lb then
		return la < lb
	end
	return normalize(chunks[a].mastery or 0, chunks[a]) < normalize(chunks[b].mastery or 0, chunks[b])
end)

-- Column widths: 2+8+2+1+2+16+2+7+2+7+2+4+2+4+2+4+2+10 = 79
if #chunk_hashes > 0 then
	print()
	print(string.format("CHUNKS (%d)", #chunk_hashes))
	print(
		string.format(
			"  %-8s  %1s  %-16s  %-7s  %-7s  %4s  %4s  %4s  %s",
			"Hash",
			"L",
			"Title",
			"Mastery",
			"Power",
			"Ivl",
			"Pass",
			"Fail",
			"Last"
		)
	)
	print(string.rep("-", 79))
	for _, h in ipairs(chunk_hashes) do
		local c = chunks[h]
		local mastery_pct = normalize(c.mastery or 0, c)
		local power_pct = normalize(calculate_power(c), c)
		local ivl = math.floor(c.ivl or 0)
		local elapsed = days_since(c.last_date)
		local due = (ivl > 0 and elapsed and elapsed >= ivl) and " *" or ""
		local title = chunk_meta[h].title
		if #title > 16 then
			title = title:sub(1, 15) .. "~"
		end
		print(
			string.format(
				"  %-8s  %1d  %-16s  %6.1f%%  %6.1f%%  %4d  %4d  %4d  %s%s",
				h:sub(1, 8),
				c.level or 0,
				title,
				mastery_pct,
				power_pct,
				ivl,
				c.n_pass_tot or 0,
				c.n_fail_tot or 0,
				c.last_date or "--",
				due
			)
		)
	end
end
