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
--        3. Checks that chn/ contains no files that were not produced by
--           the current lesson set.  Any such stale file is reported to
--           stderr and the script exits with code 1.
--        4. Emits ALL_SCANNED to signal a clean, complete scan.
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
--      ALL_SCANNED            Emitted once after a clean scan (no stale files).

local function die(fmt, ...)
	io.stderr:write(string.format("Error: " .. fmt, ...) .. "\n")
	os.exit(1)
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

	-- Emit chunk index; chunk.lua writes the chn/ files as a side-effect
	local live_chunks = {} -- hash -> true for every chunk derived from current lessons
	for _, n in ipairs(lessons) do
		local cmd = string.format("printf 'LOAD_LESSON %d\\n' | bin/load | lua src/chunk.lua", n)
		local pipe = io.popen(cmd)
		for line in pipe:lines() do
			local hash = line:match("^CHUNK .+ HASH:(%x+)")
			if hash then
				live_chunks[hash] = true
				io.write("CHUNK_NAME " .. hash .. "\n")
				io.flush()
			end
		end
		pipe:close()
	end

	-- Check for stale chunk files (in chn/ but not derived from any current lesson)
	local stale = {}
	local ls = io.popen("ls chn/*.txt 2>/dev/null")
	for path in ls:lines() do
		local hash = path:match("chn/(%x+)%.txt$")
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

-- ── main ─────────────────────────────────────────────────────────────────────

io.stdout:setvbuf("line")

scan_and_emit()

for line in io.lines() do
	line = line:gsub("\r", "")
	if line == "RESCAN" then
		scan_and_emit()
	end
end
