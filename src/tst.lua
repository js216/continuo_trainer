-- SPDX-License-Identifier: MIT
-- tst.lua --- run regression tests for unix_continuo
-- Copyright (c) 2026 Jakob Kastelic

-- DESCRIPTION
--      This script discovers and runs program regression tests located in
--      the same directory as the script. A valid test consists of:
--
--          progname_N_in.txt   (required) stdin fed to the program
--          progname_N_out.txt  (required) expected stdout
--          progname_N_arg.txt  (optional) passed as first CLI arg (e.g. a
--                              stats log); copied to a temp file before the
--                              run so the original is never mutated.
--          progname_N_arg_in.txt   alternative to _arg.txt; if present,
--                                  takes precedence over _arg.txt.
--          progname_N_arg_out.txt  expected content of the arg file after
--                                  the run (only checked when _arg_in.txt
--                                  is used); useful for testing programs
--                                  that persist state (e.g. stats.lua).
--
--      where:
--      - progname matches an executable in ../bin/ or a .lua file in ../src/
--      - N is a positive integer test number
--
--      For each valid test, the script:
--        (1) checks all _in.txt and _arg_in.txt files have Unix line endings
--            and reports any CRLF files as failures before running anything;
--        (2) runs each test with the files as-is (Unix endings);
--        (3) runs each test again with input converted to DOS endings on the
--            fly via sed, verifying the program produces identical output.
--
-- EXECUTION ORDER
--      Tests are executed in deterministic order:
--      1. Alphabetically by program name
--      2. Numerically by test number
--
-- OUTPUT FORMAT
--      If output matches, the script prints:
--          OK progname_N         (Unix pass, in green)
--          OK progname_N [dos]   (DOS pass, in green)
--
--      If output differs, it prints:
--          FAIL progname_N       (in red)
--
-- ERROR HANDLING
--      Files that do not begin with the name of an executable in ../bin/
--      are ignored. If a file begins with a valid program name but does
--      not follow the required naming convention, the script exits with
--      an error.
--
-- EXIT STATUS
--      The script returns nonzero if any test fails or if a malformed
--      test filename is encountered.

local RED = "\27[31m"
local GREEN = "\27[32m"
local RESET = "\27[0m"

local status = 0

local function usage()
	die("Usage: lua tst.lua TST_DIR BIN_DIR SRC_DIR")
end

local function die(msg)
	io.stderr:write(msg .. "\n")
	os.exit(1)
end

local function file_exists(path)
	local f = io.open(path, "r")
	if f then
		f:close()
		return true
	end
	return false
end

local function has_crlf(path)
	local f = io.open(path, "rb")
	if not f then
		return false
	end
	local content = f:read("*a")
	f:close()
	return content:find("\r\n") ~= nil
end

local function extract_number(prog, name)
	local num = name:match("^" .. prog:gsub("%-", "%%-") .. "_(%d+)_in%.txt$")
	return num
end

local function collect_cases()
	local cases = {}
	local handle = io.popen("ls -1 " .. TST_DIR)
	if not handle then
		die("Cannot list test directory: " .. TST_DIR)
	end

	local files = {}
	for line in handle:lines() do
		files[#files + 1] = line
	end
	handle:close()

	for _, name in ipairs(files) do
		if name:match("_in%.txt$") and not name:match("_arg_in%.txt$") then
			local prog = name:match("^([^_]+)")
			if prog then
				local num = extract_number(prog, name)
				if not num then
					die("Malformed test filename: " .. name)
				end
				cases[#cases + 1] = { prog = prog, num = tonumber(num), num_str = num }
			end
		end
	end
	return cases
end

-- (1) Check all input files have Unix endings.
local function check_line_endings(cases)
	local bad = {}
	for _, c in ipairs(cases) do
		local in_file = TST_DIR .. "/" .. c.prog .. "_" .. c.num_str .. "_in.txt"
		local arg_file = TST_DIR .. "/" .. c.prog .. "_" .. c.num_str .. "_arg_in.txt"
		if has_crlf(in_file) then
			bad[#bad + 1] = in_file
		end
		if file_exists(arg_file) and has_crlf(arg_file) then
			bad[#bad + 1] = arg_file
		end
	end
	if #bad > 0 then
		for _, f in ipairs(bad) do
			print(RED .. "FAIL" .. RESET .. " line-endings: " .. f .. " has CRLF — run: sed -i 's/\\r//' " .. f)
		end
		status = 1
	end
end

local function resolve_exec(prog, label)
	local bin_path = BIN_DIR .. "/" .. prog
	local src_path = SRC_DIR .. "/" .. prog .. ".lua"
	if file_exists(bin_path) then
		return bin_path
	end
	if file_exists(src_path) then
		return "lua " .. src_path
	end
	io.stderr:write(RED .. "FAIL" .. RESET .. ": No executable or source found for " .. label .. "\n")
	status = 1
	return nil
end

-- (2)/(3) Run one test case, optionally converting input to DOS on the fly.
local function run_case(prog, num_str, dos_mode)
	local in_file = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_in.txt"
	local out_file = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_out.txt"
	local label = prog .. "_" .. num_str .. (dos_mode and " [dos]" or "")

	if not file_exists(out_file) then
		io.stderr:write(RED .. "FAIL" .. RESET .. ": Missing output file: " .. out_file .. "\n")
		status = 1
		return
	end

	local exec_cmd = resolve_exec(prog, label)
	if not exec_cmd then
		return
	end

	local arg_in_file = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_arg_in.txt"
	local arg_file = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_arg.txt"
	local arg_out_file = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_arg_out.txt"
	local tmp_arg = nil
	local extra_arg = ""
	local check_arg_out = false

	if file_exists(arg_in_file) then
		tmp_arg = os.tmpname()
		if dos_mode then
			os.execute("sed 's/$/\\r/' " .. arg_in_file .. " > " .. tmp_arg)
		else
			os.execute("cp " .. arg_in_file .. " " .. tmp_arg)
		end
		extra_arg = " " .. tmp_arg
		check_arg_out = file_exists(arg_out_file)
	elseif file_exists(arg_file) then
		tmp_arg = os.tmpname()
		os.execute("cp " .. arg_file .. " " .. tmp_arg)
		extra_arg = " " .. tmp_arg
	end

	-- Feed stdin through sed to add \r when in DOS mode.
	local stdin_cmd
	if dos_mode then
		stdin_cmd = "sed 's/$/\\r/' " .. in_file
	else
		stdin_cmd = "cat " .. in_file
	end

	local cmd = stdin_cmd .. " | " .. exec_cmd .. extra_arg .. " | diff -u - " .. out_file .. " >/dev/null 2>&1"

	local ok = os.execute(cmd)
	local arg_ok = true
	if check_arg_out and tmp_arg then
		-- arg_out is always Unix; strip \r from the temp file before diffing.
		local stripped = os.tmpname()
		os.execute("sed 's/\\r//' " .. tmp_arg .. " > " .. stripped)
		local r = os.execute("diff -u " .. stripped .. " " .. arg_out_file .. " >/dev/null 2>&1")
		arg_ok = (r == true or r == 0)
		os.remove(stripped)
	end
	if tmp_arg then
		os.remove(tmp_arg)
	end

	if (ok == true or ok == 0) and arg_ok then
		print(GREEN .. "OK" .. RESET .. " " .. label)
	else
		print(RED .. "FAIL" .. RESET .. " " .. label)
		status = 1
	end
end

local function main()
	TST_DIR = arg[1] or usage()
	BIN_DIR = arg[2] or usage()
	SRC_DIR = arg[3] or usage()

	local cases = collect_cases()

	table.sort(cases, function(a, b)
		if a.prog ~= b.prog then
			return a.prog < b.prog
		end
		return a.num < b.num
	end)

	-- (1) Line-ending check
	check_line_endings(cases)

	-- (2) Unix pass
	for _, c in ipairs(cases) do
		run_case(c.prog, c.num_str, false)
	end

	-- (3) DOS pass
	for _, c in ipairs(cases) do
		run_case(c.prog, c.num_str, true)
	end
end

main()
os.exit(status)
