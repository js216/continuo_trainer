-- SPDX-License-Identifier: MIT
-- chunk.lua --- identify figure skills, generate and store lesson chunks
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--     chunks reads the normalised lesson output produced by bin/load from
--     standard input.  For each lesson it identifies "hearts" (the musical
--     skill at the centre of a chunk) and emits one chunk per heart.  Each
--     chunk consists of the heart plus CONTEXT_LEFT bass notes before it and
--     CONTEXT_RIGHT bass notes after it (clamped to lesson boundaries).
--
--     Chunks are stored under chn/<sha1>.txt in the same format as seq/*.txt
--     (key + bassline/figures/melody sections, no title or time signature).
--     The SHA-1 is computed over the file content.  If a file already exists
--     with the same hash and identical content the write is skipped
--     (idempotent).  A hash collision (same hash, different content) is
--     fatal.
--
-- PARAMETERS (edit at top of file)
--     CONTEXT_LEFT   number of context notes to the left of the heart
--     CONTEXT_RIGHT  number of context notes to the right of the heart
--
-- RECOGNIZED SKILLS
--     root        root-position triad: figure "0" or bare modifier (#/b/n)
--     6           first-inversion triad
--     6/4         second-inversion triad (cadential or passing)
--     7           dominant-seventh (root position)
--     6/5         first-inversion seventh
--     4/3         second-inversion seventh (within one figure token)
--     4/2         third-inversion seventh
--     2           shorthand for 4/2
--     4-3_sus     4-3 suspension spanning two consecutive bass notes:
--                 figure[i] carries an isolated "4" (no "6"), figure[i+1]
--                 resolves to a third ("3", "#", "b", "n", or "0")
--     other       any figure not matched by the above rules
--
-- INPUT FORMAT (from bin/load)
--     CHUNK_SESSION <hash>    Signals that the following LESSON…LESSON_END
--                             is a chunk replay (loaded via LOAD_CHUNK).
--                             chunk.lua skips the entire session: no chunks
--                             are re-derived from an already-chunked file.
--     LESSON <n> <key> <time> <title>
--     BASSNOTE <i>: <token> [passing]
--     FIGURES  <i>: <token>
--     MELODY   <i>: <tokens|-|>
--
-- OUTPUT FORMAT (one line per chunk)
--     CHUNK LESSON:<n> HEART:<s>-<e> LEN:<len> HASH:<sha1>
--           SKILLS:<skill> BASSLINE:<n0>|<n1>|...
--           FIGURE:<f0>|<f1>|... MELODY:<m0>|<m1>|...
--     (all on one line; HEART indices are 0-based, matching BASSNOTE output)

local CONTEXT_LEFT = 1
local CONTEXT_RIGHT = 1

io.stdout:setvbuf("line") -- flush after every CHUNK line so stats.lua sees them promptly

-- ── helpers ──────────────────────────────────────────────────────────────────

local function die(fmt, ...)
	io.stderr:write("\27[31mError:\27[0m " .. string.format(fmt, ...) .. "\n")
	os.exit(1)
end

local function sha1(content)
	local tmp = os.tmpname()
	local f = assert(io.open(tmp, "wb"))
	f:write(content)
	f:close()
	local pipe = io.popen("sha1sum " .. tmp)
	local line = pipe:read("*l")
	pipe:close()
	os.remove(tmp)
	if not line then
		die("sha1sum failed")
	end
	return line:match("^(%x+)") or die("sha1sum output not recognised: %s", line)
end

-- ── skill detection ───────────────────────────────────────────────────────────

-- Return numeric intervals present in a figure token (split on "/").
-- Leading modifiers (#, b, n) are stripped to expose the digit(s).
local function fig_nums(fig)
	local nums = {}
	for token in fig:gmatch("[^/]+") do
		local n = token:match("^[#bn]*(%d+)$")
		if n then
			nums[tonumber(n)] = true
		end
	end
	return nums
end

-- True when figure contains the given numeric interval (possibly modified).
local function has_num(fig, n)
	return fig_nums(fig)[n] == true
end

-- True when figure contains an isolated "4" that signals a suspension:
-- a "4" is present but there is no "6" (which would make it a 6/4 chord).
local function is_suspension_4(fig)
	if fig == "0" then
		return false
	end
	return has_num(fig, 4) and not has_num(fig, 6)
end

-- True when figure resolves a 4-3 suspension (implies a third above the bass).
local function is_3_resolution(fig)
	if fig == "0" then
		return true
	end -- root position → 5/3/8 implied
	if fig:match("^[#bn]+$") then
		return true
	end -- bare modifier = altered third
	return has_num(fig, 3)
end

-- Name the skill of a single figure token.
local function skill_of_single(fig)
	if fig == "0" then
		return "root"
	end
	if fig:match("^[#bn]+$") then
		return "root"
	end -- bare modifier = altered-third root pos
	if has_num(fig, 6) and has_num(fig, 4) then
		return "6/4"
	end
	if has_num(fig, 6) and has_num(fig, 5) then
		return "6/5"
	end
	if has_num(fig, 4) and has_num(fig, 3) then
		return "4/3"
	end
	if has_num(fig, 4) and has_num(fig, 2) then
		return "4/2"
	end
	if has_num(fig, 7) then
		return "7"
	end
	if has_num(fig, 6) then
		return "6"
	end
	if has_num(fig, 2) then
		return "2"
	end
	-- Figures containing only 3rd, 5th, or 8th (possibly modified) are root-position
	-- chords with an altered interval, e.g. "#3", "b5", "5", "8", "#3/5".
	if not has_num(fig, 6) and not has_num(fig, 7) and not has_num(fig, 4) and not has_num(fig, 2) then
		return "root"
	end
	return "other"
end

-- Partition figures into hearts: {s, e, skill} with 1-based inclusive indices.
-- Multi-note skills are matched greedily left-to-right before single-note ones.
local function identify_hearts(figures)
	local N = #figures
	local hearts = {}
	local i = 1
	while i <= N do
		if i < N and is_suspension_4(figures[i]) and is_3_resolution(figures[i + 1]) then
			hearts[#hearts + 1] = { s = i, e = i + 1, skill = "4-3_sus" }
			i = i + 2
		else
			hearts[#hearts + 1] = { s = i, e = i, skill = skill_of_single(figures[i]) }
			i = i + 1
		end
	end
	return hearts
end

-- ── chunk file construction ───────────────────────────────────────────────────

-- Build the seq-format text for the slice bass[s..e] (1-based, inclusive).
local function build_chunk_content(key, skill, bass, passing, figures, melody, s, e)
	local lines = {}
	lines[#lines + 1] = "key: " .. key
	lines[#lines + 1] = "skills: " .. skill
	lines[#lines + 1] = ""
	lines[#lines + 1] = "bassline = {"
	for i = s, e do
		local tok = passing[i] and (bass[i] .. "p") or bass[i]
		lines[#lines + 1] = "  " .. tok
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	lines[#lines + 1] = "figures = {"
	for i = s, e do
		lines[#lines + 1] = "  " .. figures[i]
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	lines[#lines + 1] = "melody = {"
	for i = s, e do
		local m = melody[i]
		if m and m ~= "-" and m ~= "" then
			lines[#lines + 1] = "  " .. m
		end
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	return table.concat(lines, "\n")
end

-- ── inline summary helpers ────────────────────────────────────────────────────

local function fmt_bassline(bass, passing, s, e)
	local t = {}
	for i = s, e do
		t[#t + 1] = passing[i] and (bass[i] .. "p") or bass[i]
	end
	return table.concat(t, "|")
end

local function fmt_figures(figures, s, e)
	local t = {}
	for i = s, e do
		t[#t + 1] = figures[i]
	end
	return table.concat(t, "|")
end

local function fmt_melody(melody, s, e)
	local t = {}
	for i = s, e do
		local m = melody[i]
		t[#t + 1] = (m and m ~= "") and m or "-"
	end
	return table.concat(t, "|")
end

-- ── per-lesson processing ─────────────────────────────────────────────────────

local function process_lesson(lesson_n, key, bass, passing, figures, melody)
	local N = #bass
	local hearts = identify_hearts(figures)

	-- Map each note (1-based) to its heart for O(1) lookup.
	local note_heart = {}
	for _, h in ipairs(hearts) do
		for i = h.s, h.e do
			note_heart[i] = h
		end
	end

	for _, heart in ipairs(hearts) do
		local s = math.max(1, heart.s - CONTEXT_LEFT)
		local e = math.min(N, heart.e + CONTEXT_RIGHT)

		-- Don't start with a passing note unless the lesson itself starts with one.
		if passing[s] and s > 1 and not passing[1] then
			s = s - 1
		end

		-- Collect skills for every heart touched by [s..e], left to right,
		-- listing each heart's skill exactly once.
		local seen = {}
		local skills = {}
		for i = s, e do
			local sk = note_heart[i].skill
			if not seen[sk] then
				seen[sk] = true
				skills[#skills + 1] = sk
			end
		end
		local skills_str = table.concat(skills, " ")

		local content = build_chunk_content(key, skills_str, bass, passing, figures, melody, s, e)
		content = content:gsub("\r\n", "\n"):gsub("\r", "\n")
		local hash = sha1(content)
		print(
			string.format(
				"CHUNK LESSON:%d HEART:%d-%d LEN:%d HASH:%s KEY:%s SKILLS:%s BASSLINE:%s FIGURE:%s MELODY:%s",
				lesson_n,
				heart.s - 1,
				heart.e - 1,
				e - s + 1,
				hash,
				key,
				skills_str,
				fmt_bassline(bass, passing, s, e),
				fmt_figures(figures, s, e),
				fmt_melody(melody, s, e)
			)
		)
	end
end

-- ── main: parse bin/load output from stdin ────────────────────────────────────

local current = nil
local in_chunk_session = false -- true while processing a CHUNK_SESSION stream

local function flush()
	if current then
		process_lesson(current.n, current.key, current.bass, current.passing, current.figures, current.melody)
		current = nil
	end
end

for line in io.lines() do
	line = line:gsub("\r$", "")
	-- CHUNK_SESSION: the following LESSON..LESSON_END is a chunk replay; skip it
	if line:match("^CHUNK_SESSION ") then
		flush() -- safety flush of any pending lesson
		in_chunk_session = true
		current = nil
		goto continue
	end

	-- LESSON_END: all data for current lesson received — emit chunks immediately
	if line:match("^LESSON_END") then
		if in_chunk_session then
			in_chunk_session = false
			goto continue -- no flush for chunk sessions
		end
		flush()
		goto continue
	end

	-- LESSON <n> <key> <time> <title>
	local n, key = line:match("^LESSON (%S+) (%S+)")
	if n then
		if in_chunk_session then
			-- The LESSON line from a chunk stream uses a hash id, not a number
			in_chunk_session = false
			goto continue
		end
		local num = tonumber(n)
		if num then
			flush() -- safety flush in case LESSON_END was missing
			current = { n = num, key = key, bass = {}, passing = {}, figures = {}, melody = {} }
		end
		goto continue
	end

	if current then
		-- BASSNOTE <i>: <token> [passing]
		local tok = line:match("^BASSNOTE %d+: (%S+)")
		if tok then
			current.bass[#current.bass + 1] = tok
			current.passing[#current.passing + 1] = line:match(" passing$") ~= nil
			goto continue
		end

		-- FIGURES <i>: <token>
		local fig = line:match("^FIGURES %d+: (.+)$")
		if fig then
			current.figures[#current.figures + 1] = fig
			goto continue
		end

		-- MELODY <i>: <tokens|-|>
		local mel = line:match("^MELODY %d+: (.+)$")
		if mel then
			current.melody[#current.melody + 1] = mel
			goto continue
		end
	end

	::continue::
end

flush()
