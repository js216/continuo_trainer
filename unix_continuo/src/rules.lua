-- SPDX-License-Identifier: MIT
-- rules.lua --- elegant voice-leading validation for figured bass
-- Copyright (c) 2026 Jakob Kastelic
-- Lua 5.4 translation of rules.rs

-- ---------------------------------------------------------------------------
-- Pitch helpers
-- ---------------------------------------------------------------------------

local function to_semitone(p)
    if not p then return nil end
    -- strip trailing digits and dots
    p = p:match("^(.-)%s*$")
    p = p:gsub("[%d%.]+$", "")
    if p == "" then return nil end

    local first = p:sub(1,1):lower()
    local rest  = p:sub(2)

    local acc, oct_part
    if rest:sub(1,4) == "isis" then
        acc, oct_part = 2, rest:sub(5)
    elseif rest:sub(1,4) == "eses" then
        acc, oct_part = -2, rest:sub(5)
    elseif rest:sub(1,2) == "is" then
        acc, oct_part = 1, rest:sub(3)
    elseif rest:sub(1,2) == "es" or rest:sub(1,2) == "as" then
        acc, oct_part = -1, rest:sub(3)
    else
        acc, oct_part = 0, rest
    end

    local octaves = 0
    for c in oct_part:gmatch(".") do
        if c == "'" then octaves = octaves + 12
        elseif c == "," then octaves = octaves - 12
        end
    end

    local base_map = {c=0, d=2, e=4, f=5, g=7, a=9, b=11}
    local base = base_map[first]
    if not base then return nil end

    return base + acc + octaves + 48  -- c' = 60
end

local PC_NAMES = {"c","cis","d","dis","e","f","fis","g","gis","a","ais","b"}

local function pc_name(semitone)
    local idx = semitone % 12
    if idx < 0 then idx = idx + 12 end
    return PC_NAMES[idx + 1] or "?"
end

local function rem_euclid(a, b)
    local r = a % b
    if r < 0 then r = r + b end
    return r
end

-- ---------------------------------------------------------------------------
-- Key signature parsing
-- ---------------------------------------------------------------------------

local MAJOR_STEPS = {0, 2, 4, 5, 7, 9, 11}
local MINOR_STEPS = {0, 2, 3, 5, 7, 8, 10}

local function parse_key(token)
    if not token or token == "" then return {scale_pcs = {}} end

    local first = token:sub(1,1)
    local is_minor = (first == first:lower())
    local up = first:upper()

    local root_base_map = {C=0, D=2, E=4, F=5, G=7, A=9, B=11}
    local root_base = root_base_map[up]
    if not root_base then return {scale_pcs = {}} end

    local rest = token:sub(2)
    local acc_map = {["#"]=1, ["is"]=1, ["##"]=2, ["isis"]=2,
                     ["b"]=-1, ["es"]=-1, ["as"]=-1, ["bb"]=-2, ["eses"]=-2}
    local acc = acc_map[rest] or 0

    local root = rem_euclid(root_base + acc, 12)
    local steps = is_minor and MINOR_STEPS or MAJOR_STEPS
    local scale_pcs = {}
    for _, s in ipairs(steps) do
        scale_pcs[#scale_pcs+1] = (root + s) % 12
    end
    return {scale_pcs = scale_pcs}
end

-- ---------------------------------------------------------------------------
-- Figure parsing
-- ---------------------------------------------------------------------------

local function parse_one_figure(t)
    t = t:match("^%s*(.-)%s*$")
    if t == "" then return nil end

    local acc, rest
    if t:sub(1,2) == "##" then
        acc, rest = 2, t:sub(3)
    elseif t:sub(1,2) == "bb" then
        acc, rest = -2, t:sub(3)
    elseif t:sub(1,1) == "#" then
        acc, rest = 1, t:sub(2)
    elseif t:sub(1,1) == "b" and (t:len() == 1 or t:sub(2,2):match("%d")) then
        acc, rest = -1, t:sub(2)
    elseif t:sub(1,1) == "n" then
        acc, rest = 0, t:sub(2)
    elseif t:sub(1,4) == "isis" then
        acc, rest = 2, t:sub(5)
    elseif t:sub(1,4) == "eses" then
        acc, rest = -2, t:sub(5)
    elseif t:sub(1,2) == "is" then
        acc, rest = 1, t:sub(3)
    elseif t:sub(1,2) == "es" or t:sub(1,2) == "as" then
        acc, rest = -1, t:sub(3)
    else
        acc, rest = 0, t
    end

    local deg
    if rest == "" then
        deg = 3
    else
        deg = tonumber(rest)
        if not deg then return nil end
    end
    return {deg = deg, acc = acc}
end

local function default_triad()
    return {{deg=3,acc=0},{deg=5,acc=0},{deg=8,acc=0}}
end

local function parse_figures(s)
    s = s and s:match("^%s*(.-)%s*$") or ""
    if s == "" or s == "0" then
        return default_triad()
    end

    local figs = {}
    for part in s:gmatch("[^/,]+") do
        local fig = parse_one_figure(part)
        if fig then figs[#figs+1] = fig end
    end

    if #figs == 0 then return default_triad() end

    if #figs == 1 and figs[1].deg == 3 then
        figs[#figs+1] = {deg=5, acc=0}
        figs[#figs+1] = {deg=8, acc=0}
    end

    if #figs == 1 and figs[1].deg == 4 then
        figs[#figs+1] = {deg=5, acc=0}
        figs[#figs+1] = {deg=8, acc=0}
    end

    if #figs == 1 and figs[1].deg == 6 then
        table.insert(figs, 1, {deg=3, acc=0})
    end

    table.sort(figs, function(a,b) return a.deg < b.deg end)

    -- dedup by deg
    local deduped = {figs[1]}
    for k = 2, #figs do
        if figs[k].deg ~= figs[k-1].deg then
            deduped[#deduped+1] = figs[k]
        end
    end
    return deduped
end

local function figure_to_pc(fig, bass_pc, key)
    local deg = ((fig.deg - 1) % 7) + 1
    local bass_deg = nil
    for i, pc in ipairs(key.scale_pcs) do
        if pc == bass_pc then bass_deg = i - 1; break end  -- 0-based
    end
    if bass_deg == nil then return nil end
    local target_deg = (bass_deg + (deg - 1)) % 7
    local diatonic_pc = key.scale_pcs[target_deg + 1]
    return rem_euclid(diatonic_pc + fig.acc, 12)
end

-- ---------------------------------------------------------------------------
-- Set helpers
-- ---------------------------------------------------------------------------

local function set_new(t)
    local s = {}
    if t then for _, v in ipairs(t) do s[v] = true end end
    return s
end

local function set_contains(s, v) return s[v] == true end
local function set_insert(s, v) s[v] = true end

-- ---------------------------------------------------------------------------
-- Rules
-- ---------------------------------------------------------------------------

local function rule_no_parallels(ctx)
    local window = ctx.window
    if #window < 2 then return true, nil end
    local prev = window[#window - 1]
    local curr = window[#window]
    if curr.passing or prev.passing then return true, nil end

    local function flatten(g)
        local v = {g.bass}
        local mel = g.melody[1]
        local b_pc = g.bass % 12
        local m_pc = mel and (mel % 12) or nil

        for _, n in ipairs(g.inner) do
            local n_pc = n % 12
            if n_pc ~= m_pc and n_pc ~= b_pc then
                v[#v+1] = n
            end
        end
        if mel then v[#v+1] = mel end
        return v
    end

    local p_voices = flatten(prev)
    local c_voices = flatten(curr)

    for i = 1, #p_voices do
        for j = i+1, #p_voices do
            if j > #c_voices or j > #p_voices then goto continue end

            local p_int = math.abs(p_voices[j] - p_voices[i]) % 12
            local c_int = math.abs(c_voices[j] - c_voices[i]) % 12

            if (p_int == 7 and c_int == 7) or (p_int == 0 and c_int == 0) then
                local motion_i = c_voices[i] - p_voices[i]
                local motion_j = c_voices[j] - p_voices[j]
                local function signum(x)
                    if x > 0 then return 1 elseif x < 0 then return -1 else return 0 end
                end
                if signum(motion_i) == signum(motion_j) and motion_i ~= 0 then
                    return false, string.format(
                        "Parallel fifths/octaves between voice %d and %d", i-1, j-1)
                end
            end
            ::continue::
        end
    end
    return true, nil
end

local function rule_bass_leap(ctx)
    local window = ctx.window
    if #window < 2 then return true, nil end
    local p = window[#window - 1]
    local c = window[#window]
    if math.abs(c.bass - p.bass) % 12 == 6 then
        return false, "Tritone leap in bass"
    end
    return true, nil
end

local function rule_check_realization(ctx)
    local g = ctx.window[#ctx.window]
    if g.passing then return true, nil end
    local key = ctx.key
    local bass_pc = g.bass % 12

    local allowed = set_new({bass_pc})

    for _, fig in ipairs(g.figures) do
        local pc = figure_to_pc(fig, bass_pc, key)
        if pc then
            set_insert(allowed, pc)
        else
            return true, nil
        end
    end

    -- suspension resolution: admit prev pitches if bass is held
    if #ctx.window >= 2 then
        local prev = ctx.window[#ctx.window - 1]
        if prev.bass % 12 == bass_pc then
            for _, n in ipairs(prev.inner) do set_insert(allowed, n % 12) end
            for _, n in ipairs(prev.melody) do set_insert(allowed, n % 12) end
            set_insert(allowed, prev.bass % 12)
        end
    end

    local fig_parts = {}
    for _, f in ipairs(g.figures) do
        local acc_str = (f.acc == 2 and "##") or (f.acc == 1 and "#")
                     or (f.acc == -1 and "b") or (f.acc == -2 and "bb") or ""
        fig_parts[#fig_parts+1] = acc_str .. tostring(f.deg)
    end
    local fig_label = table.concat(fig_parts, "/")

    for _, note in ipairs(g.inner) do
        local pc = note % 12
        if not set_contains(allowed, pc) then
            return false, string.format(
                "Incorrect realization of figure %s: %s", fig_label, pc_name(note))
        end
    end
    return true, nil
end

local function rule_bass_in_key(ctx)
    local g = ctx.window[#ctx.window]
    if g.passing then return true, nil end
    local bass_pc    = g.bass % 12
    local notated_pc = g.bass_notated % 12

    if notated_pc == bass_pc then return true, nil end

    local diatonic = false
    for _, pc in ipairs(ctx.key.scale_pcs) do
        if pc == bass_pc then diatonic = true; break end
    end
    if not diatonic then
        return false, string.format(
            "Bass note %s is not diatonic to the key", pc_name(g.bass))
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
    if g.passing then return true, nil end
    if #g.inner == 0 then
        return false, "No realization provided (inner voices are empty)"
    end
    return true, nil
end

local function rule_realization_complete(ctx)
    local g = ctx.window[#ctx.window]
    if g.passing then return true, nil end
    local key = ctx.key
    local bass_pc = g.bass % 12

    local present = set_new({bass_pc})
    for _, n in ipairs(g.inner)   do set_insert(present, n % 12) end
    for _, n in ipairs(g.melody)  do set_insert(present, n % 12) end

    for _, fig in ipairs(g.figures) do
        local required_pc = figure_to_pc(fig, bass_pc, key)
        if not required_pc then return true, nil end
        if not set_contains(present, required_pc) then
            return false, string.format(
                "Figure %d (%s) is missing from the realization",
                fig.deg, pc_name(required_pc))
        end
    end
    return true, nil
end

local RULES = {
    rule_no_parallels,
    rule_bass_leap,
    rule_check_realization,
    rule_bass_in_key,
    rule_not_past_end,
    rule_realization_not_empty,
    rule_realization_complete,
}

-- ---------------------------------------------------------------------------
-- Parsing
-- ---------------------------------------------------------------------------

local function parse_lesson_key(line)
    local tokens = {}
    for t in line:gmatch("%S+") do tokens[#tokens+1] = t end
    if tokens[3] then return parse_key(tokens[3]) end
    return nil
end

local function parse_group(line)
    if not line:match("^GROUP") then return nil end

    local id             = 0
    local passing        = line:find("passing") ~= nil
    local bass           = 0
    local bass_notated_raw = ""
    local figures        = parse_figures("0")
    local figures_raw    = ""
    local inner          = {}
    local melody         = {}
    local melody_raw_parts = {}
    local time_ms        = 0

    local tokens = {}
    for t in line:gmatch("%S+") do tokens[#tokens+1] = t end

    local i = 1
    while i <= #tokens do
        local t = tokens[i]
        local v

        v = t:match("^ID:(.+)$")
        if v then id = tonumber(v) or 0 end

        v = t:match("^BASS:(.+)$")
        if v then bass_notated_raw = v end

        v = t:match("^BASS_ACTUAL:(.+)$")
        if v then bass = to_semitone(v) or 0 end

        v = t:match("^FIGURES:(.+)$")
        if v then
            figures_raw = v
            figures = parse_figures(v)
        end

        v = t:match("^REALIZATION:(.+)$")
        if v then
            inner = {}
            for part in v:gmatch("[^/]+") do
                local s = to_semitone(part)
                if s then inner[#inner+1] = s end
            end
            table.sort(inner)
        end

        v = t:match("^MELODY:(.*)$")
        if v then
            if v ~= "" then
                melody_raw_parts[#melody_raw_parts+1] = v
                melody[#melody+1] = to_semitone(v) or 0
            end
            while i+1 <= #tokens and not tokens[i+1]:find(":") do
                i = i + 1
                melody_raw_parts[#melody_raw_parts+1] = tokens[i]
                melody[#melody+1] = to_semitone(tokens[i]) or 0
            end
        end

        v = t:match("^TIME:(.+)$")
        if v then time_ms = tonumber(v) or 0 end

        i = i + 1
    end

    local bass_notated
    if bass_notated_raw == "" then
        bass_notated = bass
    else
        bass_notated = to_semitone(bass_notated_raw) or bass
    end

    local melody_raw = table.concat(melody_raw_parts, " ")

    return {
        id               = id,
        passing          = passing,
        bass             = bass,
        bass_notated     = bass_notated,
        bass_notated_raw = bass_notated_raw,
        figures          = figures,
        figures_raw      = figures_raw,
        inner            = inner,
        melody           = melody,
        melody_raw       = melody_raw,
        time             = time_ms,
    }
end

-- ---------------------------------------------------------------------------
-- Main
-- ---------------------------------------------------------------------------

local window = {}
local key    = {scale_pcs = {}}

for line in io.lines() do
    if line:match("^LESSON") then
        local k = parse_lesson_key(line)
        if k then
            key = k
            window = {}
        end
    else
        local g = parse_group(line)
        if g then
            local id = g.id
            window[#window+1] = g
            if #window > 4 then table.remove(window, 1) end

            local ctx = {window = window, key = key}

            local ok, err = true, nil
            for _, rule in ipairs(RULES) do
                local rok, rerr = rule(ctx)
                if not rok then
                    ok, err = false, rerr
                    break
                end
            end

            if ok then
                io.write(string.format("RESULT %d \27[32mOK\27[0m\n", id))
            else
                io.write(string.format("RESULT %d \27[31mFAIL\27[0m %s\n", id, err))
            end
        end
    end
end
