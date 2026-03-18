-- SPDX-License-Identifier: MIT
-- all.lua --- build chunk files and serve as a persistent lesson/chunk index
-- Copyright (c) 2026 Jakob Kastelic
--
-- DESCRIPTION
--      On startup (and on every RESCAN command) the script:
--        1. Scans seq/*.txt and emits one LESSON_NAME line per lesson in
--           ascending numeric order.
--        2. For every lesson, generates the corresponding level-0 chunk (the
--           full lesson body with level: 0) and emits CHUNK_NAME <hash> 0.
--        3. Identifies skill-slice (level-1) chunks for every lesson and emits
--           CHUNK_NAME <hash> 1 for each.
--        4. Checks that chn/ contains no files that were not produced by
--           the current lesson set.  Any such stale file is reported to
--           stderr and the script exits with code 1.
--        5. Emits ALL_SCANNED to signal a clean, complete scan.
--      After the initial scan the script blocks on stdin until EOF.  Connect
--      a long-lived process (e.g. bin/gui) to keep it alive.
--
-- INPUT
--      RESCAN                  Repeat the full scan-and-emit cycle.
--      LOAD_CHUNK <hash>       Load chn/<hash>.txt and emit the lesson protocol.
--      QUERY_CHILDREN <hash>   Emit CHILDREN line for the given chunk.
--      (all other lines are silently ignored)
--
-- OUTPUT
--      LESSON_NAME <n>             One per lesson, in ascending numeric order.
--      CHUNK_NAME <hash> <level> [<skills>]  One per unique chunk; level-1 chunks include space-separated skills.
--      ALL_SCANNED                 Emitted once after a clean scan.
--      CHUNK_SESSION <hash>        Before LESSON/BASSNOTE/FIGURES/MELODY/LESSON_END.
--      LESSON <hash> <key> <time> <bpm> <bar>
--      BASSNOTE <i>: <token> [passing]
--      FIGURES <i>: <token>
--      MELODY <i>: <tokens|-|>
--      LESSON_END

local function die(fmt, ...)
	io.stderr:write(string.format("Error: " .. fmt, ...) .. "\n")
	os.exit(1)
end

-- ── SHA-1 ─────────────────────────────────────────────────────────────────────

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

-- ── chunk file I/O ────────────────────────────────────────────────────────────

local function write_chunk(hash, content)
	local path = "chn/" .. hash .. ".txt"
	local existing = io.open(path, "rb")
	if existing then
		local old = existing:read("*a")
		existing:close()
		if old ~= content then
			die("hash collision on %s", path)
		end
		return
	end
	local f = assert(io.open(path, "wb"))
	f:write(content)
	f:close()
end

-- ── skill detection ───────────────────────────────────────────────────────────

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

local function has_num(fig, n)
	return fig_nums(fig)[n] == true
end

local function is_suspension_4(fig)
	if fig == "0" then
		return false
	end
	return has_num(fig, 4) and not has_num(fig, 6)
end

local function is_3_resolution(fig)
	if fig == "0" then
		return true
	end
	if fig:match("^[#bn]+$") then
		return true
	end
	return has_num(fig, 3)
end

local function skill_of_single(fig)
	if fig == "0" then
		return "root"
	end
	if fig:match("^[#bn]+$") then
		return "root"
	end
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
	if not has_num(fig, 6) and not has_num(fig, 7) and not has_num(fig, 4) and not has_num(fig, 2) then
		return "root"
	end
	return "other"
end

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

-- ── bar-number helpers ────────────────────────────────────────────────────────

local function dur_sixteenths(token)
	local s = token:gsub("^[a-z]+", ""):gsub("^[',]+", "")
	local num_str = s:match("^(%d+)")
	if not num_str then
		return nil
	end
	local base_map = { [1] = 16, [2] = 8, [4] = 4, [8] = 2, [16] = 1 }
	local base = base_map[tonumber(num_str)]
	if not base then
		return nil
	end
	local after = s:sub(#num_str + 1)
	local dots = 0
	for c in after:gmatch(".") do
		if c == "." then
			dots = dots + 1
		else
			break
		end
	end
	local total, add = base, base
	for _ = 1, dots do
		add = math.floor(add / 2)
		total = total + add
	end
	return total
end

local function bar_sixteenths(time_sig)
	local n, d = time_sig:match("^(%d+)/(%d+)$")
	if not n then
		return 16
	end
	return tonumber(n) * math.floor(16 / tonumber(d))
end

local function build_offsets(bass)
	local offsets = {}
	local cum = 0
	local last_dur = 4
	for i, tok in ipairs(bass) do
		offsets[i] = cum
		local d = dur_sixteenths(tok)
		if d then
			last_dur = d
		end
		cum = cum + last_dur
	end
	offsets[#bass + 1] = cum
	return offsets
end

local function bar_split(time_sig, ps, pe, offsets, step_bars)
	local bar_len = bar_sixteenths(time_sig)
	local ps_time = offsets[ps]
	local pe_time = offsets[pe + 1]
	local step = step_bars * bar_len
	local chunks = {}
	local k = 0

	while true do
		local chunk_start_time = ps_time + k * step
		if chunk_start_time >= pe_time then
			break
		end

		-- First note at or after chunk_start_time
		local s = pe
		for i = ps, pe do
			if offsets[i] >= chunk_start_time then
				s = i
				break
			end
		end

		local next_boundary = ps_time + (k + 1) * step
		if next_boundary >= pe_time then
			chunks[#chunks + 1] = { s = s, e = pe }
			break
		end

		-- Last note starting before next_boundary + 0.5*bar_len
		local cut_time = next_boundary + math.floor(bar_len / 2)
		local e = s
		for i = s, pe do
			if offsets[i] < cut_time then
				e = i
			else
				break
			end
		end
		chunks[#chunks + 1] = { s = s, e = e }
		k = k + 1
	end

	-- Merge trailing chunk that is shorter than 1.5 bars into the previous one
	if #chunks >= 2 then
		local last = chunks[#chunks]
		local last_dur = offsets[last.e + 1] - offsets[last.s]
		if last_dur < math.floor(bar_len * 3 / 2) then
			chunks[#chunks - 1].e = last.e
			chunks[#chunks] = nil
		end
	end

	return chunks
end

local function chunk_bar(bass, time_sig, lesson_bar, start_idx)
	if start_idx == 1 then
		return lesson_bar
	end
	local bar_len = bar_sixteenths(time_sig)
	local offset, last_dur = 0, 4
	for i = 1, start_idx - 1 do
		local d = dur_sixteenths(bass[i])
		if d then
			last_dur = d
		end
		offset = offset + last_dur
	end
	return lesson_bar + math.floor(offset / bar_len)
end

local function chunk_partial_sixteenths(bass, time_sig, start_idx)
	if start_idx == 1 then
		return 0
	end
	local bar_len = bar_sixteenths(time_sig)
	local offset, last_dur = 0, 4
	for i = 1, start_idx - 1 do
		local d = dur_sixteenths(bass[i])
		if d then
			last_dur = d
		end
		offset = offset + last_dur
	end
	local offset_in_bar = offset % bar_len
	if offset_in_bar == 0 then
		return 0
	end
	return bar_len - offset_in_bar
end

-- ── level-1 chunk content ─────────────────────────────────────────────────────

local function build_chunk_content(
	key,
	title,
	composer,
	time_sig,
	bpm,
	level,
	bar,
	partial,
	bass,
	passing,
	figures,
	melody,
	s,
	e
)
	local lines = {}
	if title ~= "" then
		lines[#lines + 1] = "title: " .. title
	end
	if composer ~= "" then
		lines[#lines + 1] = "composer: " .. composer
	end
	lines[#lines + 1] = "key: " .. key
	if time_sig ~= "" then
		lines[#lines + 1] = "time: " .. time_sig
	end
	if bpm and bpm > 0 then
		lines[#lines + 1] = "bpm: " .. bpm
	end
	if bar ~= 1 then
		lines[#lines + 1] = "bar: " .. bar
	end
	if partial > 0 then
		lines[#lines + 1] = "partial: " .. partial
	end
	lines[#lines + 1] = "level: " .. level
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

-- ── bar-based chunk generation ────────────────────────────────────────────────

-- Returns a list of {hash, s, e, skills} (1-indexed, deduped by hash).
local function compute_bar_children(
	level,
	key,
	time_sig,
	bpm,
	lesson_bar,
	title,
	composer,
	bass,
	passing,
	figures,
	melody,
	ps,
	pe,
	offsets,
	step_bars
)
	local splits = bar_split(time_sig, ps, pe, offsets, step_bars)
	local children = {}
	local seen_hashes = {}

	for _, sp in ipairs(splits) do
		local s, e = sp.s, sp.e

		-- Extract skills from this slice's figures
		local slice_figs = {}
		for i = s, e do
			slice_figs[i - s + 1] = figures[i]
		end
		local hearts = identify_hearts(slice_figs)
		local seen_sk, skills = {}, {}
		for _, h in ipairs(hearts) do
			if not seen_sk[h.skill] then
				seen_sk[h.skill] = true
				skills[#skills + 1] = h.skill
			end
		end
		local skills_str = table.concat(skills, " ")

		local bar = chunk_bar(bass, time_sig, lesson_bar, s)
		local partial = chunk_partial_sixteenths(bass, time_sig, s)
		local content = build_chunk_content(
			key,
			title,
			composer,
			time_sig,
			bpm,
			level,
			bar,
			partial,
			bass,
			passing,
			figures,
			melody,
			s,
			e
		)
		content = content:gsub("\r\n", "\n"):gsub("\r", "\n")
		local hash = sha1(content)
		write_chunk(hash, content)
		if not seen_hashes[hash] then
			seen_hashes[hash] = true
			children[#children + 1] = { hash = hash, s = s, e = e, skills = skills_str }
		end
	end
	return children
end

-- ── LOAD_CHUNK: parse chunk file and emit protocol ───────────────────────────

local function is_passing_tok(tok)
	if tok:sub(-1) ~= "p" then
		return false
	end
	local pre = tok:sub(-2, -2)
	return pre:match("%d") ~= nil or pre == "."
end

local function parse_chunk(content)
	local r = {
		title = "",
		composer = "",
		key = "C",
		time = "4/4",
		bpm = 120,
		bar = 1,
		bass = {},
		figures = {},
		melody = {},
	}
	local mode = ""
	for line in (content .. "\n"):gmatch("([^\n]*)\n") do
		local l = line:match("^%s*(.-)%s*$")
		if l == "" then
			goto cont
		end
		if mode == "" then
			local v
			v = l:match("^title:%s*(.-)%s*$")
			if v and v ~= "" then
				r.title = v
				goto cont
			end
			v = l:match("^composer:%s*(.-)%s*$")
			if v and v ~= "" then
				r.composer = v
				goto cont
			end
			v = l:match("^key:%s*(.-)%s*$")
			if v and v ~= "" then
				r.key = v
				goto cont
			end
			v = l:match("^time:%s*(.-)%s*$")
			if v and v ~= "" then
				r.time = v
				goto cont
			end
			v = l:match("^bpm:%s*(%d+)%s*$")
			if v then
				r.bpm = tonumber(v) or r.bpm
				goto cont
			end
			v = l:match("^bar:%s*(%d+)%s*$")
			if v then
				r.bar = tonumber(v) or r.bar
				goto cont
			end
			if l:match("^bassline") then
				mode = "bass"
				goto cont
			end
			if l:match("^figures") then
				mode = "fig"
				goto cont
			end
			if l:match("^melody") then
				mode = "mel"
				goto cont
			end
		elseif l:match("^}") then
			mode = ""
		else
			for tok in l:gmatch("%S+") do
				if mode == "bass" then
					r.bass[#r.bass + 1] = tok
				elseif mode == "fig" then
					r.figures[#r.figures + 1] = tok
				elseif mode == "mel" then
					r.melody[#r.melody + 1] = tok
				end
			end
		end
		::cont::
	end
	return r
end

local function group_melody_tokens(bass, melody)
	local last_dur = 4
	local bass_durs = {}
	for i, tok in ipairs(bass) do
		local d = dur_sixteenths(tok)
		if d then
			last_dur = d
		end
		bass_durs[i] = last_dur
	end

	local groups = {}
	for i = 1, #bass do
		groups[i] = ""
	end

	local mel_idx = 1
	local last_mel_dur = 4
	local bass_total = 0
	local mel_total = 0

	for bi, bd in ipairs(bass_durs) do
		bass_total = bass_total + bd
		while mel_total < bass_total and mel_idx <= #melody do
			local tok = melody[mel_idx]
			local d = dur_sixteenths(tok)
			if d then
				last_mel_dur = d
			else
				d = last_mel_dur
			end
			if groups[bi] ~= "" then
				groups[bi] = groups[bi] .. " "
			end
			groups[bi] = groups[bi] .. tok
			mel_total = mel_total + d
			mel_idx = mel_idx + 1
		end
	end
	return groups
end

local function load_chunk(hash)
	local path = "chn/" .. hash .. ".txt"
	local f = io.open(path, "rb")
	if not f then
		die("Cannot read %s", path)
	end
	local content = f:read("*a")
	f:close()

	local lesson = parse_chunk(content)
	if #lesson.bass ~= #lesson.figures then
		die("Length mismatch in %s: bass=%d figures=%d", path, #lesson.bass, #lesson.figures)
	end

	local groups = group_melody_tokens(lesson.bass, lesson.melody)

	io.write("CHUNK_SESSION " .. hash .. "\n")
	io.write(string.format("LESSON %s %s %s %d %d\n", hash, lesson.key, lesson.time, lesson.bpm, lesson.bar))
	for i, tok in ipairs(lesson.bass) do
		local passing = is_passing_tok(tok)
		local clean = passing and tok:sub(1, -2) or tok
		if passing then
			io.write(string.format("BASSNOTE %d: %s passing\n", i - 1, clean))
		else
			io.write(string.format("BASSNOTE %d: %s\n", i - 1, clean))
		end
		io.write(string.format("FIGURES %d: %s\n", i - 1, lesson.figures[i]))
		local mel = groups[i]
		io.write(string.format("MELODY %d: %s\n", i - 1, mel ~= "" and mel or "-"))
	end
	io.write("LESSON_END\n")
	io.flush()
end

-- ── QUERY_CHILDREN: emit child slice descriptors for a chunk ─────────────────

local function handle_query_children(hash)
	local path = "chn/" .. hash .. ".txt"
	local f = io.open(path, "rb")
	if not f then
		die("Cannot read %s", path)
	end
	local content = f:read("*a")
	f:close()

	local level = tonumber(content:match("level:%s*(%d+)") or "")
	if not level or level >= 2 then
		io.write("CHILDREN " .. hash .. "\n")
		io.flush()
		return
	end

	local raw = parse_chunk(content)
	local bass = {}
	local passing = {}
	for _, tok in ipairs(raw.bass) do
		local p = is_passing_tok(tok)
		bass[#bass + 1] = p and tok:sub(1, -2) or tok
		passing[#passing + 1] = p
	end
	local groups = group_melody_tokens(raw.bass, raw.melody)
	local melody = {}
	for i = 1, #groups do
		melody[i] = groups[i] ~= "" and groups[i] or "-"
	end

	local N = #bass
	local offsets = build_offsets(bass)
	local step_bars = (level == 0) and 3 or 1
	local child_level = level + 1

	local children = compute_bar_children(
		child_level,
		raw.key,
		raw.time,
		raw.bpm,
		raw.bar,
		raw.title,
		raw.composer,
		bass,
		passing,
		raw.figures,
		melody,
		1,
		N,
		offsets,
		step_bars
	)
	local parts = { hash }
	for _, c in ipairs(children) do
		parts[#parts + 1] = c.hash .. ":" .. (c.s - 1) .. ":" .. (c.e - 1)
	end
	io.write("CHILDREN " .. table.concat(parts, " ") .. "\n")
	io.flush()
end

-- ── native seq-file parsing for level-1 chunk generation ─────────────────────

local function load_lesson(n)
	local path = string.format("seq/%d.txt", n)
	local f = assert(io.open(path, "rb"))
	local content = f:read("*a")
	f:close()
	content = content:gsub("\r\n", "\n"):gsub("\r", "\n")

	local raw = parse_chunk(content)

	-- Detect and strip passing-note marker from bass tokens.
	local bass = {}
	local passing = {}
	for _, tok in ipairs(raw.bass) do
		local p = is_passing_tok(tok)
		bass[#bass + 1] = p and tok:sub(1, -2) or tok
		passing[#passing + 1] = p
	end

	-- Group melody tokens across bass notes.
	local groups = group_melody_tokens(raw.bass, raw.melody)
	local melody = {}
	for i = 1, #groups do
		melody[i] = groups[i] ~= "" and groups[i] or "-"
	end

	return {
		n = n,
		key = raw.key,
		time = raw.time,
		bpm = raw.bpm,
		bar = raw.bar,
		title = raw.title,
		composer = raw.composer,
		bass = bass,
		passing = passing,
		figures = raw.figures,
		melody = melody,
	}
end

-- ── lesson scanning ───────────────────────────────────────────────────────────

local function collect_lessons()
	local lessons = {}
	local pipe = io.popen("ls seq/*.txt 2>/dev/null")
	for name in pipe:lines() do
		local n = name:match("seq/(%d+)%.txt$")
		if n then
			lessons[#lessons + 1] = tonumber(n)
		end
	end
	pipe:close()
	table.sort(lessons)
	return lessons
end

local function scan_and_emit()
	local lessons = collect_lessons()
	if #lessons == 0 then
		die("no lesson files found in seq/")
	end

	os.execute("mkdir -p chn")

	for _, n in ipairs(lessons) do
		io.write("LESSON_NAME " .. n .. "\n")
	end
	io.flush()

	local live_chunks = {}

	for _, n in ipairs(lessons) do
		local lesson = load_lesson(n)
		local N = #lesson.bass
		local offsets = build_offsets(lesson.bass)

		-- ── level-0: full lesson as a chunk ──────────────────────────────────
		local content0 = build_chunk_content(
			lesson.key,
			lesson.title,
			lesson.composer,
			lesson.time,
			lesson.bpm,
			0,
			lesson.bar,
			0,
			lesson.bass,
			lesson.passing,
			lesson.figures,
			lesson.melody,
			1,
			N
		)
		content0 = content0:gsub("\r\n", "\n"):gsub("\r", "\n")
		local hash0 = sha1(content0)
		write_chunk(hash0, content0)
		live_chunks[hash0] = true
		io.write("CHUNK_NAME " .. hash0 .. " 0\n")
		io.flush()

		-- ── level-1: 3-bar chunks; level-2: 1-bar chunks ─────────────────────

		local chunks1 = compute_bar_children(
			1,
			lesson.key,
			lesson.time,
			lesson.bpm,
			lesson.bar,
			lesson.title,
			lesson.composer,
			lesson.bass,
			lesson.passing,
			lesson.figures,
			lesson.melody,
			1,
			N,
			offsets,
			3
		)
		local child1_parts = { hash0 }
		for _, c1 in ipairs(chunks1) do
			live_chunks[c1.hash] = true
			io.write("CHUNK_NAME " .. c1.hash .. " 1 " .. c1.skills .. "\n")
			io.flush()

			local chunks2 = compute_bar_children(
				2,
				lesson.key,
				lesson.time,
				lesson.bpm,
				lesson.bar,
				lesson.title,
				lesson.composer,
				lesson.bass,
				lesson.passing,
				lesson.figures,
				lesson.melody,
				c1.s,
				c1.e,
				offsets,
				1
			)
			local child2_parts = { c1.hash }
			for _, c2 in ipairs(chunks2) do
				live_chunks[c2.hash] = true
				io.write("CHUNK_NAME " .. c2.hash .. " 2 " .. c2.skills .. "\n")
				io.flush()
				child2_parts[#child2_parts + 1] = c2.hash .. ":" .. (c2.s - 1) .. ":" .. (c2.e - 1)
			end
			io.write("CHILDREN " .. table.concat(child2_parts, " ") .. "\n")
			io.flush()

			child1_parts[#child1_parts + 1] = c1.hash .. ":" .. (c1.s - 1) .. ":" .. (c1.e - 1)
		end
		io.write("CHILDREN " .. table.concat(child1_parts, " ") .. "\n")
		io.flush()
	end

	-- ── stale-file check ──────────────────────────────────────────────────────
	local stale = {}
	local ls = io.popen("ls chn/*.txt 2>/dev/null")
	for fpath in ls:lines() do
		local hash = fpath:match("chn/(%x+)%.txt$")
		if hash and not live_chunks[hash] then
			stale[#stale + 1] = hash
		end
	end
	ls:close()
	if #stale > 0 then
		table.sort(stale)
		io.stderr:write("Error: stale chunk files in chn/ (not derived from any current lesson):\n")
		for _, h in ipairs(stale) do
			io.stderr:write("  " .. h .. "\n")
		end
		os.exit(1)
	end

	io.write("ALL_SCANNED\n")
	io.flush()
end

-- ── main ──────────────────────────────────────────────────────────────────────

io.stdout:setvbuf("line")

for line in io.lines() do
	line = line:gsub("\r", "")
	if line == "RESCAN" then
		scan_and_emit()
	else
		local hash = line:match("^LOAD_CHUNK (%S+)$")
		if hash then
			load_chunk(hash)
		else
			hash = line:match("^QUERY_CHILDREN (%S+)$")
			if hash then
				handle_query_children(hash)
			end
		end
	end
end
