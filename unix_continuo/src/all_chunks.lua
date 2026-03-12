-- SPDX-License-Identifier: MIT
-- all_chunks.lua --- generate chn/<hash>.txt for every lesson in seq/
-- Copyright (c) 2026 Jakob Kastelic
--
-- Usage: lua src/all_chunks.lua
-- Runs bin/load | lua src/chunk.lua for every seq/<n>.txt and writes the
-- resulting chunk files to chn/.  Idempotent: existing files with matching
-- content are skipped; mismatched content on the same hash is fatal.

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
	-- gmatch above produces one extra empty entry at the end; remove it
	if t[#t] == "" then
		t[#t] = nil
	end
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
	for _, tok in ipairs(bassline) do
		lines[#lines + 1] = "  " .. tok
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	lines[#lines + 1] = "figures = {"
	for _, fig in ipairs(figures) do
		lines[#lines + 1] = "  " .. fig
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	lines[#lines + 1] = "melody = {"
	for _, mel in ipairs(melody) do
		if mel ~= "-" then
			lines[#lines + 1] = "  " .. mel
		end
	end
	lines[#lines + 1] = "}"
	lines[#lines + 1] = ""
	return table.concat(lines, "\n")
end

-- Parse one CHUNK line; return a table of fields or nil on failure.
local function parse_chunk_line(line)
	local hash = line:match("HASH:(%x+)")
	local key = line:match("KEY:(%S+)")
	-- SKILLS may be multi-word (e.g. "root 6"); capture up to next keyword
	local skills = line:match("SKILLS:(.-) BASSLINE:")
	local bassline_str = line:match("BASSLINE:(%S+)")
	local figure_str = line:match("FIGURE:(%S+)")
	local melody_str = line:match("MELODY:(.+)$")
	if not (hash and key and skills and bassline_str and figure_str and melody_str) then
		return nil
	end
	return {
		hash = hash,
		key = key,
		skills = skills,
		bassline = split(bassline_str, "|"),
		figures = split(figure_str, "|"),
		melody = split(melody_str, "|"),
	}
end

-- Collect lesson numbers from seq/*.txt
local lessons = {}
do
	local pipe = io.popen("ls seq/*.txt 2>/dev/null")
	for name in pipe:lines() do
		local n = name:match("seq/(%d+)%.txt$")
		if n then
			lessons[#lessons + 1] = tonumber(n)
		end
	end
	pipe:close()
	table.sort(lessons)
end

if #lessons == 0 then
	die("no lesson files found in seq/")
end

os.execute("mkdir -p chn")

local written, skipped = 0, 0

for _, n in ipairs(lessons) do
	local cmd = string.format("printf 'LOAD_LESSON %d\\n' | bin/load | lua src/chunk.lua", n)
	local pipe = io.popen(cmd)
	for line in pipe:lines() do
		if line:match("^CHUNK ") then
			local f = parse_chunk_line(line)
			if not f then
				io.stderr:write(string.format("warning: could not parse CHUNK line from lesson %d\n", n))
			else
				local content = build_content(f.key, f.skills, f.bassline, f.figures, f.melody)
				local computed = sha1(content)
				if computed ~= f.hash then
					die(
						"hash mismatch for chunk from lesson %d: computed %s, chunk.lua said %s",
						n,
						computed,
						f.hash
					)
				end
				if write_chunk(f.hash, content) then
					written = written + 1
				else
					skipped = skipped + 1
				end
			end
		end
	end
	pipe:close()
end

io.stderr:write(string.format("done: %d written, %d skipped\n", written, skipped))
