-- SPDX-License-Identifier: MIT
-- rules.lua --- elegant voice-leading validation for figured bass
-- Copyright (c) 2026 Jakob Kastelic
-- Lua 5.4 translation of rules.rs

-- DESCRIPTION
--     rules is a filter that reads a stream of realized musical groups from
--     standard input and evaluates them against voice-leading and harmonic
--     rules.  It uses a sliding window of the last 4 groups to check
--     transitions between chords.  For each GROUP line, the program writes
--     a RESULT line to standard output indicating whether it passes (OK) or
--     fails (FAIL), with ANSI color highlighting.
--
--     A LESSON line resets the key signature and the sliding window; the key
--     token (3rd whitespace-separated field, e.g. "C", "g", "Bb", "f#") is
--     parsed to derive the diatonic pitch-class set used by harmonic rules.
--     All other unrecognized lines are silently ignored.
--
-- INPUT FORMAT
--     The program processes two kinds of lines:
--
--     LESSON <name> <key>
--         Resets context.  <key> is a LilyPond-style key: uppercase = major,
--         lowercase = minor; accidentals via #/b or is/es/as suffixes
--         (e.g. "Fis" = F# major, "bes" = Bb minor).
--
--     GROUP <field> ...
--         Describes one realized chord.  Fields are space-separated tokens:
--
--         ID:<n>
--             Decimal integer identifier echoed verbatim in RESULT output.
--
--         passing
--             Optional bare keyword.  Marks the group as a passing chord;
--             most harmonic rules are skipped for passing groups (parallel-
--             motion and bass-tritone-leap rules still fire for them).
--
--         BASS:<pitch>
--             The notated (written) bass pitch in LilyPond syntax.  Used to
--             detect chromatic alterations and the special sentinel value "?"
--             which marks a group past the end of the lesson.
--
--         BASS_ACTUAL:<pitch>
--             The sounding bass pitch used for all interval calculations.
--
--         FIGURES:<spec>
--             Figured-bass annotation, slash- or comma-separated
--             (e.g. "6", "6/4", "b7", "#").  Empty or "0" means a plain
--             triad (3/5/8).  A lone "3" expands to 3/5/8; a lone "4"
--             expands to 4/5/8; a lone "6" prepends an implicit 3.
--             Accidentals: #/##/b/bb/n or is/isis/es/eses/as prefixes.
--
--         REALIZATION:<p1>/<p2>/...
--             Slash-separated LilyPond pitches for the inner voices, sorted
--             ascending by semitone.
--
--         MELODY:<pitch> [<pitch> ...]
--             One or more LilyPond pitches for the melody.  Multiple pitches
--             may follow as separate tokens until the next "key:value" token.
--
--         TIME:<ms>
--             Onset time in milliseconds.
--
-- PITCH SYNTAX
--     All pitches use LilyPond notation: note letter (c d e f g a b),
--     optional accidental suffix (is = sharp, es/as = flat, isis = double-
--     sharp, eses = double-flat), and octave marks (' to go up, , to go
--     down, relative to c' = MIDI 60).  Trailing digits and dots (duration
--     info) are stripped before parsing.
--
-- VALIDATION RULES
--     Rules are applied in order; the first failure stops evaluation of that
--     group.  Rules marked [all] fire even for passing chords; others are
--     skipped when the "passing" keyword is present.
--
--     Not Past End  [all]
--         Fails if BASS is the sentinel value "?", indicating the group lies
--         beyond the end of the lesson definition.
--
--     Parallel Fifths / Octaves  [all]
--         Across consecutive non-passing groups, checks every pair of voices
--         (bass, inner voices sorted ascending, melody) for parallel motion
--         into a perfect fifth (7 semitones mod 12) or unison/octave (0
--         semitones mod 12).  Duplicate pitch-classes shared by bass or
--         melody are collapsed before comparison.
--
--     Bass Tritone Leap  [all]
--         The bass may not move by exactly 6 semitones (mod 12) between
--         consecutive groups.
--
--     Realization Not Empty
--         At least one inner-voice pitch must be provided.
--
--     Realization Correctness
--         Every inner-voice pitch-class must belong to the set derived from
--         the figured-bass annotation.  Exception: if the bass pitch-class is
--         unchanged from the previous group (held bass), all pitch-classes
--         present in the previous group's inner voices and melody are also
--         admitted, to allow suspension resolution.
--
--     Bass In Key
--         The sounding bass pitch-class must be diatonic to the current key.
--         This check is suppressed when the notated and sounding bass share
--         the same pitch-class (i.e. no chromatic alteration has occurred).
--
--     Realization Complete
--         Every pitch-class required by the figured-bass annotation must
--         appear somewhere in the combined set of bass, inner voices, and
--         melody.
--
-- OUTPUT FORMAT
--     For every GROUP line the program writes one line to standard output:
--
--         RESULT <id> OK
--             All rules passed (printed in green via ANSI escape codes).
--
--         RESULT <id> FAIL <message>
--             A rule was violated; <message> describes the first failure
--             (printed in red via ANSI escape codes).
--
-- EXIT STATUS
--     Always returns 0.

local window = {}
local key = { scale_pcs = {} }

-- ---------------------------------------------------------------------------
-- Pitch helpers
-- ---------------------------------------------------------------------------

local function to_semitone(p)
	if not p then
		return nil
	end
	-- strip trailing digits and dots
	p = p:match("^(.-)%s*$")
	p = p:gsub("[%d%.]+$", "")
	if p == "" then
		return nil
	end

	local first = p:sub(1, 1):lower()
	local rest = p:sub(2)

	local acc, oct_part
	if rest:sub(1, 4) == "isis" then
		acc, oct_part = 2, rest:sub(5)
	elseif rest:sub(1, 4) == "eses" then
		acc, oct_part = -2, rest:sub(5)
	elseif rest:sub(1, 2) == "is" then
		acc, oct_part = 1, rest:sub(3)
	elseif rest:sub(1, 2) == "es" or rest:sub(1, 2) == "as" then
		acc, oct_part = -1, rest:sub(3)
	else
		acc, oct_part = 0, rest
	end

	local octaves = 0
	for c in oct_part:gmatch(".") do
		if c == "'" then
			octaves = octaves + 12
		elseif c == "," then
			octaves = octaves - 12
		end
	end

	local base_map = { c = 0, d = 2, e = 4, f = 5, g = 7, a = 9, b = 11 }
	local base = base_map[first]
	if not base then
		return nil
	end

	return base + acc + octaves + 48 -- c' = 60
end

local PC_NAMES = { "c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "ais", "b" }

local function pc_name(semitone)
	local idx = semitone % 12
	if idx < 0 then
		idx = idx + 12
	end
	return PC_NAMES[idx + 1] or "?"
end

local function rem_euclid(a, b)
	local r = a % b
	if r < 0 then
		r = r + b
	end
	return r
end

local function signum(x)
	if x > 0 then
		return 1
	elseif x < 0 then
		return -1
	else
		return 0
	end
end

-- Format a semitone value as a LilyPond-style pitch string with octave marks.
local function pitch_name(semitone)
	local NOTE_NAMES = { "c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "ais", "b" }
	local pc = semitone % 12
	if pc < 0 then
		pc = pc + 12
	end
	local name = NOTE_NAMES[pc + 1] or "?"
	-- c' = 60; each 12 semitones is one octave mark
	local octave = math.floor(semitone / 12) - 4 -- relative to c' octave
	local marks = ""
	if octave > 0 then
		marks = string.rep("'", octave)
	elseif octave < 0 then
		marks = string.rep(",", -octave)
	end
	return name .. marks
end

-- Collapse inner voices that duplicate the bass or melody pitch-class, then
-- append the melody.  Returns a list of {semitone, role} where role is
-- "bass", "inner_N" (1-based original index), or "melody".  Roles allow
-- find_parallel to check only same-voice pairs across consecutive groups.
local function flatten_voices(g)
	local voices = { { semitone = g.bass, role = "bass" } }
	local mel = g.melody[1]
	local b_pc = g.bass % 12
	local m_pc = mel and (mel % 12) or nil
	for idx, n in ipairs(g.inner) do
		local n_pc = n % 12
		if n_pc ~= m_pc and n_pc ~= b_pc then
			voices[#voices + 1] = { semitone = n, role = "inner_" .. idx }
		end
	end
	if mel then
		voices[#voices + 1] = { semitone = mel, role = "melody" }
	end
	return voices
end

-- ---------------------------------------------------------------------------
-- Key signature parsing
-- ---------------------------------------------------------------------------

local MAJOR_STEPS = { 0, 2, 4, 5, 7, 9, 11 }
local MINOR_STEPS = { 0, 2, 3, 5, 7, 8, 10 }

local function parse_key(token)
	if not token or token == "" then
		return { scale_pcs = {} }
	end

	local first = token:sub(1, 1)
	local is_minor = (first == first:lower())
	local up = first:upper()

	local root_base_map = { C = 0, D = 2, E = 4, F = 5, G = 7, A = 9, B = 11 }
	local root_base = root_base_map[up]
	if not root_base then
		return { scale_pcs = {} }
	end

	local rest = token:sub(2)
	local acc_map = {
		["#"] = 1,
		["is"] = 1,
		["##"] = 2,
		["isis"] = 2,
		["b"] = -1,
		["es"] = -1,
		["as"] = -1,
		["bb"] = -2,
		["eses"] = -2,
	}
	local acc = acc_map[rest] or 0

	local root = rem_euclid(root_base + acc, 12)
	local steps = is_minor and MINOR_STEPS or MAJOR_STEPS
	local scale_pcs = {}
	for _, s in ipairs(steps) do
		scale_pcs[#scale_pcs + 1] = (root + s) % 12
	end
	return { scale_pcs = scale_pcs }
end

-- ---------------------------------------------------------------------------
-- Figure parsing
-- ---------------------------------------------------------------------------

-- Split a figure string on '/' or ',' but treat '\/' and '\\' as literals.
local function split_figure_tokens(s)
	local parts = {}
	local cur = ""
	local i = 1
	while i <= #s do
		if s:sub(i, i + 1) == "\\\\" then
			cur = cur .. "\\\\"
			i = i + 2
		elseif s:sub(i, i + 1) == "\\/" then
			cur = cur .. "\\/"
			i = i + 2
		elseif s:sub(i, i) == "/" or s:sub(i, i) == "," then
			if cur ~= "" then parts[#parts + 1] = cur end
			cur = ""
			i = i + 1
		else
			cur = cur .. s:sub(i, i)
			i = i + 1
		end
	end
	if cur ~= "" then parts[#parts + 1] = cur end
	return parts
end

local function parse_one_figure(t)
	t = t:match("^%s*(.-)%s*$")
	if t == "" then
		return nil
	end

	local acc, rest
	if t:sub(1, 2) == "##" then
		acc, rest = 2, t:sub(3)
	elseif t:sub(1, 2) == "bb" then
		acc, rest = -2, t:sub(3)
	elseif t:sub(1, 1) == "#" then
		acc, rest = 1, t:sub(2)
	elseif t:sub(1, 1) == "b" and (t:len() == 1 or t:sub(2, 2):match("%d")) then
		acc, rest = -1, t:sub(2)
	elseif t:sub(1, 1) == "n" then
		acc, rest = 0, t:sub(2)
	elseif t:sub(1, 4) == "isis" then
		acc, rest = 2, t:sub(5)
	elseif t:sub(1, 4) == "eses" then
		acc, rest = -2, t:sub(5)
	elseif t:sub(1, 2) == "is" then
		acc, rest = 1, t:sub(3)
	elseif t:sub(1, 2) == "es" or t:sub(1, 2) == "as" then
		acc, rest = -1, t:sub(3)
	elseif t:sub(-2) == "\\\\" then
		acc, rest = 1, t:sub(1, -3)
	elseif t:sub(-2) == "\\/" then
		acc, rest = 1, t:sub(1, -3)
	elseif t:sub(-1) == "+" then
		acc, rest = 1, t:sub(1, -2)
	elseif t:sub(1, 1) == "+" then
		acc, rest = 1, t:sub(2)
	else
		acc, rest = 0, t
	end

	local deg
	if rest == "" then
		deg = 3
	else
		deg = tonumber(rest)
		if not deg then
			return nil
		end
	end
	return { deg = deg, acc = acc }
end

local function default_triad()
	return { { deg = 3, acc = 0 }, { deg = 5, acc = 0 }, { deg = 8, acc = 0 } }
end

local function parse_figures(s)
	s = s and s:match("^%s*(.-)%s*$") or ""
	if s == "" or s == "0" then
		return default_triad()
	end

	local figs = {}
	for _, part in ipairs(split_figure_tokens(s)) do
		local fig = parse_one_figure(part)
		if fig then
			figs[#figs + 1] = fig
		end
	end

	if #figs == 0 then
		return default_triad()
	end

	if #figs == 1 and figs[1].deg == 3 then
		figs[#figs + 1] = { deg = 5, acc = 0 }
		figs[#figs + 1] = { deg = 8, acc = 0 }
	end

	if #figs == 1 and figs[1].deg == 4 then
		figs[#figs + 1] = { deg = 5, acc = 0 }
		figs[#figs + 1] = { deg = 8, acc = 0 }
	end

	if #figs == 1 and figs[1].deg == 5 then
		figs[#figs + 1] = { deg = 3, acc = 0 }
		figs[#figs + 1] = { deg = 8, acc = 0 }
	end

	if #figs == 1 and figs[1].deg == 6 then
		table.insert(figs, 1, { deg = 3, acc = 0 })
	end

	if #figs == 1 and figs[1].deg == 7 then
		figs[#figs + 1] = { deg = 5, acc = 0 }
		figs[#figs + 1] = { deg = 3, acc = 0 }
	end

	table.sort(figs, function(a, b)
		return a.deg < b.deg
	end)

	-- dedup by deg
	local deduped = { figs[1] }
	for k = 2, #figs do
		if figs[k].deg ~= figs[k - 1].deg then
			deduped[#deduped + 1] = figs[k]
		end
	end
	return deduped
end

-- Semitone values of the natural (unaltered) letter names, in scale-degree order.
local NATURAL_PCS = { 0, 2, 4, 5, 7, 9, 11 } -- C D E F G A B

local function figure_to_pc(fig, bass_pc, bass_letter_idx, key)
	local deg = ((fig.deg - 1) % 7) + 1
	if fig.acc ~= 0 then
		-- Accidental present: apply it to the natural (white-key) note at
		-- degree `deg` above the bass letter, ignoring the key signature.
		-- e.g. "#" above D (bass_pc=2): natural 3rd above D is F (pc 5),
		-- +1 semitone = F# (pc 6).  We find the bass letter by locating its
		-- natural pc in NATURAL_PCS, then stepping `deg-1` letters up.
		local bass_natural_idx = nil
		local bass_natural_pc = rem_euclid(bass_pc, 12)
		for i, np in ipairs(NATURAL_PCS) do
			if np == bass_natural_pc then
				bass_natural_idx = i
				break
			end
		end
		if bass_natural_idx == nil then
			-- Bass is altered (e.g. fis); use the letter index passed in.
			bass_natural_idx = bass_letter_idx
		end
		if bass_natural_idx == nil then
			goto key_based
		end
		local target_idx = ((bass_natural_idx - 1 + (deg - 1)) % 7) + 1
		return rem_euclid(NATURAL_PCS[target_idx] + fig.acc, 12)
	end
	::key_based::
	-- No accidental (or altered bass fallback): use the key-adjusted diatonic pitch.
	local bass_deg = nil
	for i, pc in ipairs(key.scale_pcs) do
		if pc == bass_pc then
			bass_deg = i - 1
			break
		end -- 0-based
	end
	if bass_deg == nil then
		return nil
	end
	local target_deg = (bass_deg + (deg - 1)) % 7
	return key.scale_pcs[target_deg + 1]
end

-- ---------------------------------------------------------------------------
-- Set helpers
-- ---------------------------------------------------------------------------

local function set_new(t)
	local s = {}
	if t then
		for _, v in ipairs(t) do
			s[v] = true
		end
	end
	return s
end

local function set_contains(s, v)
	return s[v] == true
end
local function set_insert(s, v)
	s[v] = true
end

-- ---------------------------------------------------------------------------
-- Rules
-- ---------------------------------------------------------------------------

-- Format a parallel-motion violation as "(p_i, p_j) -> (c_i, c_j)".
local function parallel_msg(kind, p_i, p_j, c_i, c_j)
	return string.format(
		"Parallel %s: (%s, %s) -> (%s, %s)",
		kind,
		pitch_name(p_i),
		pitch_name(p_j),
		pitch_name(c_i),
		pitch_name(c_j)
	)
end

-- Core detector: checks every pair of voices that share the same role in
-- both p_voices and c_voices for parallel motion at the given interval.
-- Returns p_i, p_j, c_i, c_j (semitone values) or nil.
local function find_parallel(p_voices, c_voices, interval)
	-- index c_voices by role for O(1) lookup
	local c_by_role = {}
	for _, v in ipairs(c_voices) do
		c_by_role[v.role] = v.semitone
	end

	for vi = 1, #p_voices do
		local pv_i = p_voices[vi]
		local cv_i = c_by_role[pv_i.role]
		if cv_i == nil then
			goto continue_i
		end

		for vj = vi + 1, #p_voices do
			local pv_j = p_voices[vj]
			local cv_j = c_by_role[pv_j.role]
			if cv_j == nil then
				goto continue_j
			end

			local p_int = math.abs(pv_j.semitone - pv_i.semitone) % 12
			local c_int = math.abs(cv_j - cv_i) % 12
			if p_int == interval and c_int == interval then
				local motion_i = cv_i - pv_i.semitone
				local motion_j = cv_j - pv_j.semitone
				if signum(motion_i) == signum(motion_j) and motion_i ~= 0 then
					return pv_i.semitone, pv_j.semitone, cv_i, cv_j
				end
			end
			::continue_j::
		end
		::continue_i::
	end
	return nil
end

-- Shared guard: returns p_voices, c_voices when the window is ready and
-- neither chord is a passing chord; otherwise returns nil.
local function parallel_voices(ctx)
	local window = ctx.window
	if #window < 2 then
		return nil
	end
	local prev = window[#window - 1]
	local curr = window[#window]
	if curr.passing or prev.passing then
		return nil
	end
	return flatten_voices(prev), flatten_voices(curr)
end

local function rule_no_parallel_fifths(ctx)
	local p_voices, c_voices = parallel_voices(ctx)
	if not p_voices then
		return true, nil
	end
	local p_i, p_j, c_i, c_j = find_parallel(p_voices, c_voices, 7)
	if p_i then
		return false, parallel_msg("fifths", p_i, p_j, c_i, c_j)
	end
	return true, nil
end

local function rule_no_parallel_octaves(ctx)
	local p_voices, c_voices = parallel_voices(ctx)
	if not p_voices then
		return true, nil
	end
	local p_i, p_j, c_i, c_j = find_parallel(p_voices, c_voices, 0)
	if p_i then
		return false, parallel_msg("octaves", p_i, p_j, c_i, c_j)
	end
	return true, nil
end

local function rule_bass_leap(ctx)
	local window = ctx.window
	if #window < 2 then
		return true, nil
	end
	local p = window[#window - 1]
	local c = window[#window]
	if math.abs(c.bass - p.bass) % 12 == 6 then
		return false, "Tritone leap in bass"
	end
	return true, nil
end

local function rule_check_realization(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then
		return true, nil
	end
	if g.in_rising_56 then
		return true, nil
	end
	local key = ctx.key
	-- Compute the required chord from the NOTATED bass (what the score
	-- shows), not the played bass — otherwise a wrong bass note would
	-- silently shift all figure-implied pitches and produce confusing
	-- messages like "Figure 3 (gis) is missing" when the score said A
	-- with #3 (meaning C#).  The played bass pitch is admitted as an
	-- inner-voice doubling (users may play the bass above the lowest note).
	local notated_pc = g.bass_notated % 12
	local played_pc = g.bass % 12

	local allowed = set_new({ played_pc })

	for _, fig in ipairs(g.figures) do
		local pc = figure_to_pc(fig, notated_pc, g.bass_notated_letter_idx, key)
		if pc then
			set_insert(allowed, pc)
		else
			return true, nil
		end
	end

	-- suspension resolution: admit prev pitches if bass is held
	if #ctx.window >= 2 then
		local prev = ctx.window[#ctx.window - 1]
		if prev.bass % 12 == played_pc then
			for _, n in ipairs(prev.inner) do
				set_insert(allowed, n % 12)
			end
			for _, n in ipairs(prev.melody) do
				set_insert(allowed, n % 12)
			end
			set_insert(allowed, prev.bass % 12)
		end
	end

	local fig_parts = {}
	for _, f in ipairs(g.figures) do
		local acc_str = (f.acc == 2 and "##")
			or (f.acc == 1 and "#")
			or (f.acc == -1 and "b")
			or (f.acc == -2 and "bb")
			or ""
		fig_parts[#fig_parts + 1] = acc_str .. tostring(f.deg)
	end
	local fig_label = table.concat(fig_parts, "/")

	for _, note in ipairs(g.inner) do
		local pc = note % 12
		if not set_contains(allowed, pc) then
			return false, string.format("Incorrect realization of figure %s: %s", fig_label, pc_name(note))
		end
	end
	return true, nil
end

-- An octave-jump in the bass (same pitch class as the previous group, but
-- a different absolute pitch — e.g. c4 → c2) carries the same harmony and
-- realization as the previous note.  Mark the group as passing so the
-- harmonic rules apply to the previous beat only and the user isn't
-- penalised for "incomplete" right-hand voicing on the jump.  Must run
-- before any rule that consults g.passing.
local function rule_mark_octave_jump(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then return true, nil end
	if #ctx.window < 2 then return true, nil end
	local prev = ctx.window[#ctx.window - 1]
	if g.bass_notated % 12 == prev.bass_notated % 12
		and g.bass_notated ~= prev.bass_notated then
		g.passing = true
	end
	return true, nil
end

local function rule_passing_bass(ctx)
	local g = ctx.window[#ctx.window]
	if not g.passing then return true, nil end
	if g.bass % 12 == g.bass_notated % 12 then return true, nil end
	return false, string.format("Passing bass: expected %s, got %s",
		pc_name(g.bass_notated), pc_name(g.bass))
end

-- The bass the user played must match the notated bass (mod 12) for any
-- non-passing group.  rule_passing_bass covers the passing case with its
-- own message, and rule_bass_in_key catches non-diatonic notes with a more
-- specific "not diatonic" message.  This fills the gap for diatonic-but-
-- wrong notes (e.g. playing E in a chord notated over C in C major), which
-- previously slipped through silently.
local function rule_bass_matches_notated(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then return true, nil end
	if g.bass_notated_raw == "?" then return true, nil end  -- wildcard
	local bass_pc = g.bass % 12
	local notated_pc = g.bass_notated % 12
	if bass_pc == notated_pc then return true, nil end
	-- Defer to rule_bass_in_key for non-diatonic notes so the user gets
	-- the more informative "not diatonic" message.
	for _, pc in ipairs(ctx.key.scale_pcs) do
		if pc == bass_pc then
			return false, string.format("Wrong bass: expected %s, got %s",
				pc_name(g.bass_notated), pc_name(g.bass))
		end
	end
	return true, nil
end

local function rule_bass_in_key(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then
		return true, nil
	end
	local bass_pc = g.bass % 12
	local notated_pc = g.bass_notated % 12

	if notated_pc == bass_pc then
		return true, nil
	end

	local diatonic = false
	for _, pc in ipairs(ctx.key.scale_pcs) do
		if pc == bass_pc then
			diatonic = true
			break
		end
	end
	if not diatonic then
		return false, string.format("Bass note %s is not diatonic to the key", pc_name(g.bass))
	end
	return true, nil
end

local function rule_not_past_end(ctx)
	local g = ctx.window[#ctx.window]
	if g.bass_notated_raw == "?" then
		return false, "Group is past the end of the lesson"
	end
	return true, nil
end

local function rule_realization_not_empty(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then
		return true, nil
	end
	if g.in_rising_56 then
		return true, nil
	end
	if #g.inner == 0 then
		return false, "No realization provided (inner voices are empty)"
	end
	return true, nil
end

-- Check if a group's figures_raw is a bare "7" (possibly with accidental prefix).
local function is_bare_figure_7(g)
	return g.figures_raw:match("^[#bn]*7$") ~= nil
end

local function is_bare_figure_5(g)
	return g.figures_raw:match("^[#bn]*5$") ~= nil
end

local function is_bare_figure_6(g)
	return g.figures_raw:match("^[#bn]*6$") ~= nil
end

-- Deferred completeness check for figure-7 groups: resolved by resolve_pending_7
-- when the next group arrives (to distinguish 7-6 suspensions from standalone 7ths).
local pending_7 = nil -- { group = g, key = key }

-- Check completeness for a non-deferred group.  Required pitches come from
-- the NOTATED bass (score), not the played bass, so a wrong-bass error
-- doesn't mutate the expected chord.  Each figure pc must appear in the
-- RIGHT HAND (g.inner) — the bass providing that pc isn't enough.  Figure
-- "3" means the implicit triad 3/5/8, so right hand needs three voices
-- spanning those three pcs (octave doubling of the bass goes in the right
-- hand, not just the left).  Without this, a 2-voice realization (e.g.
-- e/g over C bass) slipped through despite being a thin voicing.
local function check_completeness(g, key)
	local notated_pc = g.bass_notated % 12
	local upper_pcs = set_new({})
	for _, n in ipairs(g.inner) do
		set_insert(upper_pcs, n % 12)
	end
	for _, fig in ipairs(g.figures) do
		local required_pc = figure_to_pc(fig, notated_pc, g.bass_notated_letter_idx, key)
		if not required_pc then
			return true, nil
		end
		if not set_contains(upper_pcs, required_pc) then
			return false, string.format("Figure %d (%s) is missing from the right hand", fig.deg, pc_name(required_pc))
		end
	end
	return true, nil
end

-- Resolve a deferred figure-7 completeness check.  Called when the next group
-- arrives (or at lesson boundary).  If the next group forms a 7-6 suspension
-- (figure 6, same bass), demand exactly 2 inner voices (3rd and 7th only).
-- Otherwise treat as a standalone 7th and demand full completeness.
local function resolve_pending_7(next_g)
	if not pending_7 then return end
	local prev = pending_7.group
	local key = pending_7.key
	pending_7 = nil

	local bass_pc = prev.bass % 12
	local is_76 = next_g and is_bare_figure_6(next_g)
		and (next_g.bass % 12 == bass_pc)

	if is_76 then
		-- 7-6 suspension: demand exactly 2 inner voices (3rd and 7th).
		if #prev.inner ~= 2 then
			io.write(string.format(
				"RESULT %d TIME:%d \27[31mFAIL\27[0m 7-6 suspension requires exactly 2 right-hand voices (had %d)\n",
				prev.id, prev.time, #prev.inner))
			return
		end
		-- Verify the 2 voices are the 3rd and 7th.
		local pc_3 = figure_to_pc({ deg = 3, acc = 0 }, bass_pc, prev.bass_letter_idx, key)
		local pc_7 = figure_to_pc({ deg = 7, acc = 0 }, bass_pc, prev.bass_letter_idx, key)
		if not pc_3 or not pc_7 then return end
		local inner_pcs = set_new({})
		for _, n in ipairs(prev.inner) do
			set_insert(inner_pcs, n % 12)
		end
		if not set_contains(inner_pcs, pc_3) then
			io.write(string.format(
				"RESULT %d TIME:%d \27[31mFAIL\27[0m 7-6 suspension: 3rd (%s) is missing\n",
				prev.id, prev.time, pc_name(pc_3)))
		elseif not set_contains(inner_pcs, pc_7) then
			io.write(string.format(
				"RESULT %d TIME:%d \27[31mFAIL\27[0m 7-6 suspension: 7th (%s) is missing\n",
				prev.id, prev.time, pc_name(pc_7)))
		end
	else
		-- Standalone 7th: full completeness check.
		local ok, err = check_completeness(prev, key)
		if not ok then
			io.write(string.format(
				"RESULT %d TIME:%d \27[31mFAIL\27[0m %s\n",
				prev.id, prev.time, err))
		end
	end
end

local function rule_realization_complete(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then
		return true, nil
	end
	if g.in_rising_56 then
		return true, nil
	end

	-- Defer completeness for figure-7 groups (resolved by resolve_pending_7).
	if is_bare_figure_7(g) then
		pending_7 = { group = g, key = ctx.key }
		return true, nil
	end

	return check_completeness(g, ctx.key)
end

-- 5-6 rising: validate voice leading in a decorated rising-scale pattern.
-- When the lesson marks consecutive non-passing beats with figure "5" then
-- figure "6" (and the bass drops a diatonic 3rd from "5" to "6"), the two
-- beats form a pair.  On the "5" beat the right hand plays the 3rd and 5th
-- of the bass; on the "6" beat the 3rd is held and the 5th moves up one
-- diatonic step.  This rule must run before rule_check_realization; it marks
-- paired beats with g.in_rising_56 so standard checks are skipped.

local rising_56_state = nil -- { main_pc, main_letter_idx }

local function rule_rising_56(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing then
		return true, nil
	end
	local key = ctx.key

	if is_bare_figure_5(g) then
		local bass_pc = g.bass % 12
		rising_56_state = {
			main_pc = bass_pc,
			main_letter_idx = g.bass_letter_idx,
		}
		g.in_rising_56 = true

		if #g.inner ~= 2 then
			return false, string.format(
				"5-6 rising: expected 2 right-hand voices (had %d)", #g.inner)
		end

		local pc_3 = figure_to_pc({ deg = 3, acc = 0 }, bass_pc, g.bass_letter_idx, key)
		local pc_5 = figure_to_pc({ deg = 5, acc = 0 }, bass_pc, g.bass_letter_idx, key)
		if not pc_3 or not pc_5 then
			return true, nil
		end

		local inner_pcs = {}
		for _, n in ipairs(g.inner) do
			inner_pcs[n % 12] = true
		end

		if not inner_pcs[pc_3] then
			return false, string.format("5-6 rising: 3rd (%s) is missing", pc_name(pc_3))
		end
		if not inner_pcs[pc_5] then
			return false, string.format("5-6 rising: 5th (%s) is missing", pc_name(pc_5))
		end

		return true, nil
	end

	if is_bare_figure_6(g) and rising_56_state then
		local bass_pc = g.bass % 12
		local main_pc = rising_56_state.main_pc
		local main_letter_idx = rising_56_state.main_letter_idx

		-- Verify bass is a diatonic 3rd below the main note.
		local third_above = figure_to_pc(
			{ deg = 3, acc = 0 }, bass_pc, g.bass_letter_idx, key)
		if third_above ~= main_pc then
			rising_56_state = nil
			return true, nil -- not a 5-6 pair; let standard rules handle
		end

		g.in_rising_56 = true
		rising_56_state = nil -- pair consumed

		if #g.inner ~= 2 then
			return false, string.format(
				"5-6 rising: expected 2 right-hand voices (had %d)", #g.inner)
		end

		local pc_3 = figure_to_pc({ deg = 3, acc = 0 }, main_pc, main_letter_idx, key)
		local pc_6 = figure_to_pc({ deg = 6, acc = 0 }, main_pc, main_letter_idx, key)
		if not pc_3 or not pc_6 then
			return true, nil
		end

		local inner_pcs = {}
		for _, n in ipairs(g.inner) do
			inner_pcs[n % 12] = true
		end

		if not inner_pcs[pc_3] then
			return false, string.format(
				"5-6 rising: 3rd (%s) should be held from previous chord",
				pc_name(pc_3))
		end
		if not inner_pcs[pc_6] then
			return false, string.format(
				"5-6 rising: 5th should move up to %s", pc_name(pc_6))
		end

		return true, nil
	end

	-- Not part of a 5-6 pair; reset state.
	if not g.passing then
		rising_56_state = nil
	end

	return true, nil
end

-- Cadence top-note rule: when a beat is marked with the "c" cadence modifier
-- and the previous non-passing chord's highest inner voice is the 5th of the
-- current chord (= root of the dominant), the current chord's top voice should
-- drop to the 3rd rather than repeating the 5th.
local function rule_cadence_top_note(ctx)
	local g = ctx.window[#ctx.window]
	if not g.cadence or g.passing then
		return true, nil
	end
	if #g.inner == 0 then
		return true, nil
	end

	-- Find previous non-passing group.
	local prev = nil
	for i = #ctx.window - 1, 1, -1 do
		if not ctx.window[i].passing then
			prev = ctx.window[i]
			break
		end
	end
	if not prev or #prev.inner == 0 then
		return true, nil
	end

	local key = ctx.key
	local bass_pc = g.bass % 12
	local pc_5 = figure_to_pc({ deg = 5, acc = 0 }, bass_pc, g.bass_letter_idx, key)
	local pc_3 = figure_to_pc({ deg = 3, acc = 0 }, bass_pc, g.bass_letter_idx, key)
	if not pc_5 or not pc_3 then
		return true, nil
	end

	-- Was the previous chord's top voice the 5th of the current chord?
	local prev_top = prev.inner[#prev.inner]
	if prev_top % 12 ~= pc_5 then
		return true, nil
	end

	-- It was: the current chord's top voice must be the 3rd, not the 5th.
	local curr_top = g.inner[#g.inner]
	if curr_top % 12 == pc_5 then
		return false, string.format(
			"Cadence: drop the top voice to the 3rd (%s) instead of repeating the 5th (%s)",
			pc_name(pc_3), pc_name(pc_5))
	end

	return true, nil
end

-- Sequence rule: when the bass forms a repeating two-interval pattern (e.g.
-- up-5th/down-3rd), the student must use the same voicing factor (3rd, 5th,
-- or root on top) at corresponding positions across pairs, and different
-- factors on the two notes within each pair.

local seq_np_buf = {}       -- recent non-passing: {letter_idx, bass_semi, factor}
local seq_active = false
local seq_motif = nil       -- {within_pair_interval, between_pair_interval}
local seq_next_pos = 0      -- expected position of NEXT note (1 or 2)
local seq_factors = {}      -- [1]=factor at pos 1, [2]=factor at pos 2
local seq_prev_factor = nil -- factor of previous note in the sequence

local function diatonic_interval(from_letter, from_semi, to_letter, to_semi)
	if to_semi == from_semi then
		return 0
	end
	local letter_diff = (to_letter - from_letter + 7) % 7
	if letter_diff == 0 then
		return to_semi > from_semi and 7 or -7
	end
	if to_semi > from_semi then
		return letter_diff
	else
		return -(7 - letter_diff)
	end
end

local FACTOR_NAME = {
	[2] = "2nd", [3] = "3rd", [4] = "4th",
	[5] = "5th", [6] = "6th", [7] = "7th",
	[8] = "root",
}

local function top_voice_factor(g, key)
	if #g.inner == 0 then
		return nil
	end
	local top_pc = g.inner[#g.inner] % 12
	-- Use the NOTATED bass (and its letter index) so the factor reflects
	-- what the score asks for, not what the user happened to play in the
	-- bass.  For figures with accidentals (e.g. "#" = sharp 3), we must
	-- match against the figure-implied pitch (e.g. C# above A) — not the
	-- plain diatonic 3rd (C natural in d minor) — otherwise a correctly
	-- played sharp third would never be credited.
	local bass_pc = g.bass_notated % 12
	if top_pc == bass_pc then
		return 8
	end
	for _, fig in ipairs(g.figures) do
		local pc = figure_to_pc(fig, bass_pc, g.bass_notated_letter_idx, key)
		if pc and top_pc == pc then
			-- Normalise compound intervals (9→2, 10→3, …) so all eight
			-- diatonic positions land in the small canonical set tracked
			-- by stats.lua's POSITION_FACTORS.
			local d = ((fig.deg - 1) % 7) + 1
			if d == 1 then return 8 end -- octave / root doubling
			if d == 2 or d == 3 or d == 4
				or d == 5 or d == 6 or d == 7 then
				return d
			end
			return nil
		end
	end
	return nil
end

local function seq_reset()
	seq_np_buf = {}
	seq_active = false
	seq_motif = nil
	seq_factors = {}
	seq_prev_factor = nil
end

local function rule_sequence(ctx)
	local g = ctx.window[#ctx.window]
	if g.passing or g.in_rising_56 or g.cadence then
		return true, nil
	end
	if g.figures_raw ~= "0" then
		-- Only applies to plain root-position triads.
		if not g.passing then
			seq_active = false
		end
		return true, nil
	end

	local key = ctx.key
	local factor = top_voice_factor(g, key)
	local entry = {
		letter_idx = g.bass_letter_idx,
		bass_semi = g.bass,
		factor = factor,
	}

	-- Compute diatonic interval from previous non-passing note.
	local interval = nil
	if #seq_np_buf > 0 then
		local prev = seq_np_buf[#seq_np_buf]
		interval = diatonic_interval(
			prev.letter_idx, prev.bass_semi,
			entry.letter_idx, entry.bass_semi)
	end

	-- Push to buffer (keep last 5).
	seq_np_buf[#seq_np_buf + 1] = entry
	if #seq_np_buf > 5 then
		table.remove(seq_np_buf, 1)
	end

	-- ── Active sequence: verify interval and voicing ────────────────────
	if seq_active and interval then
		local expected
		if seq_next_pos == 1 then
			expected = seq_motif[2] -- between-pair step
		else
			expected = seq_motif[1] -- within-pair interval
		end

		if interval ~= expected then
			seq_active = false
			seq_motif = nil
			seq_factors = {}
			seq_prev_factor = nil
			-- Fall through to detection.
		else
			-- Cross-pair consistency.
			if factor and seq_factors[seq_next_pos] then
				if factor ~= seq_factors[seq_next_pos] then
					local name = FACTOR_NAME[seq_factors[seq_next_pos]]
						or tostring(seq_factors[seq_next_pos])
					seq_prev_factor = factor
					seq_next_pos = seq_next_pos == 1 and 2 or 1
					return false, string.format(
						"Sequence: keep the %s on top (matching previous pairs)", name)
				end
			elseif factor then
				seq_factors[seq_next_pos] = factor
			end

			-- Within-pair variety (position 2 only).
			if seq_next_pos == 2 and factor and seq_prev_factor
				and factor == seq_prev_factor then
				local name = FACTOR_NAME[factor] or tostring(factor)
				seq_prev_factor = factor
				seq_next_pos = 1
				return false, string.format(
					"Sequence: avoid repeating the %s on top within this pair", name)
			end

			seq_prev_factor = factor
			seq_next_pos = seq_next_pos == 1 and 2 or 1
			return true, nil
		end
	end

	-- ── Detection: check if last 4 entries form a repeating pattern ─────
	if #seq_np_buf >= 4 and interval then
		local n = #seq_np_buf
		local i1 = diatonic_interval(
			seq_np_buf[n - 3].letter_idx, seq_np_buf[n - 3].bass_semi,
			seq_np_buf[n - 2].letter_idx, seq_np_buf[n - 2].bass_semi)
		local i2 = diatonic_interval(
			seq_np_buf[n - 2].letter_idx, seq_np_buf[n - 2].bass_semi,
			seq_np_buf[n - 1].letter_idx, seq_np_buf[n - 1].bass_semi)
		local i3 = interval -- from buf[n-1] to buf[n]

		if i1 == i3 and i1 ~= 0 then
			-- Pattern confirmed: motif = (within-pair, between-pair).
			seq_active = true
			seq_motif = { i1, i2 }
			seq_next_pos = 1 -- next note starts pair 3
			seq_factors = {
				[1] = seq_np_buf[n - 3].factor,
				[2] = seq_np_buf[n - 2].factor,
			}
			seq_prev_factor = factor

			-- Check current note (pos 2 of pair 2) against established pos 2.
			if factor and seq_factors[2] and factor ~= seq_factors[2] then
				local name = FACTOR_NAME[seq_factors[2]]
					or tostring(seq_factors[2])
				return false, string.format(
					"Sequence: keep the %s on top (matching previous pairs)", name)
			end

			-- Within-pair: current (pos 2) vs pair-2 pos-1 factor.
			local pair2_pos1 = seq_np_buf[n - 1].factor
			if factor and pair2_pos1 and factor == pair2_pos1 then
				local name = FACTOR_NAME[factor] or tostring(factor)
				return false, string.format(
					"Sequence: avoid repeating the %s on top within this pair", name)
			end
		end
	end

	return true, nil
end

local RULES = {
	rule_mark_octave_jump,
	rule_rising_56,
	rule_no_parallel_fifths,
	rule_no_parallel_octaves,
	rule_bass_leap,
	rule_passing_bass,
	rule_bass_matches_notated,
	rule_check_realization,
	rule_bass_in_key,
	rule_not_past_end,
	rule_realization_not_empty,
	rule_realization_complete,
	rule_cadence_top_note,
	rule_sequence,
}

-- ---------------------------------------------------------------------------
-- Parsing
-- ---------------------------------------------------------------------------

local function parse_lesson_key(line)
	local tokens = {}
	for t in line:gmatch("%S+") do
		tokens[#tokens + 1] = t
	end
	if tokens[3] then
		return parse_key(tokens[3])
	end
	return nil
end

local function parse_group(line)
	if not line:match("^GROUP") then
		return nil
	end

	local id = 0
	local passing = line:find("passing") ~= nil
	local bass = 0
	local bass_letter_idx = nil -- 1-based index into NATURAL_PCS for the bass letter
	local bass_notated_letter_idx = nil -- same, but for the notated (score) bass
	local bass_notated_raw = ""
	local cadence = false
	local figures = parse_figures("0")
	local figures_raw = ""
	local inner = {}
	local melody = {}
	local melody_raw_parts = {}
	local time_ms = 0

	local tokens = {}
	for t in line:gmatch("%S+") do
		tokens[#tokens + 1] = t
	end

	local i = 1
	while i <= #tokens do
		local t = tokens[i]
		local v

		v = t:match("^ID:(.+)$")
		if v then
			id = tonumber(v) or 0
		end

		v = t:match("^BASS:(.+)$")
		if v then
			bass_notated_raw = v
			local letter_str = v:gsub("[%d%.]+$", ""):sub(1, 1):lower()
			local letter_map = { c = 1, d = 2, e = 3, f = 4, g = 5, a = 6, b = 7 }
			bass_notated_letter_idx = letter_map[letter_str]
		end

		v = t:match("^BASS_ACTUAL:(.+)$")
		if v then
			bass = to_semitone(v) or 0
			-- Extract the letter index (1-based into NATURAL_PCS) from the
			-- pitch name, stripping duration digits/dots then accidental suffixes.
			local letter_str = v:gsub("[%d%.]+$", ""):sub(1, 1):lower()
			local letter_map = { c = 1, d = 2, e = 3, f = 4, g = 5, a = 6, b = 7 }
			bass_letter_idx = letter_map[letter_str]
		end

		v = t:match("^FIGURES:(.+)$")
		if v then
			figures_raw = v
			local fig_for_parse = v
			if fig_for_parse:sub(-1) == "c" then
				cadence = true
				fig_for_parse = fig_for_parse:sub(1, -2)
			end
			figures = parse_figures(fig_for_parse)
		end

		v = t:match("^REALIZATION:(.+)$")
		if v then
			inner = {}
			for part in v:gmatch("[^/]+") do
				local s = to_semitone(part)
				if s then
					inner[#inner + 1] = s
				end
			end
			table.sort(inner)
		end

		v = t:match("^MELODY:(.*)$")
		if v then
			if v ~= "" then
				melody_raw_parts[#melody_raw_parts + 1] = v
				melody[#melody + 1] = to_semitone(v) or 0
			end
			while i + 1 <= #tokens and not tokens[i + 1]:find(":") do
				i = i + 1
				melody_raw_parts[#melody_raw_parts + 1] = tokens[i]
				melody[#melody + 1] = to_semitone(tokens[i]) or 0
			end
		end

		v = t:match("^TIME:(.+)$")
		if v then
			time_ms = tonumber(v) or 0
		end

		i = i + 1
	end

	local bass_notated
	if bass_notated_raw == "" then
		bass_notated = bass
	else
		bass_notated = to_semitone(bass_notated_raw) or bass
	end

	local melody_raw = table.concat(melody_raw_parts, " ")

	-- If the notated bass was omitted or unparseable, fall back to the
	-- played letter index so figure computations still work.
	if not bass_notated_letter_idx then
		bass_notated_letter_idx = bass_letter_idx
	end

	return {
		id = id,
		passing = passing,
		cadence = cadence,
		bass = bass,
		bass_letter_idx = bass_letter_idx,
		bass_notated = bass_notated,
		bass_notated_raw = bass_notated_raw,
		bass_notated_letter_idx = bass_notated_letter_idx,
		figures = figures,
		figures_raw = figures_raw,
		inner = inner,
		melody = melody,
		melody_raw = melody_raw,
		time = time_ms,
	}
end

local function push_window(g)
	window[#window + 1] = g
	if #window > 4 then
		table.remove(window, 1)
	end
end

local function run_rules(ctx)
	local errors = {}
	for _, rule in ipairs(RULES) do
		local ok, err = rule(ctx)
		if not ok then
			errors[#errors + 1] = err
		end
	end
	if #errors == 0 then
		return true, {}
	end
	return false, errors
end

local function print_result(g, ok, errors, factor)
	local suffix = factor and string.format(" FACTOR:%d", factor) or ""
	if ok then
		io.write(string.format("RESULT %d TIME:%d \27[32mOK\27[0m%s\n", g.id, g.time, suffix))
	else
		for _, err in ipairs(errors) do
			io.write(string.format("RESULT %d TIME:%d \27[31mFAIL\27[0m %s%s\n", g.id, g.time, err, suffix))
		end
	end
end

local function main()
	local current_lesson_id = nil
	for line in io.lines() do
		line = line:gsub("\r", "")
		if line:match("^LESSON") then
			-- Distinguish a genuine lesson change from a reload of the same
			-- lesson.  On reload we drop any stale pending_7 silently — the
			-- user is starting over and shouldn't get a FAIL for a 7-chord
			-- they're about to replay.  On a real lesson change, flush so
			-- the previous lesson's last 7-chord still gets validated.
			local new_id = line:match("^LESSON%s+(%S+)")
			if current_lesson_id and new_id ~= current_lesson_id then
				resolve_pending_7(nil)
			else
				pending_7 = nil
			end
			current_lesson_id = new_id
			rising_56_state = nil
			seq_reset()
			local k = parse_lesson_key(line)
			if k then
				key = k
				window = {}
			end
		elseif line:match("^GROUP") then
			local g = parse_group(line)
			if g then
				push_window(g)
				resolve_pending_7(g)
				local ok, err = run_rules({
					window = window,
					key = key,
				})
				local factor = (g.id == 0 and not g.passing)
					and top_voice_factor(g, key) or nil
				print_result(g, ok, err, factor)
			end
		end
	end
end

main()
