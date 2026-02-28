-- SPDX-License-Identifier: MIT
-- tst.lua --- run regression tests for unix_continuo
-- Copyright (c) 2026 Jakob Kastelic

-- This script discovers and runs program regression tests located in
-- the same directory as the script.  A valid test consists of a pair:
--
--     progname_N_in.txt
--     progname_N_out.txt
--
-- where:
--   - progname matches an executable in ../bin/
--   - N is a positive integer test number
--
-- For each valid test, the script feeds the _in.txt file to
-- ../bin/progname via stdin and compares stdout with the corresponding
-- _out.txt file using diff.
--
-- Tests are executed in deterministic order:
--   1. Alphabetically by program name
--   2. Numerically by test number
--
-- If output matches, the script prints:
--     OK progname_N     (in green)
--
-- If output differs, it prints:
--     FAIL progname_N   (in red)
--
-- Files that do not begin with the name of an executable in ../bin/
-- are ignored.  If a file begins with a valid program name but does
-- not follow the required naming convention, the script exits with an
-- error.
--
-- The script returns nonzero if any test fails or if a malformed test
-- filename is encountered.

local RED   = "\27[31m"
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
   if f then f:close() return true end
   return false
end

local function extract_number(prog, name)
   local num = name:match("^" .. prog:gsub("%-", "%%-") .. "_(%d+)_in%.txt$")
   return num
end

local function collect_cases()
   local cases = {}
   local handle = io.popen("ls -1 " .. TST_DIR)
   if not handle then die("Cannot list test directory: " .. TST_DIR) end

   local files = {}
   for line in handle:lines() do
      files[#files + 1] = line
   end
   handle:close()

   for _, name in ipairs(files) do
      if name:match("_in%.txt$") then
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

local function run_case(prog, num_str)
   local in_file  = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_in.txt"
   local out_file = TST_DIR .. "/" .. prog .. "_" .. num_str .. "_out.txt"
   local label    = prog .. "_" .. num_str

   if not file_exists(out_file) then
      io.stderr:write(RED .. "FAIL" .. RESET .. ": Missing output file: " .. label .. "_out.txt\n")
      status = 1
      return
   end

   -- Logic to determine the execution command
   local bin_path = BIN_DIR .. "/" .. prog
   local src_path = SRC_DIR .. "/" .. prog .. ".lua"
   local exec_cmd

   if file_exists(bin_path) then
      exec_cmd = bin_path
   elseif file_exists(src_path) then
      exec_cmd = "lua " .. src_path
   else
      io.stderr:write(RED .. "FAIL" .. RESET .. ": No executable or source found for " .. label .. "\n")
      status = 1
      return
   end

   -- Construct and run the command
   local cmd = exec_cmd
   .. " < " .. in_file
   .. " | diff -u - " .. out_file
   .. " >/dev/null 2>&1"

   local ok = os.execute(cmd)
   if ok == true or ok == 0 then
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

   for _, c in ipairs(cases) do
      run_case(c.prog, c.num_str)
   end
end

main()
os.exit(status)
