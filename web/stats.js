// SPDX-License-Identifier: MIT
// stats.js --- browser-side port of stats.lua
// Copyright (c) 2026 Jakob Kastelic
//
// Stores stats as JSON in localStorage under key "continuo_stats".
// Listens for LESSON, BASSNOTE, RESULT, CHUNK_NAME, CHILDREN, LOAD_CHUNK,
// QUERY_STATS, and SUGGEST_LESSON lines.
// Emits STATS, SUGGESTION, and BPM lines via the onLine callback.
// Sends QUERY_CHILDREN lines via the send callback (→ server → all.lua).

// Any single transition longer than MAX_DURATION seconds is clamped to that
// value and counted as a failure.
const MAX_DURATION = 60;

const STATS_KEY = "continuo_stats";

const ALGORITHM_DEFAULTS = {
    score_goal:              1000,
    ema_alpha:               0.18,
    pass_accuracy:           80,
    mastery_growth:          0.20,
    power_half_life:         0.693,
    mastery_points_per_pct:  2.5,
    power_points_per_pct:    1.0,
    bottleneck_thresh:       0.6,
    dominance_margin:        0.2,
    min_quality:             0.1,
    power_score_frac:        0.7,
    overlearn_min:           5,
    overlearn_max:           15,
    mistake_power_penalty:   0.15,
    mastery_decay_half_life: 90,
    skill_order:             "root 6 4-3_sus 6/4 7 other",
    weak_ema_thresh:         0.8,
    ease_initial:            2.5,
    ease_min:                1.3,
    ease_max:                3.5,
    ease_pass_delta:         0.1,
    ease_fail_delta:         0.2,
    ivl_first:               1,
    ivl_second:              6,
    ivl_max:                 365,
    chunk_mastery_thresh:    80,
    chunk_power_thresh:      70,
};

class Stats {
    // onLine: callback for STATS/SUGGESTION/BPM lines emitted by stats
    // send:   function to forward QUERY_CHILDREN to the server
    constructor(onLine, send) {
        this._onLine          = onLine;
        this._send            = send;
        this._chunkSkills     = {};  // hash → skills string
        this._childrenOf      = {};  // hash → [child hashes]
        this._pendingChildren = {};  // hash → { results, abs_s, abs_e, failedGroups }
        this._current         = null;  // active lesson accumulator
        this._currentLoaded   = null;  // hash of last LOAD_CHUNK

        const stats = this._load();
        this._applyMasteryDecay(stats);
        // Pre-populate from stored data (like stats.lua's fast startup)
        for (const [h, c] of Object.entries(stats.chunks)) {
            if (c.skills)    this._chunkSkills[h] = c.skills;
            if (c.children)  this._childrenOf[h]  = c.children;
        }
    }

    // ── Storage ───────────────────────────────────────────────────────────────

    _load() {
        let data = {};
        try { const raw = localStorage.getItem(STATS_KEY); if (raw) data = JSON.parse(raw); }
        catch (_) {}
        return this._withDefaults(data);
    }

    _save(data) {
        try { localStorage.setItem(STATS_KEY, JSON.stringify(data)); } catch (_) {}
    }

    _withDefaults(data) {
        data.algorithm = data.algorithm || {};
        for (const [k, v] of Object.entries(ALGORITHM_DEFAULTS))
            if (data.algorithm[k] === undefined) data.algorithm[k] = v;
        data.daily  = data.daily  || {};
        data.chunks = data.chunks || {};
        return data;
    }

    // ── Date helpers ──────────────────────────────────────────────────────────

    _dateStr() { return new Date().toISOString().slice(0, 10); }

    _daysElapsed(dateStr) {
        const [y, m, d] = dateStr.split("-").map(Number);
        const ms = Date.UTC(y, m - 1, d, 12, 0, 0);
        return Math.max(0, Math.floor((Date.now() - ms) / 86400000));
    }

    // ── Metrics ───────────────────────────────────────────────────────────────

    _calculatePower(lData, alg) {
        const mastery = lData.mastery || 0;
        const ivl     = lData.ivl     || 0;
        if (!lData.last_date || ivl <= 0) return 0;
        const days      = this._daysElapsed(lData.last_date);
        const stability = Math.exp(-alg.power_half_life * days / ivl);
        return Math.min(mastery, mastery * stability) * (lData.power_factor || 1.0);
    }

    _calculateEffectivePower(lData, alg) {
        const mastery = Math.max(lData.mastery || 0, lData.t_mastery || 0);
        const ivl     = lData.ivl || 0;
        let effLast   = lData.last_date;
        const t       = lData.t_last_date;
        if (t && (!effLast || t > effLast)) effLast = t;
        if (!effLast || ivl <= 0) return 0;
        const days      = this._daysElapsed(effLast);
        const stability = Math.exp(-alg.power_half_life * days / ivl);
        return Math.min(mastery, mastery * stability) * (lData.power_factor || 1.0);
    }

    _calculateStreak(data) {
        let streak = 0;
        const today = this._dateStr();
        let ts = Date.now();
        while (true) {
            const dStr     = new Date(ts).toISOString().slice(0, 10);
            const dayScore = (data.daily[dStr] && data.daily[dStr].score) || 0;
            if (dayScore >= data.algorithm.score_goal) {
                streak++;
                ts -= 86400000;
            } else {
                if (dStr === today && streak === 0) ts -= 86400000;
                else break;
            }
        }
        return streak;
    }

    _normalize(raw, lData) {
        const mg = lData.max_groups || 0;
        return mg > 0 ? (raw / (mg * 5.0)) * 100.0 : raw;
    }

    // ── Mastery decay ─────────────────────────────────────────────────────────

    _applyMasteryDecay(stats) {
        const alg      = stats.algorithm;
        const today    = this._dateStr();
        const halfLife = alg.mastery_decay_half_life;
        let changed    = false;
        for (const c of Object.values(stats.chunks)) {
            const lum = c.last_updated_mastery;
            if (lum && halfLife > 0) {
                const days = this._daysElapsed(lum);
                if (days > 0) {
                    const factor = Math.exp(-0.693 * days / halfLife);
                    if ((c.mastery   || 0) > 0) { c.mastery   = (c.mastery   || 0) * factor; changed = true; }
                    if ((c.t_mastery || 0) > 0) { c.t_mastery = (c.t_mastery || 0) * factor; changed = true; }
                }
            }
            if (lum) { c.last_updated_mastery = today; changed = true; }
        }
        if (changed) this._save(stats);
    }

    // ── Score computation ─────────────────────────────────────────────────────

    _computeScoreData(results, s, e) {
        let firstIdx = null;
        for (let i = s; i <= e; i++) if (results[i]) { firstIdx = i; break; }
        if (firstIdx === null) return null;

        const totalGroups = e - s + 1;
        let okCount = 0, minDelta = Infinity, maxDelta = 0, sumDelta = 0;
        let transitionCount = 0, totalDuration = 0;

        for (let i = s; i <= e; i++) {
            if (!results[i]) continue;
            let status = results[i].status;
            if (i > s && results[i - 1]) {
                let deltaSec = (results[i].time - results[i - 1].time) / 1000;
                if (deltaSec > MAX_DURATION) { status = "FAIL"; deltaSec = MAX_DURATION; }
                const deltaMs = deltaSec * 1000;
                if (deltaMs < minDelta) minDelta = deltaMs;
                if (deltaMs > maxDelta) maxDelta = deltaMs;
                sumDelta += deltaMs;
                transitionCount++;
                totalDuration += deltaSec;
            }
            if (status === "OK") okCount++;
        }

        const accuracy = (okCount / totalGroups) * 100;
        let score = 0;
        if (accuracy === 100) {
            let sf;
            if      (maxDelta <= 50)   sf = 5.0;
            else if (maxDelta >= 3000) sf = 1.0;
            else                       sf = 5.0 - 4.0 * (maxDelta - 50) / (3000 - 50);
            score = totalGroups * sf;
        }
        return {
            accuracy,
            score,
            duration:  totalDuration,
            slowest:   maxDelta / 1000,
            fastest:   minDelta === Infinity ? 0 : minDelta / 1000,
            average:   transitionCount > 0 ? sumDelta / transitionCount / 1000 : 0,
            groups:    totalGroups,
        };
    }

    // ── Entry update ──────────────────────────────────────────────────────────

    _updateEntry(entry, sd, alg) {
        const today = this._dateStr();
        if (sd.groups > (entry.max_groups || 0)) entry.max_groups = sd.groups;

        const oldMastery = entry.mastery || 0;
        const oldPower   = this._calculatePower(entry, alg);

        // EMA pass rate
        const passThis = sd.accuracy >= alg.pass_accuracy ? 1.0 : 0.0;
        entry.ema_pass = entry.ema_pass !== undefined
            ? alg.ema_alpha * passThis + (1 - alg.ema_alpha) * entry.ema_pass
            : passThis;

        // Speed factor
        let speedFactor;
        if (sd.average > 0) {
            if (entry.best_avg === undefined || sd.average < entry.best_avg) entry.best_avg = sd.average;
            speedFactor = Math.min(1.0, entry.best_avg / sd.average);
        } else {
            speedFactor = 1.0;
        }

        // Evenness factor
        const evennessFactor = (sd.slowest > 0 && sd.fastest > 0)
            ? Math.min(1.0, sd.fastest / sd.slowest)
            : 1.0;

        const quality = entry.ema_pass * speedFactor * evennessFactor;

        // Mastery growth
        if (sd.accuracy >= alg.pass_accuracy && sd.score > oldMastery) {
            entry.mastery = oldMastery + (sd.score - oldMastery) * alg.mastery_growth * quality;
            entry.last_updated_mastery = today;
        }

        // SRS
        if (sd.accuracy === 100) {
            entry.n_pass     = (entry.n_pass || 0) + 1;
            entry.n_fail     = 0;
            entry.n_pass_tot = (entry.n_pass_tot || 0) + 1;
            if      (entry.n_pass === 1) entry.ivl = alg.ivl_first;
            else if (entry.n_pass === 2) entry.ivl = alg.ivl_second;
            else                          entry.ivl = Math.min(alg.ivl_max, Math.ceil((entry.ivl || 1) * (entry.ease || alg.ease_initial)));
            entry.ease = Math.min(alg.ease_max, (entry.ease || alg.ease_initial) + alg.ease_pass_delta);
        } else {
            entry.n_fail     = (entry.n_fail || 0) + 1;
            entry.n_pass     = 0;
            entry.n_fail_tot = (entry.n_fail_tot || 0) + 1;
            entry.ivl        = 1;
            entry.ease       = Math.max(alg.ease_min, (entry.ease || alg.ease_initial) - alg.ease_fail_delta);
        }

        entry.total_duration = (entry.total_duration || 0) + sd.duration;
        entry.last_date      = today;

        const newPower = this._calculatePower(entry, alg);
        return {
            mDelta: Math.max(0, (entry.mastery || 0) - oldMastery),
            pDelta: Math.max(0, newPower - oldPower),
        };
    }

    // Transitive update: only touches t_mastery, t_last_date, max_groups.
    _updateEntryTransitive(entry, sd, alg) {
        const today = this._dateStr();
        if (sd.groups > (entry.max_groups || 0)) entry.max_groups = sd.groups;
        const oldT = entry.t_mastery || 0;
        if (sd.accuracy >= alg.pass_accuracy && sd.score > oldT) {
            entry.t_mastery = oldT + (sd.score - oldT) * alg.mastery_growth;
            entry.last_updated_mastery = today;
        }
        entry.t_last_date = today;
    }

    // ── Output helpers ────────────────────────────────────────────────────────

    _statsLine(stats, chunkHash) {
        const alg    = stats.algorithm;
        const today  = this._dateStr();
        const d      = stats.daily[today] || { score: 0, duration: 0 };
        const streak = this._calculateStreak(stats);
        let chunkStr = "";
        if (chunkHash && stats.chunks[chunkHash]) {
            const c     = stats.chunks[chunkHash];
            const power = this._calculatePower(c, alg);
            chunkStr = ` chunk=${chunkHash}[ivl=${Math.floor(c.ivl || 0)},ease=${(c.ease || alg.ease_initial).toFixed(2)},mastery=${this._normalize(c.mastery || 0, c).toFixed(2)},power=${this._normalize(power, c).toFixed(2)}]`;
        }
        return `STATS time=${Math.round(Date.now() / 1000)} total_today=${d.score.toFixed(2)} goal=${alg.score_goal.toFixed(2)} total_duration_today=${d.duration.toFixed(3)} streak=${streak} mastery_thresh=${alg.chunk_mastery_thresh} power_thresh=${alg.chunk_power_thresh}${chunkStr}`;
    }

    // ── Suggestion engine ─────────────────────────────────────────────────────

    _skillRank(hash, skillOrderStr) {
        const skills = this._chunkSkills[hash];
        if (!skills) return -1;
        const order = {};
        let n = 0;
        for (const sk of skillOrderStr.split(/\s+/)) order[sk] = n++;
        let rank = -1;
        for (const sk of skills.split(/\s+/)) {
            const r = order[sk] !== undefined ? order[sk] : n;
            if (r > rank) rank = r;
        }
        return rank;
    }

    _suggestBestHash(stats) {
        const alg = stats.algorithm;
        let bestUnmastered = null, bestNew = null;
        let allPracticedMastered = true;

        for (const [h, c] of Object.entries(stats.chunks)) {
            const isNew  = !c.last_date && !c.t_last_date;
            const mPct   = this._normalize(Math.max(c.mastery || 0, c.t_mastery || 0), c);
            const level  = c.level || 0;

            if (isNew) {
                const sr = this._skillRank(h, alg.skill_order);
                if (!bestNew
                    || level < bestNew.level
                    || (level === bestNew.level && sr < bestNew.sr)
                    || (level === bestNew.level && sr === bestNew.sr && h < bestNew.hash))
                    bestNew = { hash: h, level, sr };
            } else {
                if (mPct < alg.chunk_mastery_thresh) {
                    allPracticedMastered = false;
                    const p = this._calculateEffectivePower(c, alg);
                    if (!bestUnmastered
                        || level > bestUnmastered.level
                        || (level === bestUnmastered.level && p < bestUnmastered.p)
                        || (level === bestUnmastered.level && p === bestUnmastered.p && h < bestUnmastered.hash))
                        bestUnmastered = { hash: h, level, p };
                }
            }
        }

        if (!allPracticedMastered) return bestUnmastered ? bestUnmastered.hash : null;
        if (bestNew) return bestNew.hash;
        return null;
    }

    _handleSuggest(stats) {
        const best = this._suggestBestHash(stats);
        if (best) {
            const c     = stats.chunks[best];
            const isNew = !c.last_date && !c.t_last_date;
            this._onLine(`SUGGESTION chunk=${best} level=${c.level || 0} skills=${this._chunkSkills[best] || "?"} reason=${isNew ? "new_chunk" : "weak_chunk"}`);
        } else {
            this._onLine("SUGGESTION none reason=all_up_to_date");
        }
    }

    // ── Lesson finalisation ───────────────────────────────────────────────────

    _finalize(stats) {
        const cur = this._current;
        if (!cur || cur.maxBassId < 0) { this._current = null; return; }

        const sd = this._computeScoreData(cur.results, 0, cur.maxBassId);
        if (!sd) { this._current = null; return; }

        const alg   = stats.algorithm;
        const today = this._dateStr();
        const hash  = cur.id;

        const c = stats.chunks[hash]
            || { ease: alg.ease_initial, ivl: 0, mastery: 0, total_duration: 0, max_groups: 0 };
        c.n_consecutive = alg.last_lesson_scored === hash ? (c.n_consecutive || 0) + 1 : 1;
        alg.last_lesson_scored = hash;

        const res = this._updateEntry(c, sd, alg);

        // Collect failed groups; apply power_factor penalty
        const failedGroups = {};
        for (const [i, r] of Object.entries(cur.results))
            if (r.status === "FAIL") failedGroups[i] = true;

        if (sd.accuracy === 100) c.power_factor = 1.0;
        else c.power_factor = Math.max(0, (c.power_factor || 1.0) * (1.0 - alg.mistake_power_penalty));

        // Update EMA BPM
        if (sd.duration > 0 && cur.totalBeats > 0) {
            const actualBpm = cur.totalBeats * (cur.timeDenom / 4.0) * 60.0 / sd.duration;
            c.ema_bpm = alg.ema_alpha * actualBpm + (1.0 - alg.ema_alpha) * (c.ema_bpm || cur.refBpm);
        }

        stats.chunks[hash] = c;
        const points = this._normalize(res.mDelta, c) * alg.mastery_points_per_pct
                     + this._normalize(res.pDelta, c) * alg.power_points_per_pct;
        stats.daily[today] = stats.daily[today] || { score: 0, duration: 0 };
        stats.daily[today].score    += points;
        stats.daily[today].duration += sd.duration;
        this._save(stats);

        const power  = this._calculatePower(c, alg);
        const d      = stats.daily[today];
        const streak = this._calculateStreak(stats);
        const ema    = c.ema_pass || 1.0;
        const required = alg.overlearn_min + (alg.overlearn_max - alg.overlearn_min) * (1.0 - ema);
        let suggestion = "";
        if (c.n_consecutive >= required) {
            const best = this._suggestBestHash(stats);
            if (best && best !== hash) suggestion = " suggestion=try_another_lesson";
        }

        this._onLine(
            `STATS time=${Math.round(Date.now() / 1000)} total_today=${d.score.toFixed(2)} goal=${alg.score_goal.toFixed(2)} total_duration_today=${d.duration.toFixed(3)} streak=${streak} mastery_thresh=${alg.chunk_mastery_thresh} power_thresh=${alg.chunk_power_thresh} chunk=${hash}[ivl=${Math.floor(c.ivl || 0)},ease=${(c.ease || alg.ease_initial).toFixed(2)},mastery=${this._normalize(c.mastery || 0, c).toFixed(2)},power=${this._normalize(power, c).toFixed(2)}]${suggestion}`
        );

        const savedResults   = cur.results;
        const savedMaxBassId = cur.maxBassId;
        this._current = null;
        this._pendingChildren[hash] = { results: savedResults, abs_s: 0, abs_e: savedMaxBassId, failedGroups };
        this._send(`QUERY_CHILDREN ${hash}`);
    }

    // ── Main line handler ─────────────────────────────────────────────────────

    handleLine(line) {
        line = line.trim();
        if (!line) return;

        if (line.startsWith("CHUNK_NAME ")) {
            const parts     = line.split(/\s+/);
            const h         = parts[1];
            const level     = parseInt(parts[2]) || 0;
            const skillsStr = parts.slice(3).join(" ");
            if (skillsStr && !this._chunkSkills[h]) this._chunkSkills[h] = skillsStr;
            const stats     = this._load();
            let needsSave   = false;
            if (!stats.chunks[h]) {
                stats.chunks[h] = { level, ease: stats.algorithm.ease_initial, ivl: 0, mastery: 0, total_duration: 0, max_groups: 0 };
                needsSave = true;
            } else if (stats.chunks[h].level === undefined) {
                stats.chunks[h].level = level; needsSave = true;
            }
            if (skillsStr && stats.chunks[h].skills !== skillsStr) {
                stats.chunks[h].skills = skillsStr; needsSave = true;
            }
            if (needsSave) this._save(stats);

        } else if (line.startsWith("LOAD_CHUNK ")) {
            const h = line.split(/\s+/)[1];
            if (h) {
                this._currentLoaded = h;
                this._onLine(this._statsLine(this._load(), h));
            }

        } else if (line.startsWith("LESSON ")) {
            const parts    = line.split(/\s+/);
            const chunkId  = parts[1];
            const timeStr  = parts[3];
            const refBpm   = parseFloat(parts[4]) || 120;
            const timeDenom = parseInt((timeStr || "").split("/")[1]) || 4;
            // Finalise any abandoned in-progress session
            if (this._current && this._current.maxBassId >= 0 && !this._current.results[this._current.maxBassId])
                this._finalize(this._load());
            this._current = { id: chunkId, maxBassId: -1, results: {}, refBpm, timeDenom, totalBeats: 0 };
            // Emit BPM: use EMA if available, otherwise reference BPM
            const bpmChunk = this._load().chunks[chunkId];
            const bpmOut   = (bpmChunk && bpmChunk.ema_bpm) || refBpm;
            this._onLine(`BPM ${bpmOut.toFixed(2)}`);

        } else if (line.startsWith("BASSNOTE ")) {
            if (this._current) {
                const m = line.match(/^BASSNOTE (\d+):/);
                if (m) {
                    const bid = parseInt(m[1]);
                    if (bid > this._current.maxBassId) this._current.maxBassId = bid;
                }
                const tm = line.match(/^BASSNOTE \d+: (\S+)/);
                if (tm) {
                    const tok = tm[1].replace(/p$/, "");
                    const dm  = tok.match(/(\d+)(\.*)$/);
                    if (dm) {
                        let beats = 4.0 / parseInt(dm[1]);
                        for (const _ of dm[2]) beats *= 1.5;
                        this._current.totalBeats += beats;
                    }
                }
            }

        } else if (line.startsWith("RESULT ")) {
            if (this._current) {
                const m = line.match(/^RESULT (\d+) TIME:(\d+)/);
                if (m) {
                    const id = parseInt(m[1]);
                    this._current.results[id] = {
                        status: line.includes(" OK") ? "OK" : "FAIL",
                        time:   parseInt(m[2]),
                    };
                    if (id === this._current.maxBassId) this._finalize(this._load());
                }
            }

        } else if (line.startsWith("CHILDREN ")) {
            const parts     = line.split(/\s+/);
            const phash     = parts[1];
            if (!phash) return;
            const childList = [];
            for (const entry of parts.slice(2)) {
                const m = entry.match(/^([0-9a-f]+):(\d+):(\d+)$/);
                if (m) childList.push({ hash: m[1], s: parseInt(m[2]), e: parseInt(m[3]) });
            }

            // Update children map
            this._childrenOf[phash] = childList.map(ci => ci.hash);

            const pending = this._pendingChildren[phash];
            delete this._pendingChildren[phash];

            if (!pending) {
                // Proactive CHILDREN from all.lua during scan: persist child hashes
                if (childList.length > 0) {
                    const stats = this._load();
                    if (stats.chunks[phash]) {
                        stats.chunks[phash].children = childList.map(ci => ci.hash);
                        this._save(stats);
                    }
                }
                return;
            }

            // Transitive update: update each child's stats entry
            const stats = this._load();
            for (const ci of childList) {
                const abs_s = pending.abs_s + ci.s;
                const abs_e = pending.abs_s + ci.e;
                const sd    = this._computeScoreData(pending.results, abs_s, abs_e);
                const alg   = stats.algorithm;
                const c     = stats.chunks[ci.hash]
                    || { ease: alg.ease_initial, ivl: 0, mastery: 0, total_duration: 0, max_groups: 0 };
                if (sd) this._updateEntryTransitive(c, sd, alg);

                // Power factor: penalise if any failed group falls in this child's range
                let childHasFail = false;
                for (const gi of Object.keys(pending.failedGroups))
                    if (parseInt(gi) >= abs_s && parseInt(gi) <= abs_e) { childHasFail = true; break; }

                if (childHasFail)
                    c.power_factor = Math.max(0, (c.power_factor || 1.0) * (1.0 - alg.mistake_power_penalty));
                else
                    c.power_factor = 1.0;

                // SRS scheduling from parent session result
                if (sd) {
                    if (!childHasFail && sd.accuracy === 100) {
                        c.ivl  = c.ivl ? Math.min(alg.ivl_max, Math.ceil(c.ivl * (c.ease || alg.ease_initial))) : alg.ivl_first;
                        c.ease = Math.min(alg.ease_max, (c.ease || alg.ease_initial) + alg.ease_pass_delta);
                    } else if (childHasFail) {
                        c.ivl  = 1;
                        c.ease = Math.max(alg.ease_min, (c.ease || alg.ease_initial) - alg.ease_fail_delta);
                    }
                }

                stats.chunks[ci.hash] = c;
                this._pendingChildren[ci.hash] = { results: pending.results, abs_s, abs_e, failedGroups: pending.failedGroups };
                this._send(`QUERY_CHILDREN ${ci.hash}`);
            }
            this._save(stats);

        } else if (line.startsWith("QUERY_STATS")) {
            this._onLine(this._statsLine(this._load(), this._currentLoaded));

        } else if (line.startsWith("SUGGEST_LESSON")) {
            this._handleSuggest(this._load());
        }
    }
}
