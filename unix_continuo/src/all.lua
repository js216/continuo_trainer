-- SPDX-License-Identifier: MIT
-- all.lua --- build chunk files and serve as a persistent lesson/chunk index
-- Copyright (c) 2026 Jakob Kastelic
--
-- DESCRIPTION
--      On startup (and on every RESCAN command) the script:
--        1. Scans seq/*.txt and emits one LESSON_NAME line per lesson in
--           ascending numeric order.
--        2. Runs bin/load | lua src/chunk.lua for every lesson, writes any
--           new chn/<hash>.txt files (idempotent; hash collision is fatal),
--           and emits one CHUNK_NAME line per chunk.
--      After the initial scan the script blocks on stdin until EOF.  Connect
--      a long-lived process (e.g. bin/gui) to keep it alive.
--
-- INPUT
--      RESCAN      Repeat the full scan-and-emit cycle.
--      (all other lines are silently ignored)
--
-- OUTPUT
--      LESSON_NAME <n>        One per lesson, in ascending numeric order.
--      CHUNK_NAME <hash>      One per unique chunk, in lesson order.

local function die(fmt, ...)
	io.stderr:write(string.format("Error: " .. fmt, ...) .. "\n")
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

local function write_chunk(hash, content)
	local path = "chn/" .. hash .. ".txt"
	local existing = io.open(path, "rb")
	if existing then
		local old = existing:read("*a")
		existing:close()
		if old ~= content then
			die("hash collision on %s", path)
		end
		return false -- already existed
	end
	local f = assert(io.open(path, "wb"))
	f:write(content)
	f:close()
	return true -- newly written
end

-- Split s on literal separator char (single char).
local function split(s, sep)
	local t = {}
	local pat = "([^" .. sep .. "]*)" .. sep .. "?"
	for tok in (s .. sep):gmatch(pat) do
		t[#t + 1] = tok
	end
	if t[#t] == "" then t[#t] = nil end
	return t
end

-- Reconstruct chunk file content from parsed CHUNK line fields.
-- Must produce byte-for-byte identical output to chunk.lua's build_chunk_content.
local function build_content(key, skills, bassline, figures, melody)
	local lines = {}
	lines[#lines + 1] = "key: " .. key
	lines[#lines + 1] = "skills: " .. skills
	lines[#lines + 1] = ""
	lines[#lines + 1] = "bassline = {"
	for _, tok in ipairs(bassline) do lines[#lines + 1] = "  " .. tok end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	lines[#lines + 1] = "figures = {"
	for _, fig in ipairs(figures) do lines[#lines + 1] = "  " .. fig end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	lines[#lines + 1] = "melody = {"
	for _, mel in ipairs(melody) do
		if mel ~= "-" then lines[#lines + 1] = "  " .. mel end
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	return table.concat(lines, "\n")
end

-- Parse one CHUNK line; return a table of fields or nil on failure.
local function parse_chunk_line(line)
	local hash        = line:match("HASH:(%x+)")
	local key         = line:match("KEY:(%S+)")
	local skills      = line:match("SKILLS:(.-) BASSLINE:")
	local bassline_str = line:match("BASSLINE:(%S+)")
	local figure_str  = line:match("FIGURE:(%S+)")
	local melody_str  = line:match("MELODY:(.+)$")
	if not (hash and key and skills and bassline_str and figure_str and melody_str) then
		return nil
	end
	return {
		hash     = hash,
		key      = key,
		skills   = skills,
		bassline = split(bassline_str, "|"),
		figures  = split(figure_str,  "|"),
		melody   = split(melody_str,  "|"),
	}
end

local function collect_lessons()
	local lessons = {}
	local pipe = io.popen("ls seq/*.txt 2>/dev/null")
	for name in pipe:lines() do
		local n = name:match("seq/(%d+)%.txt$")
		if n then lessons[#lessons + 1] = tonumber(n) end
	end
	pipe:close()
	table.sort(lessons)
	return lessons
end

local function scan_and_emit()
	local lessons = collect_lessons()
	if #lessons == 0 then die("no lesson files found in seq/") end

	os.execute("mkdir -p chn")

	-- Emit lesson index
	for _, n in ipairs(lessons) do
		io.write("LESSON_NAME " .. n .. "\n")
	end
	io.flush()

	-- Emit chunk index (writing new files as a side-effect)
	for _, n in ipairs(lessons) do
		local cmd = string.format("printf 'LOAD_LESSON %d\\n' | bin/load | lua src/chunk.lua", n)
		local pipe = io.popen(cmd)
		for line in pipe:lines() do
			if line:match("^CHUNK ") then
				local f = parse_chunk_line(line)
				if not f then
					io.stderr:write(string.format(
						"warning: could not parse CHUNK line from lesson %d\n", n))
				else
					local content = build_content(f.key, f.skills, f.bassline, f.figures, f.melody)
					local computed = sha1(content)
					if computed ~= f.hash then
						die("hash mismatch for chunk from lesson %d: computed %s, chunk.lua said %s",
							n, computed, f.hash)
					end
					write_chunk(f.hash, content)
					io.write("CHUNK_NAME " .. f.hash .. "\n")
					io.flush()
				end
			end
		end
		pipe:close()
	end
end

-- ── main ─────────────────────────────────────────────────────────────────────

io.stdout:setvbuf("line")

scan_and_emit()

for line in io.lines() do
	line = line:gsub("\r$", "")
	if line == "RESCAN" then
		scan_and_emit()
	end
end
