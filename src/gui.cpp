// SPDX-License-Identifier: MIT
// gui.cpp --- OpenGL/ImGui continuo trainer front-end
// Copyright (c) 2026 Jakob Kastelic

// DESCRIPTION
//     gui is the interactive front-end for the continuo trainer.  It opens
//     a GLFW window showing the current lesson or chunk score image and a
//     row of green/red squares tracking pass/fail results.  A status bar
//     shows mastery, power, today's score, streak, and coaching suggestions.
//
//     All pipeline communication is via stdin/stdout.  Stdin is polled
//     non-blocking so the UI remains responsive.
//
// KEYBOARD SHORTCUTS
//     ESC / X     Close the current lesson display (X also clicks the X button).
//     R           Reload: re-emit LOAD_CHUNK for the current item.
//     SPACE       Practice: SUGGEST_LESSON.  Performance: start/abort karaoke.
//     K           Toggle karaoke mode (emits KARAOKE_ON / KARAOKE_OFF).
//     P           Toggle practice/performance mode (emits MODE).
//     S           Toggle settings screen (MIDI devices, algorithm params).
//
// RECEIVED (stdin, from stats.lua, all.lua, and bin/karaoke)
//     BASSNOTE <i>: <token> [passing]
//         Updates the expected note count for progress-square sizing.
//     RESULT <i> TIME:<t> OK|FAIL [<message>]
//         Updates the pass/fail square for group <i>.  On the last result,
//         automatically reloads the current lesson or chunk.
//     STATS time=<t> total_today=<n.nn> goal=<n.nn> streak=<n>
//           [lesson=<id>[...,mastery=<n>,power=<n>]] [suggestion=<token>]
//           [chunk=<hash>[...,mastery=<n>,power=<n>]]
//         Updates the status bar.  Points delta animation fires when
//         today's score increases after the first stats load.
//     SUGGESTION chunk=<hash> skills=<s> reason=<r>
//         Enters chunk mode: sets current_chunk=<hash>, emits LOAD_CHUNK,
//         flashes skills as status text.
//     SUGGESTION lesson=<id> reason=<r>
//         Loads lesson <id> and flashes the reason string.
//     SUGGESTION none reason=<token>
//         Flashes the reason string without changing lesson.
//
// EMITTED (stdout, to all.lua and stats.lua)
//     LOAD_CHUNK <hash>     Load chunk file chn/<hash>.txt.
//     SUGGEST_LESSON        Request the best item to practice (chunk or
//     lesson). QUERY_STATS           Request a stats refresh (at 2:00 AM).
//     KARAOKE_ON            Enable karaoke mode.
//     KARAOKE_OFF           Disable karaoke mode.
//
// RECEIVED (stdin, from stats.lua, bin/load, and bin/karaoke)
//     KARAOKE_DONE          Karaoke playback finished; resets button to normal.
//
// FILES
//     seq/<n>.png     Score image for lesson n.
//     chn/<hash>.png  Score image for chunk <hash>.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <ctime>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_LINES 128
#define MAX_LEN 2048

struct Square {
	enum State { OK, FAIL, DONE };
	State state;
};

struct status_line {
	// current lesson
	float acc = 0.0f;
	float slowest = 0.0f;
	float mastery = 0.0f;
	float power = 0.0f;
	float ema_bpm = 0.0f;
	float ema_evenness = 0.0f;
	float perf_power = 0.0f;
	float perf_ema_bpm = 0.0f;
	float perf_ema_evenness = 0.0f;

	// today
	int pts = 0;
	float goal = 0.0f;
	int pts_delta = 0; // points earned on the last completed lesson
	double pts_delta_time =
	    -1e9; // initialised far in the past; never triggers on startup
	bool goal_met = false;
	int streak = 0;

	// thresholds from STATS line
	float mastery_thresh = 80.0f;
	float power_thresh = 70.0f;

	// coaching suggestion from last STATS line
	char suggestion[256] = "";
	double suggestion_time = -1e9;
};

struct state {
	bool running = true;
	char level0_hashes[256][64]; // level-0 chunk hashes in arrival order
	int num_level0 = 0;
	int current_level0_idx = 0;
	char current_chunk[64] =
	    ""; // non-empty while a chunk session is active (from SUGGESTION)
	struct Square squares[MAX_LINES];
	int num_squares = 0;
	int num_notes = 0;
	char explanation[MAX_LINES * MAX_LEN];
	struct status_line status;
	int image_width = 0;
	bool triggered_today = false;
	bool stats_initialized = false; // true after first STATS line;
					// suppresses startup celebration
	bool karaoke_on = false;
	int current_chunk_level = -1; // level from last SUGGESTION chunk= line
	float bpm = 120.0f;           // current BPM; updated by BPM command

	// Practice/Performance mode
	enum Mode { MODE_PRACTICE, MODE_PERFORMANCE } mode = MODE_PRACTICE;

	// Badge state (from BADGE messages)
	bool badge_p = false, badge_s = false, badge_e = false;     // practice
	bool badge_pp = false, badge_ps = false, badge_pe = false;  // performance
	bool badge_graduated = false;
	bool badge_mastered = false;

	// Badge progress (from BADGE_PROGRESS messages or computed)
	char badge_progress_tag[4] = "";
	float badge_progress_cur = 0.0f;
	float badge_progress_tgt = 0.0f;

	// Badge celebration overlay
	char badge_celebration[64] = "";
	double badge_celebration_time = -1e9;

	// Performance feedback overlay
	char perf_feedback[64] = "";
	char perf_detail[128] = "";  // per-note timing summary
	double perf_feedback_time = -1e9;


	// MIDI devices (populated from DEVICE_AVAIL lines)
	char midi_names[32][128];
	int  midi_count  = 0;
	int  midi_in     = -1;  // selected input  device index, -1 = none
	int  midi_out    = -1;  // selected output device index, -1 = none
	bool midi_fwd    = false;

	// Synth volume (forwarded as SET MASTER_GAIN; 0 = off)
	float synth_vol = 0.3f;

	// Per-skill mastery (from SKILL_STATS lines emitted by stats.lua)
	struct SkillStat { char name[32]; float mastery; };
	SkillStat skill_stats[32];
	int num_skills = 0;

	// Level-up tracking: current_level = index of first unmastered skill
	int current_level = -1;      // -1 = not yet computed; 0+ = level index
	char levelup_text[64] = "";  // celebration text
	double levelup_time = -1e9;
	int skills_expected = 0;     // num skills from first full batch (0 = unknown)

	// Settings screen
	bool settings_open = false;
	int  settings_tab = 0;  // 0 = MIDI, 1 = Algorithm

	// MIDI event log (ring buffer for settings display)
	char midi_log[16][128];
	int  midi_log_count = 0;
	int  midi_log_start = 0;  // oldest entry index in ring

	// Algorithm parameters (from ALG_PARAMS)
	struct AlgParam { char key[40]; float value; char tooltip[128]; bool is_string; char str_value[64]; };
	AlgParam alg_params[64];
	int num_alg_params = 0;
	bool alg_params_loaded = false;
} state;

#define GUI_SETTINGS_PATH "log/gui_settings.log"

static void save_gui_settings(void)
{
	FILE *f = fopen(GUI_SETTINGS_PATH, "w");
	if (!f) return;
	fprintf(f, "VOLUME %.3f\n", state.synth_vol);
	fclose(f);
}

static void load_gui_settings(void)
{
	FILE *f = fopen(GUI_SETTINGS_PATH, "r");
	if (!f) return;
	char line[64];
	while (fgets(line, sizeof(line), f)) {
		float v;
		if (sscanf(line, "VOLUME %f", &v) == 1)
			state.synth_vol = v;
	}
	fclose(f);
}

static void midi_log_push(const char *msg)
{
	int idx = (state.midi_log_start + state.midi_log_count) % 16;
	if (state.midi_log_count == 16)
		state.midi_log_start = (state.midi_log_start + 1) % 16;
	else
		state.midi_log_count++;
	strncpy(state.midi_log[idx], msg, 127);
	state.midi_log[idx][127] = '\0';
	// Strip trailing newline
	char *nl = strpbrk(state.midi_log[idx], "\r\n");
	if (nl) *nl = '\0';
}

static const char *alg_tooltip(const char *key)
{
	struct { const char *k; const char *t; } tips[] = {
		{"score_goal", "Daily point target"},
		{"ema_alpha", "EMA smoothing for pass-rate (~10-session window)"},
		{"pass_accuracy", "Minimum accuracy % to count as a pass"},
		{"mastery_growth", "Fraction of (score - mastery) gap closed per session"},
		{"power_half_life", "ln(2): power reaches 50% of mastery when days_elapsed == interval"},
		{"mastery_points_per_pct", "Points awarded per 1% normalised mastery gain"},
		{"power_points_per_pct", "Points awarded per 1% normalised power gain"},
		{"bottleneck_thresh", "Factor value below which it is considered a bottleneck"},
		{"dominance_margin", "How much lower the bottleneck must be than the others"},
		{"min_quality", "Quality below this triggers a 'raise quality' suggestion"},
		{"power_score_frac", "Skip suggesting chunks whose power >= this fraction of mastery"},
		{"overlearn_min", "Min consecutive attempts before suggesting another chunk (ema_pass=1)"},
		{"overlearn_max", "Max consecutive attempts before suggesting another chunk (ema_pass=0)"},
		{"mistake_power_penalty", "Power factor multiplied by (1 - penalty) per failed session"},
		{"mastery_decay_half_life", "Days for mastery to halve without a mastery-improving session"},
		{"skill_order", "Order in which skills are introduced to the student (space-separated).\nEarlier skills are suggested before later ones for new chunks."},
		{"weak_ema_thresh", "ema_pass below this marks a lesson as 'needs work'"},
		{"ease_initial", "Starting ease factor for new lessons (SM-2)"},
		{"ease_min", "Minimum ease factor"},
		{"ease_max", "Maximum ease factor"},
		{"ease_pass_delta", "Ease increase on a perfect pass"},
		{"ease_fail_delta", "Ease decrease on a fail"},
		{"ivl_first", "Interval (days) after first perfect pass"},
		{"ivl_second", "Interval (days) after second consecutive perfect pass"},
		{"ivl_max", "Maximum interval (days); caps runaway SRS growth"},
		{"chunk_mastery_thresh", "Normalised % below which a chunk needs practice"},
		{"chunk_power_thresh", "Normalised % below which a chunk needs practice"},
		{"midnight_time", "Hour (UTC) at which the practice day resets (0 = midnight)"},
		{"ema_evenness_alpha", "EMA window for evenness tracking (~10 attempts)"},
		{"badge_power_thresh", "Power % to earn practice P badge"},
		{"badge_speed_thresh", "EMA BPM to earn practice S badge"},
		{"badge_evenness_thresh", "EMA evenness to earn practice E badge"},
		{"perf_badge_power_thresh", "Power % to earn performance P' badge"},
		{"perf_badge_speed_thresh", "EMA perf BPM to earn performance S' badge"},
		{"perf_badge_evenness_thresh", "EMA perf evenness to earn performance E' badge"},
		{"badge_power_bonus", "Points awarded on earning P / P' badge"},
		{"badge_speed_bonus", "Points awarded on earning S / S' badge"},
		{"badge_evenness_bonus", "Points awarded on earning E / E' badge"},
		{"badge_graduate_bonus", "Extra points on completing practice badge set"},
		{"badge_mastered_bonus", "Extra points on completing all 6 badges"},
		{"perf_fail_accuracy", "Timing accuracy % below which performance attempt fails"},
		{"perf_fail_timing_tol", "Max |actual-expected|/expected per beat"},
		{"perf_fail_streak", "Consecutive fails before suggesting practice mode"},
		{"rotation_pool_size", "Active rotation pool size for interleaved practice"},
		{"min_away", "Min attempts on other chunks before returning to a rotated-out chunk"},
		{"length_bonus_threshold", "Lesson groups up to this count get no length bonus"},
		{"length_bonus_per_group", "Extra point fraction per group above the threshold (e.g. 0.25 = +25%)"},
		{NULL, NULL}
	};
	for (int i = 0; tips[i].k; i++)
		if (strcmp(tips[i].k, key) == 0)
			return tips[i].t;
	return key;
}

static const char *skill_display_name(const char *sk)
{
	struct { const char *k; const char *d; } names[] = {
		{"root", "Root Position"},
		{"6", "Sixth Chords"},
		{"4-3_sus", "4-3 Suspensions"},
		{"6/4", "Six-Four Chords"},
		{"7", "Seventh Chords"},
		{"7-6_sus", "7-6 Suspensions"},
		{"4", "Fourth Chords"},
		{"other", "Advanced"},
		{NULL, NULL}
	};
	for (int i = 0; names[i].k; i++)
		if (strcmp(names[i].k, sk) == 0)
			return names[i].d;
	return sk;
}

// Recompute current_level from skill mastery; detect level-ups.
static void update_skill_level(void)
{
	if (state.num_skills == 0) return;

	// Don't compute level until we've seen a full batch of skills
	if (state.skills_expected == 0) return;

	float thresh = state.status.mastery_thresh;
	int old_level = state.current_level;
	int new_level = 0;

	for (int i = 0; i < state.num_skills; i++) {
		if (state.skill_stats[i].mastery >= thresh)
			new_level = i + 1;
		else
			break;
	}

	state.current_level = new_level;

	// Detect level-up (but not on first load)
	if (old_level >= 0 && new_level > old_level && state.stats_initialized) {
		if (new_level < state.num_skills) {
			snprintf(state.levelup_text, sizeof(state.levelup_text),
				 "Level Up! %s",
				 skill_display_name(state.skill_stats[new_level].name));
		} else {
			snprintf(state.levelup_text, sizeof(state.levelup_text),
				 "All Skills Mastered!");
		}
		state.levelup_time = glfwGetTime();
	}
}

static float alg_param_value(const char *key, float fallback)
{
	for (int i = 0; i < state.num_alg_params; i++)
		if (!state.alg_params[i].is_string && strcmp(state.alg_params[i].key, key) == 0)
			return state.alg_params[i].value;
	return fallback;
}

static void clear_badges(void)
{
	state.badge_p = state.badge_s = state.badge_e = false;
	state.badge_pp = state.badge_ps = state.badge_pe = false;
	state.badge_graduated = state.badge_mastered = false;
	state.badge_progress_tag[0] = '\0';
}

static void clear_status(void)
{
	state.num_squares = 0;
	state.explanation[0] = '\0';
	state.status.slowest = 0;
}

static void quit_lesson(void)
{
	if (state.karaoke_on) {
		printf("KARAOKE_OFF\n");
		fflush(stdout);
		state.karaoke_on = false;
	}
	clear_status();
	state.running = false;
}

static void reload_lesson(void)
{
	if (state.current_chunk[0])
		printf("LOAD_CHUNK %s\n", state.current_chunk);
	else if (state.num_level0 > 0)
		printf("LOAD_CHUNK %s\n",
		       state.level0_hashes[state.current_level0_idx]);
	fflush(stdout);
	clear_status();
}

static void suggest_lesson(void)
{
	printf("KARAOKE_OFF\n");
	printf("SUGGEST_LESSON\n");
	fflush(stdout);
}

static void toggle_karaoke(void)
{
	state.karaoke_on = !state.karaoke_on;
	if (state.karaoke_on)
		printf("KARAOKE_ON\n");
	else
		printf("KARAOKE_OFF\n");
	fflush(stdout);
}

static void toggle_mode(void)
{
	if (state.mode == state::MODE_PRACTICE) {
		state.mode = state::MODE_PERFORMANCE;
		printf("MODE performance\n");
		// If not all practice badges, flash a warning but allow
		if (!state.badge_graduated) {
			strncpy(state.status.suggestion, "earn_badges_first",
				sizeof(state.status.suggestion) - 1);
			state.status.suggestion_time = glfwGetTime();
		}
	} else {
		state.mode = state::MODE_PRACTICE;
		printf("MODE practice\n");
	}
	fflush(stdout);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
			 int mods)
{
	if (action == GLFW_PRESS) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
		case GLFW_KEY_X:
			quit_lesson();
			break;
		case GLFW_KEY_R:
			reload_lesson();
			break;
		case GLFW_KEY_SPACE:
			if (state.mode == state::MODE_PERFORMANCE) {
				// In performance mode: Space starts/aborts karaoke
				if (state.karaoke_on) {
					// Abort: stop playback + MIDI panic, no scoring
					printf("KARAOKE_STOP\n");
					fflush(stdout);
					state.karaoke_on = false;
				} else {
					// Start performance attempt
					printf("KARAOKE_ON\n");
					fflush(stdout);
					state.karaoke_on = true;
				}
			} else {
				suggest_lesson();
			}
			break;
		case GLFW_KEY_K:
			if (state.mode == state::MODE_PRACTICE)
				toggle_karaoke();
			break;
		case GLFW_KEY_P:
			toggle_mode();
			break;
		case GLFW_KEY_S:
			state.settings_open = !state.settings_open;
			if (state.settings_open) {
				printf("MUTE\n");
				if (!state.alg_params_loaded)
					printf("QUERY_ALG\n");
			} else {
				printf("UNMUTE\n");
			}
			fflush(stdout);
			break;
		}
	}
}

static void handle_result(const char *buf)
{
	// RESULT <id> TIME:<t> <OK|FAIL> [error message]
	int id;
	if (sscanf(buf, "RESULT %d", &id) != 1)
		return;

	if (id == 0) {
		clear_status();
		state.status.pts_delta = 0;
	}

	bool ok = (strstr(buf, " OK") != NULL || strstr(buf, "\tOK") != NULL ||
		   strstr(buf, "mOK") != NULL);

	// In performance mode, forward result timestamps for scoring
	if (state.mode == state::MODE_PERFORMANCE) {
		long long rtime = 0;
		const char *tp = strstr(buf, "TIME:");
		if (tp) sscanf(tp, "TIME:%lld", &rtime);
		printf("PERF_RESULT %d %lld %s\n", id, rtime,
		       ok ? "OK" : "FAIL");
		fflush(stdout);
	}

	if (state.num_squares < MAX_LINES) {
		if (ok)
			state.squares[state.num_squares].state = Square::OK;
		else
			state.squares[state.num_squares].state = Square::FAIL;
		state.num_squares = id + 1;
	}

	if (id + 1 == state.num_notes + 1) {
		state.squares[id + 1].state = Square::State::DONE;
		state.num_squares = id + 2;
		if (state.current_chunk[0])
			printf("LOAD_CHUNK %s\n", state.current_chunk);
		else if (state.num_level0 > 0)
			printf("LOAD_CHUNK %s\n",
			       state.level0_hashes[state.current_level0_idx]);
	}

	if (!ok) {
		const char *fail = strstr(buf, "FAIL");
		const char *msg = fail ? strchr(fail, ' ') : NULL;
		if (msg) {
			msg++; // skip space after FAIL
			size_t len = strlen(state.explanation);
			snprintf(state.explanation + len,
				 sizeof(state.explanation) - len - 1, "%d\t%s",
				 id, msg);
		}
	}

	if (state.num_squares == 1)
		state.status.slowest = 0;
}

static void handle_score(const char *buf)
{
	// SCORE time=<t> accuracy=<n> slowest=<s.ss> ...
	if (strncmp(buf, "SCORE", 5) != 0)
		return;

	const char *p;
	p = strstr(buf, "accuracy=");
	if (p)
		sscanf(p, "accuracy=%f", &state.status.acc);

	p = strstr(buf, "slowest=");
	if (p)
		sscanf(p, "slowest=%f", &state.status.slowest);
}

static void handle_stats(const char *buf)
{
	// STATS time=<t> total_today=<n.nn> goal=<n.nn> streak=<n>
	//       [lesson=<id>[ivl=<n>,ease=<n>,tot_dur=<s>,mastery=<n>,power=<n>]]
	if (strncmp(buf, "STATS", 5) != 0)
		return;

	float total = 0.0f, goal = 0.0f;
	const char *p;

	p = strstr(buf, "total_today=");
	if (p)
		sscanf(p, "total_today=%f", &total);

	p = strstr(buf, "goal=");
	if (p) {
		sscanf(p, "goal=%f", &goal);
		state.status.goal = goal;
	}

	p = strstr(buf, "streak=");
	if (p)
		sscanf(p, "streak=%d", &state.status.streak);

	p = strstr(buf, "mastery_thresh=");
	if (p)
		sscanf(p, "mastery_thresh=%f", &state.status.mastery_thresh);

	p = strstr(buf, "power_thresh=");
	if (p)
		sscanf(p, "power_thresh=%f", &state.status.power_thresh);

	// mastery= and power= live inside the chunk/lesson bracket, e.g.
	// chunk=<hash>[ivl=6,ease=2.60,mastery=18.50,power=9.20]
	p = strstr(buf, "mastery=");
	if (p)
		sscanf(p, "mastery=%f", &state.status.mastery);

	p = strstr(buf, "power=");
	if (p)
		sscanf(p, "power=%f", &state.status.power);

	p = strstr(buf, "ema_bpm=");
	if (p)
		sscanf(p, "ema_bpm=%f", &state.status.ema_bpm);

	p = strstr(buf, "ema_evenness=");
	if (p)
		sscanf(p, "ema_evenness=%f", &state.status.ema_evenness);

	p = strstr(buf, "perf_power=");
	if (p)
		sscanf(p, "perf_power=%f", &state.status.perf_power);

	p = strstr(buf, "perf_ema_bpm=");
	if (p)
		sscanf(p, "perf_ema_bpm=%f", &state.status.perf_ema_bpm);

	p = strstr(buf, "perf_ema_evenness=");
	if (p)
		sscanf(p, "perf_ema_evenness=%f", &state.status.perf_ema_evenness);

	p = strstr(buf, "suggestion=");
	if (p) {
		char raw[32] = "";
		sscanf(p, "suggestion=%31s", raw);
		strncpy(state.status.suggestion, raw,
			sizeof(state.status.suggestion) - 1);
		state.status.suggestion[sizeof(state.status.suggestion) - 1] =
		    '\0';
		state.status.suggestion_time = glfwGetTime();
	}

	int new_pts = (int)(total + 0.5f);
	// Accumulate the delta — do NOT clear it here. The second STATS line
	// (from LOAD_LESSON) will add 0, leaving the delta intact until the
	// next attempt starts. Clearing happens in handle_result on id == 0.
	state.status.pts_delta += new_pts - state.status.pts;
	// Only celebrate points earned during this session, never on the
	// initial load of existing totals from the stats file.
	if (state.stats_initialized && state.status.pts_delta > 0 &&
	    new_pts > state.status.pts)
		state.status.pts_delta_time = glfwGetTime();
	state.status.pts = new_pts;
	state.status.goal_met = (total >= goal && goal > 0.0f);
	state.stats_initialized = true;
}

static void handle_lesson(const char *buf)
{
	if (strncmp(buf, "LESSON", 6) != 0)
		return;
}

static void handle_bassnote(const char *buf)
{
	const char cmd[] = "BASSNOTE ";
	size_t cmd_len = sizeof(cmd) - 1;

	if (strncmp(buf, cmd, cmd_len) != 0)
		return;

	const char *p = buf + cmd_len;
	sscanf(p, "%d", &state.num_notes);
}

static void handle_suggestion(const char *buf)
{
	// SUGGESTION chunk=<hash> skills=<s> reason=<r>
	// SUGGESTION lesson=<id> reason=<r>
	// SUGGESTION none reason=<token>
	if (strncmp(buf, "SUGGESTION", 10) != 0)
		return;

	const char *cp = strstr(buf, "chunk=");
	if (cp) {
		char hash[64] = "";
		sscanf(cp, "chunk=%63s", hash);
		strncpy(state.current_chunk, hash,
			sizeof(state.current_chunk) - 1);
		state.current_chunk[sizeof(state.current_chunk) - 1] = '\0';
		const char *lp = strstr(buf, "level=");
		state.current_chunk_level = lp ? atoi(lp + 6) : -1;
		printf("LOAD_CHUNK %s\n", hash);
		fflush(stdout);
		clear_status();
		clear_badges();
		// Flash skills as the status message
		char skills[24] = "?";
		const char *sp = strstr(buf, "skills=");
		if (sp)
			sscanf(sp, "skills=%23s", skills);
		snprintf(state.status.suggestion,
			 sizeof(state.status.suggestion), "chunk: %s", skills);
		state.status.suggestion_time = glfwGetTime();
		return;
	}

	const char *p = strstr(buf, "lesson=");
	if (p) {
		// Flash the reason; LOAD_LESSON no longer used.
		const char *r = strstr(buf, "reason=");
		if (r) {
			char reason[32] = "";
			sscanf(r, "reason=%31s", reason);
			strncpy(state.status.suggestion, reason,
				sizeof(state.status.suggestion) - 1);
			state.status
			    .suggestion[sizeof(state.status.suggestion) - 1] =
			    '\0';
			state.status.suggestion_time = glfwGetTime();
		}
		return;
	}

	// No lesson — show the reason as a status flash.
	p = strstr(buf, "reason=");
	if (p) {
		char reason[32] = "";
		sscanf(p, "reason=%31s", reason);
		strncpy(state.status.suggestion, reason,
			sizeof(state.status.suggestion) - 1);
		state.status.suggestion[sizeof(state.status.suggestion) - 1] =
		    '\0';
		state.status.suggestion_time = glfwGetTime();
	}
}

static void handle_chunk_name(const char *buf)
{
	char hash[64];
	int level;
	if (sscanf(buf, "CHUNK_NAME %63s %d", hash, &level) != 2)
		return;
	if (level == 0 && state.num_level0 < 256) {
		strncpy(state.level0_hashes[state.num_level0], hash, 63);
		state.level0_hashes[state.num_level0][63] = '\0';
		state.num_level0++;
	}
}

static void parse_line(const char *buf)
{
	// Detect end of first SKILL_STATS batch: any non-SKILL_STATS line
	// after skills have been added means the batch is complete.
	if (state.skills_expected == 0 && state.num_skills > 0
	    && strncmp(buf, "SKILL_STATS ", 12) != 0) {
		state.skills_expected = state.num_skills;
		update_skill_level();
	}

	if (strncmp(buf, "KARAOKE_DONE", 12) == 0) {
		state.karaoke_on = false;
		// Forward to stats.lua for performance scoring (prefixed to
		// avoid re-entry via midi.c pass-through)
		printf("PERF_DONE\n");
		fflush(stdout);
		return;
	}
	// Forward KARAOKE_ABORT from karaoke to stats.lua
	if (strncmp(buf, "KARAOKE_ABORT", 13) == 0) {
		printf("PERF_ABORT\n");
		fflush(stdout);
		return;
	}
	// BADGE <tag> <pts>  — badge earned
	if (strncmp(buf, "BADGE ", 6) == 0) {
		char tag[16] = "";
		int pts = 0;
		sscanf(buf + 6, "%15s %d", tag, &pts);
		if (strcmp(tag, "P") == 0) state.badge_p = true;
		else if (strcmp(tag, "S") == 0) state.badge_s = true;
		else if (strcmp(tag, "E") == 0) state.badge_e = true;
		else if (strcmp(tag, "PP") == 0) state.badge_pp = true;
		else if (strcmp(tag, "PS") == 0) state.badge_ps = true;
		else if (strcmp(tag, "PE") == 0) state.badge_pe = true;
		else if (strcmp(tag, "READY") == 0) state.badge_graduated = true;
		else if (strcmp(tag, "MASTERED") == 0) state.badge_mastered = true;
		// Celebration overlay (pts == 0 means state reconstruction, not a new badge)
		if (pts <= 0) return;
		if (strcmp(tag, "P") == 0 || strcmp(tag, "PP") == 0)
			snprintf(state.badge_celebration, sizeof(state.badge_celebration),
				 "Power Bonus +%dpts", pts);
		else if (strcmp(tag, "S") == 0 || strcmp(tag, "PS") == 0)
			snprintf(state.badge_celebration, sizeof(state.badge_celebration),
				 "Speed Bonus +%dpts", pts);
		else if (strcmp(tag, "E") == 0 || strcmp(tag, "PE") == 0)
			snprintf(state.badge_celebration, sizeof(state.badge_celebration),
				 "Evenness Bonus +%dpts", pts);
		else if (strcmp(tag, "READY") == 0)
			snprintf(state.badge_celebration, sizeof(state.badge_celebration),
				 "Performance Ready! +%dpts [P]", pts);
		else if (strcmp(tag, "MASTERED") == 0)
			snprintf(state.badge_celebration, sizeof(state.badge_celebration),
				 "Chunk Mastered! +%dpts", pts);
		state.badge_celebration_time = glfwGetTime();
		return;
	}
	// PERF_NOTE <id> <+/-ms> <ok|late> — per-note timing detail
	if (strncmp(buf, "PERF_NOTE ", 10) == 0) {
		int id, delta;
		char status[8] = "";
		if (sscanf(buf + 10, "%d %dms %7s", &id, &delta, status) >= 2) {
			size_t len = strlen(state.perf_detail);
			if (len > 0 && len < sizeof(state.perf_detail) - 1)
				state.perf_detail[len++] = ' ';
			snprintf(state.perf_detail + len,
				 sizeof(state.perf_detail) - len,
				 strcmp(status, "ok") == 0 ? "%d:ok" : "%d:%+dms",
				 id, delta);
		}
		return;
	}
	// PERF_STATUS pass|fail <accuracy>
	if (strncmp(buf, "PERF_STATUS ", 12) == 0) {
		char result[8] = "";
		float acc = 0.0f;
		sscanf(buf + 12, "%7s %f", result, &acc);
		if (strcmp(result, "pass") == 0)
			snprintf(state.perf_feedback, sizeof(state.perf_feedback),
				 "PASS  %.0f%%", acc);
		else
			snprintf(state.perf_feedback, sizeof(state.perf_feedback),
				 "FAIL  %.0f%%", acc);
		// Show timing detail in the yellow info line
		if (state.perf_detail[0]) {
			snprintf(state.status.suggestion,
				 sizeof(state.status.suggestion),
				 "Timing: %s", state.perf_detail);
			state.status.suggestion_time = glfwGetTime();
		}
		state.perf_detail[0] = '\0';
		state.perf_feedback_time = glfwGetTime();
		// Reload chunk to reset group.rs counter for the next attempt
		if (state.current_chunk[0])
			printf("LOAD_CHUNK %s\n", state.current_chunk);
		fflush(stdout);
		return;
	}
	// MODE_SUGGEST practice|performance
	// Skip if perf feedback is active (don't overwrite timing detail)
	if (strncmp(buf, "MODE_SUGGEST ", 13) == 0) {
		double perf_age = glfwGetTime() - state.perf_feedback_time;
		if (perf_age < 3.0)
			return;
		char m[16] = "";
		sscanf(buf + 13, "%15s", m);
		if (strcmp(m, "practice") == 0) {
			strncpy(state.status.suggestion, "back_to_practice",
				sizeof(state.status.suggestion) - 1);
		} else if (strcmp(m, "performance") == 0) {
			strncpy(state.status.suggestion, "try_performance",
				sizeof(state.status.suggestion) - 1);
		}
		state.status.suggestion_time = glfwGetTime();
		return;
	}
	// INTERLEAVE <hash> — show as a yellow suggestion hint
	if (strncmp(buf, "INTERLEAVE ", 11) == 0) {
		strncpy(state.status.suggestion, "time_to_mix_it_up",
			sizeof(state.status.suggestion) - 1);
		state.status.suggestion[sizeof(state.status.suggestion) - 1] = '\0';
		state.status.suggestion_time = glfwGetTime();
		return;
	}
	if (strncmp(buf, "BPM ", 4) == 0) {
		float bpm;
		if (sscanf(buf + 4, "%f", &bpm) == 1 && bpm > 0.0f)
			state.bpm = bpm;
		return;
	}
	// Capture MIDI note events into ring buffer for settings display
	if (strncmp(buf, "NOTE_ON ", 8) == 0 || strncmp(buf, "NOTE_OFF ", 9) == 0) {
		midi_log_push(buf);
		return;
	}
	// ALG_PARAMS key1=val1\tkey2=val2\t...  (tab-delimited)
	if (strncmp(buf, "ALG_PARAMS ", 11) == 0) {
		state.num_alg_params = 0;
		const char *p = buf + 11;
		while (*p && state.num_alg_params < 64) {
			// skip tabs/spaces
			while (*p == '\t' || *p == ' ') p++;
			if (!*p || *p == '\n' || *p == '\r') break;
			char key[40] = "";
			char val[64] = "";
			int klen = 0;
			while (*p && *p != '=' && klen < 39) key[klen++] = *p++;
			key[klen] = '\0';
			if (*p == '=') p++;
			int vlen = 0;
			while (*p && *p != '\t' && *p != '\n' && *p != '\r' && vlen < 63) val[vlen++] = *p++;
			val[vlen] = '\0';
			auto &ap = state.alg_params[state.num_alg_params];
			strncpy(ap.key, key, 39); ap.key[39] = '\0';
			strncpy(ap.tooltip, alg_tooltip(key), 127); ap.tooltip[127] = '\0';
			float fval;
			if (sscanf(val, "%f", &fval) == 1 && strcmp(key, "skill_order") != 0
			    && strcmp(key, "last_lesson_scored") != 0) {
				ap.value = fval;
				ap.is_string = false;
			} else {
				strncpy(ap.str_value, val, 63); ap.str_value[63] = '\0';
				ap.is_string = true;
			}
			state.num_alg_params++;
		}
		state.alg_params_loaded = true;
		return;
	}
	if (strncmp(buf, "DEVICE_AVAIL ", 13) == 0) {
		int n;
		char name[128] = "";
		if (sscanf(buf + 13, "%d %127[^\r\n]", &n, name) >= 1 &&
		    n >= 0 && n < 32) {
			if (n == 0) state.midi_count = 0; // reset on rescan
			if (n >= state.midi_count) state.midi_count = n + 1;
			strncpy(state.midi_names[n], name, 127);
			state.midi_names[n][127] = '\0';
		}
		return;
	}
	if (strncmp(buf, "STATUS MIDI input opened: ", 26) == 0) {
		char nm[128]; strncpy(nm, buf + 26, 127); nm[127] = '\0';
		char *nl = strpbrk(nm, "\r\n"); if (nl) *nl = '\0';
		for (int i = 0; i < state.midi_count; i++)
			if (strcmp(state.midi_names[i], nm) == 0) { state.midi_in = i; break; }
		return;
	}
	if (strncmp(buf, "STATUS MIDI output opened: ", 27) == 0) {
		char nm[128]; strncpy(nm, buf + 27, 127); nm[127] = '\0';
		char *nl = strpbrk(nm, "\r\n"); if (nl) *nl = '\0';
		for (int i = 0; i < state.midi_count; i++)
			if (strcmp(state.midi_names[i], nm) == 0) { state.midi_out = i; break; }
		return;
	}
	if (strncmp(buf, "STATUS MIDI forward: ", 21) == 0) {
		state.midi_fwd = (strncmp(buf + 21, "ON", 2) == 0);
		return;
	}
	// SKILL_STATS skill=<name> mastery=<n>  (one line per skill, from stats.lua)
	if (strncmp(buf, "SKILL_STATS ", 12) == 0) {
		char sk[32] = ""; float m = 0.0f;
		const char *sp = strstr(buf, "skill=");
		const char *mp = strstr(buf, "mastery=");
		if (sp) sscanf(sp, "skill=%31s", sk);
		if (mp) sscanf(mp, "mastery=%f", &m);
		if (sk[0]) {
			bool existing = false;
			for (int i = 0; i < state.num_skills; i++) {
				if (strcmp(state.skill_stats[i].name, sk) == 0) {
					state.skill_stats[i].mastery = m;
					existing = true;
					break;
				}
			}
			if (!existing && state.num_skills < 32) {
				strncpy(state.skill_stats[state.num_skills].name, sk, 31);
				state.skill_stats[state.num_skills].name[31] = '\0';
				state.skill_stats[state.num_skills].mastery = m;
				state.num_skills++;
			}
			update_skill_level();
		}
		return;
	}
	handle_chunk_name(buf);
	handle_lesson(buf);
	handle_result(buf);
	handle_score(buf);
	handle_stats(buf);
	handle_bassnote(buf);
	handle_suggestion(buf);
}

static void check_new_day(void)
{
	std::time_t now = std::time(nullptr);
	std::tm *localTime = std::localtime(&now);

	int hour = localTime->tm_hour;
	int minute = localTime->tm_min;

	// Trigger at 2:00 AM
	if (hour == 2 && minute == 0 && !state.triggered_today) {
		printf("QUERY_STATS\n");
		state.triggered_today = true;
	}

	// Reset flag after 2:00 AM window passes
	if (hour == 2 && minute > 0) {
		state.triggered_today = false; // ready for next day
	}
}

static int LoadImage(const char *fname, GLuint *img, int *w, int *h)
{
	// Load from file to memory
	FILE *f = fopen(fname, "rb");
	if (f == NULL)
		return -1;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	if (size < 0)
		return -1;
	fseek(f, 0, SEEK_SET);
	void *data = IM_ALLOC((size_t)size);
	fread(data, 1, (size_t)size, f);
	fclose(f);

	// Load from memory
	unsigned char *image_data = stbi_load_from_memory(
	    (const unsigned char *)data, (int)size, w, h, NULL, 4);
	if (image_data == NULL)
		return -1;

	// Create a OpenGL texture identifier
	glGenTextures(1, img);
	glBindTexture(GL_TEXTURE_2D, *img);

	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Upload pixels into texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0, GL_RGBA,
		     GL_UNSIGNED_BYTE, image_data);
	stbi_image_free(image_data);

	IM_FREE(data);
	return 0;
}

static void show_music(void)
{
	static int iw = 0;
	static int ih = 0;
	static char img_path[128] = "";
	static GLuint img = 0;

	const char *hash = state.current_chunk[0]
	    ? state.current_chunk
	    : (state.num_level0 > 0
		   ? state.level0_hashes[state.current_level0_idx]
		   : "");
	if (!hash[0])
		return;
	char desired[128];
	snprintf(desired, sizeof(desired), "chn/%s.png", hash);

	if (!img || strcmp(img_path, desired) != 0) {
		if (LoadImage(desired, &img, &iw, &ih))
			return;
		strncpy(img_path, desired, sizeof(img_path) - 1);
	}

	state.image_width = iw;
	ImGui::Image((ImTextureID)(intptr_t)img, ImVec2(iw, ih));
}

static void DrawSquare(ImDrawList *draw_list, ImVec2 pos, float sz,
		       Square::State ss, int index, bool filled)
{
	// choose size
	float width = sz * 1.4f;
	float height = sz;

	// choose color
	ImU32 bg = IM_COL32(0, 0, 0, 255);
	switch (ss) {
	case Square::State::OK:
		bg = IM_COL32(0, 220, 80, 255); // green
		break;
	case Square::State::FAIL:
		bg = IM_COL32(220, 50, 50, 255); // red
		break;
	case Square::State::DONE:
		bg = IM_COL32(50, 50, 220, 255); // blue
		break;
	default:
		fprintf(stderr, "ERROR: Unknown square state\n");
		exit(1);
	}

	// choose coordinates
	ImVec2 a = ImVec2(pos.x + 1, pos.y + 1);
	ImVec2 b = ImVec2(pos.x + width - 1, pos.y + height - 1);

	// draw the square
	if (filled) {
		draw_list->AddRectFilled(a, b, bg,
					 2.0f); // Slight rounding for style
	} else {
		draw_list->AddRect(a, b, bg, 2.0f, 0, 1.5f);
	}

	// text inside the square
	char buf[12];
	if (ss == Square::State::DONE)
		snprintf(buf, sizeof(buf), "|");
	else
		snprintf(buf, sizeof(buf), "%d", index);
	ImVec2 text_size = ImGui::CalcTextSize(buf);
	ImVec2 text_pos = ImVec2(a.x + ((b.x - a.x) - text_size.x) * 0.5f,
				 a.y + ((b.y - a.y) - text_size.y) * 0.5f);

	// choose text color
	ImU32 fg = IM_COL32(0, 0, 0, 255);
	if (!filled)
		fg = bg;
	else if (ss == Square::State::DONE)
		fg = IM_COL32(255, 255, 255, 255);

	// draw text
	draw_list->AddText(text_pos, fg, buf);

	// advance cursor by the custom width
	ImGui::Dummy(ImVec2(width, height));
}

static void show_squares(void)
{
	if (state.num_squares == 0)
		return;

	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	float sz = ImGui::GetFontSize() * 1.2f;
	float square_width = sz * 1.4f + 2.0f;

	float window_x = ImGui::GetWindowPos().x;
	float window_w = ImGui::GetWindowWidth();

	for (int i = 0; i < state.num_squares; i++) {
		// Wrap to next line if this square would overflow
		float cursor_x = ImGui::GetCursorScreenPos().x;
		if (cursor_x + square_width > window_x + window_w) {
			ImGui::NewLine();
		}

		DrawSquare(draw_list, ImGui::GetCursorScreenPos(), sz,
			   state.squares[i].state, i, true);
		ImGui::SameLine(0, 2.0f);
	}
	ImGui::NewLine();
}

static const char *expand_suggestion(const char *s)
{
	if (strcmp(s, "try_again") == 0)
		return "Try again!";
	if (strcmp(s, "play_faster") == 0)
		return "Try to play faster!";
	if (strcmp(s, "play_more_evenly") == 0)
		return "Focus on an even tempo!";
	if (strcmp(s, "be_more_consistent") == 0)
		return "Be more consistent!";
	if (strcmp(s, "already_mastered") == 0)
		return "Excellent! Already mastered.";
	if (strcmp(s, "raise_quality_be_more_consistent") == 0)
		return "Good score! Play more consistently to grow mastery.";
	if (strcmp(s, "raise_quality_play_faster") == 0)
		return "Good score! Play faster to grow mastery.";
	if (strcmp(s, "raise_quality_play_more_evenly") == 0)
		return "Good score! Even up your tempo to grow mastery.";
	if (strcmp(s, "try_another_lesson") == 0)
		return "Press SPACE for a new lesson.";
	if (strcmp(s, "time_to_mix_it_up") == 0)
		return "Time to mix it up! Press SPACE.";
	if (strcmp(s, "no_lessons_available") == 0)
		return "No lessons found in seq/.";
	if (strcmp(s, "all_up_to_date") == 0)
		return "All caught up! Come back later.";
	if (strcmp(s, "back_to_practice") == 0)
		return "Back to practice? [P]";
	if (strcmp(s, "try_performance") == 0)
		return "Ready for performance! [P]";
	if (strcmp(s, "earn_badges_first") == 0)
		return "Earn all practice badges first!";
	if (strcmp(s, "overdue") == 0)
		return "Due for review.";
	if (strcmp(s, "needs_work") == 0)
		return "Needs more practice.";
	if (strcmp(s, "new_lesson") == 0)
		return "New lesson!";
	return s; // fallback: show the raw token
}

// Draw a small badge square (earned or dim)
static void DrawBadgeSquare(const char *label, bool earned, ImU32 color,
			    const char *tooltip, bool is_perf)
{
	float sz = ImGui::GetFrameHeight();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 a = ImVec2(pos.x + 1, pos.y + 1);
	ImVec2 b = ImVec2(pos.x + sz - 1, pos.y + sz - 1);
	ImDrawList *dl = ImGui::GetWindowDrawList();

	ImU32 dim = IM_COL32(60, 60, 60, 255);
	ImU32 bg = earned ? color : dim;

	if (earned) {
		dl->AddRectFilled(a, b, bg, 2.0f);
		if (is_perf)
			dl->AddRect(a, b, IM_COL32(255, 255, 255, 200), 2.0f, 0, 2.0f);
	} else {
		dl->AddRect(a, b, bg, 2.0f, 0, 1.5f);
	}

	ImVec2 tsz = ImGui::CalcTextSize(label);
	ImVec2 tpos = ImVec2(a.x + ((b.x - a.x) - tsz.x) * 0.5f,
			     a.y + ((b.y - a.y) - tsz.y) * 0.5f);
	ImU32 fg = earned ? IM_COL32(0, 0, 0, 255) : IM_COL32(100, 100, 100, 255);
	dl->AddText(tpos, fg, label);

	ImGui::Dummy(ImVec2(sz, sz));
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tooltip);
}

static void show_stats_bar(void)
{
	const status_line &s = state.status;
	float bar_w = 70.0f;
	float bar_h = ImGui::GetFrameHeight();
	float sp = 2.0f;

	// Mode indicator
	bool is_perf = (state.mode == state::MODE_PERFORMANCE);
	if (is_perf)
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "[P]erf");
	else
		ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "[P]ract");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("[P] to toggle practice/performance mode");
	ImGui::SameLine();

	// Badge row: [P] [S] [E] [P'][S'][E']
	ImU32 green  = IM_COL32(0, 220, 80, 255);
	ImU32 blue   = IM_COL32(50, 120, 220, 255);
	ImU32 orange = IM_COL32(220, 160, 30, 255);

	char tip[6][96];
	snprintf(tip[0], 96, "Power: %.1f / %.0f%%", s.power, alg_param_value("badge_power_thresh", 40));
	snprintf(tip[1], 96, "Speed: %.1f / %.0f BPM", s.ema_bpm, alg_param_value("badge_speed_thresh", 60));
	snprintf(tip[2], 96, "Evenness: %.2f / %.2f", s.ema_evenness, alg_param_value("badge_evenness_thresh", 0.70f));
	snprintf(tip[3], 96, "Perf Power: %.1f / %.0f%%", s.perf_power, alg_param_value("perf_badge_power_thresh", 60));
	snprintf(tip[4], 96, "Perf Speed: %.1f / %.0f BPM", s.perf_ema_bpm, alg_param_value("perf_badge_speed_thresh", 120));
	snprintf(tip[5], 96, "Perf Evenness: %.2f / %.2f", s.perf_ema_evenness, alg_param_value("perf_badge_evenness_thresh", 0.85f));

	DrawBadgeSquare("P", state.badge_p, green, tip[0], false);
	ImGui::SameLine(0, sp);
	DrawBadgeSquare("S", state.badge_s, blue, tip[1], false);
	ImGui::SameLine(0, sp);
	DrawBadgeSquare("E", state.badge_e, orange, tip[2], false);
	ImGui::SameLine(0, sp + 4.0f);
	DrawBadgeSquare("P", state.badge_pp, green, tip[3], true);
	ImGui::SameLine(0, sp);
	DrawBadgeSquare("S", state.badge_ps, blue, tip[4], true);
	ImGui::SameLine(0, sp);
	DrawBadgeSquare("E", state.badge_pe, orange, tip[5], true);

	// Progress bar for next unearned badge (if any)
	if (!state.badge_mastered) {
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		float frac = 0.0f;
		const char *tip_label = "";
		float tip_cur = 0.0f, tip_tgt = 0.0f;
		char bar_label[16] = "";
		if (!is_perf) {
			if (!state.badge_p) {
				tip_label = "Power";
				tip_tgt = alg_param_value("badge_power_thresh", 40);
				tip_cur = s.power;
				frac = (tip_tgt > 0) ? fminf(s.power / tip_tgt, 1.0f) : 0.0f;
				snprintf(bar_label, sizeof(bar_label), "P %.0f%%", frac * 100.0f);
			} else if (!state.badge_s) {
				tip_label = "Speed";
				tip_tgt = alg_param_value("badge_speed_thresh", 60);
				tip_cur = s.ema_bpm;
				frac = (tip_tgt > 0) ? fminf(s.ema_bpm / tip_tgt, 1.0f) : 0.0f;
				snprintf(bar_label, sizeof(bar_label), "S %.0f%%", frac * 100.0f);
			} else if (!state.badge_e) {
				tip_label = "Evenness";
				tip_tgt = alg_param_value("badge_evenness_thresh", 0.70f);
				tip_cur = s.ema_evenness;
				frac = (tip_tgt > 0) ? fminf(s.ema_evenness / tip_tgt, 1.0f) : 0.0f;
				snprintf(bar_label, sizeof(bar_label), "E %.0f%%", frac * 100.0f);
			}
		} else {
			if (!state.badge_pp) {
				tip_label = "Perf Power";
				tip_tgt = alg_param_value("perf_badge_power_thresh", 60);
				tip_cur = s.perf_power;
				frac = (tip_tgt > 0) ? fminf(s.perf_power / tip_tgt, 1.0f) : 0.0f;
				snprintf(bar_label, sizeof(bar_label), "P' %.0f%%", frac * 100.0f);
			} else if (!state.badge_ps) {
				tip_label = "Perf Speed";
				tip_tgt = alg_param_value("perf_badge_speed_thresh", 120);
				tip_cur = s.perf_ema_bpm;
				frac = (tip_tgt > 0) ? fminf(s.perf_ema_bpm / tip_tgt, 1.0f) : 0.0f;
				snprintf(bar_label, sizeof(bar_label), "S' %.0f%%", frac * 100.0f);
			} else if (!state.badge_pe) {
				tip_label = "Perf Evenness";
				tip_tgt = alg_param_value("perf_badge_evenness_thresh", 0.85f);
				tip_cur = s.perf_ema_evenness;
				frac = (tip_tgt > 0) ? fminf(s.perf_ema_evenness / tip_tgt, 1.0f) : 0.0f;
				snprintf(bar_label, sizeof(bar_label), "E' %.0f%%", frac * 100.0f);
			}
		}
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.8f, 1.0f));
		ImGui::ProgressBar(frac, ImVec2(bar_w, bar_h), bar_label);
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s: %.2f / %.2f", tip_label, tip_cur, tip_tgt);
		ImGui::PopStyleColor();
	}

}

static void show_info_line(void)
{
	const status_line &s = state.status;
	double age = glfwGetTime() - s.suggestion_time;
	bool show_msg = (s.suggestion[0] != '\0' && age < 3.0);

	// Performance mode prompt when no suggestion is active
	if (!show_msg && state.mode == state::MODE_PERFORMANCE && !state.karaoke_on) {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
				   "Space to start  |  BPM: %.0f", state.bpm);
		return;
	}

	if (!show_msg) {
		ImGui::Dummy(ImVec2(1.0f, ImGui::GetTextLineHeight()));
		return;
	}

	const char *text = expand_suggestion(s.suggestion);
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "%s", text);
}

static void show_celebration(void)
{
	const status_line &s = state.status;
	if (s.pts_delta <= 0)
		return;

	double age = glfwGetTime() - s.pts_delta_time;
	const double DURATION = 2.0;
	if (age >= DURATION)
		return;

	float t = (float)(age / DURATION); // 0 → 1 over lifetime
	float alpha = 1.0f - t; // fade out
	float rise = t * 40.0f; // drift upward 40px

	ImGuiIO &io = ImGui::GetIO();
	float cx = io.DisplaySize.x * 0.5f;
	float cy = io.DisplaySize.y * 0.4f - rise;

	char buf[32];
	snprintf(buf, sizeof(buf), "+%d pts!", s.pts_delta);

	// Draw with a large font scale via the foreground draw list so it
	// floats above all windows without affecting layout.
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	float font_size = ImGui::GetFontSize() * 3.0f;
	ImVec2 text_size = ImGui::CalcTextSize(buf);
	// CalcTextSize uses current font size; scale manually for centering.
	float scale = 3.0f;
	ImVec2 pos = ImVec2(cx - text_size.x * scale * 0.5f,
			    cy - text_size.y * scale * 0.5f);

	// Choose colour: gold normally, bright green when goal just met.
	ImU32 col;
	if (s.goal_met)
		col = IM_COL32(80, 255, 120, (int)(alpha * 255));
	else
		col = IM_COL32(255, 210, 40, (int)(alpha * 255));

	// Shadow for legibility.
	ImU32 shadow = IM_COL32(0, 0, 0, (int)(alpha * 180));
	dl->AddText(ImGui::GetFont(), font_size, ImVec2(pos.x + 2, pos.y + 2),
		    shadow, buf);
	dl->AddText(ImGui::GetFont(), font_size, pos, col, buf);
}

static void show_badge_celebration(void)
{
	if (state.badge_celebration[0] == '\0')
		return;

	double age = glfwGetTime() - state.badge_celebration_time;
	const double DURATION = 1.5;
	if (age >= DURATION)
		return;

	float t = (float)(age / DURATION);
	float alpha = 1.0f - t;
	float rise = t * 30.0f;

	ImGuiIO &io = ImGui::GetIO();
	float cx = io.DisplaySize.x * 0.5f;
	float cy = io.DisplaySize.y * 0.35f - rise;

	ImDrawList *dl = ImGui::GetForegroundDrawList();
	float font_size = ImGui::GetFontSize() * 2.5f;
	ImVec2 text_size = ImGui::CalcTextSize(state.badge_celebration);
	float scale = 2.5f;
	ImVec2 pos = ImVec2(cx - text_size.x * scale * 0.5f,
			    cy - text_size.y * scale * 0.5f);

	ImU32 col = IM_COL32(255, 220, 50, (int)(alpha * 255));
	ImU32 shadow = IM_COL32(0, 0, 0, (int)(alpha * 180));
	dl->AddText(ImGui::GetFont(), font_size, ImVec2(pos.x + 2, pos.y + 2),
		    shadow, state.badge_celebration);
	dl->AddText(ImGui::GetFont(), font_size, pos, col, state.badge_celebration);
}

static void show_perf_feedback(void)
{
	if (state.perf_feedback[0] == '\0')
		return;

	double age = glfwGetTime() - state.perf_feedback_time;
	const double DURATION = 3.0;
	if (age >= DURATION)
		return;

	float t = (float)(age / DURATION);
	float alpha = 1.0f - t;

	ImGuiIO &io = ImGui::GetIO();
	float cx = io.DisplaySize.x * 0.5f;
	float cy = io.DisplaySize.y * 0.45f;

	ImDrawList *dl = ImGui::GetForegroundDrawList();

	// Main PASS/FAIL text
	float font_size = ImGui::GetFontSize() * 3.0f;
	ImVec2 text_size = ImGui::CalcTextSize(state.perf_feedback);
	float scale = 3.0f;
	ImVec2 pos = ImVec2(cx - text_size.x * scale * 0.5f,
			    cy - text_size.y * scale * 0.5f);

	bool is_pass = (state.perf_feedback[0] == 'P');
	ImU32 col = is_pass ? IM_COL32(80, 255, 120, (int)(alpha * 255))
			    : IM_COL32(255, 80, 80, (int)(alpha * 255));
	ImU32 shadow = IM_COL32(0, 0, 0, (int)(alpha * 180));
	dl->AddText(ImGui::GetFont(), font_size, ImVec2(pos.x + 2, pos.y + 2),
		    shadow, state.perf_feedback);
	dl->AddText(ImGui::GetFont(), font_size, pos, col, state.perf_feedback);

}

static void show_levelup(void)
{
	if (state.levelup_text[0] == '\0')
		return;

	double age = glfwGetTime() - state.levelup_time;
	const double DURATION = 2.5;
	if (age >= DURATION)
		return;

	float t = (float)(age / DURATION);
	float alpha = (t < 0.2f) ? (t / 0.2f) : (1.0f - (t - 0.2f) / 0.8f);
	float rise = t * 50.0f;

	ImGuiIO &io = ImGui::GetIO();
	float cx = io.DisplaySize.x * 0.5f;
	float cy = io.DisplaySize.y * 0.35f - rise;

	ImDrawList *dl = ImGui::GetForegroundDrawList();
	float font_size = ImGui::GetFontSize() * 3.0f;
	ImVec2 text_size = ImGui::CalcTextSize(state.levelup_text);
	float scale = 3.0f;
	ImVec2 pos = ImVec2(cx - text_size.x * scale * 0.5f,
			    cy - text_size.y * scale * 0.5f);

	ImU32 col = IM_COL32(100, 255, 180, (int)(alpha * 255));
	ImU32 shadow = IM_COL32(0, 0, 0, (int)(alpha * 200));
	dl->AddText(ImGui::GetFont(), font_size, ImVec2(pos.x + 2, pos.y + 2),
		    shadow, state.levelup_text);
	dl->AddText(ImGui::GetFont(), font_size, pos, col, state.levelup_text);
}

// ── Top bar: stats left, chunk+[R]eload+X right ────────────────────────────

static void show_top_bar(void)
{
	ImGuiStyle &style = ImGui::GetStyle();
	float fp       = style.FramePadding.x;
	float sp       = style.ItemSpacing.x;
	float win_w    = ImGui::GetWindowWidth();
	float pad      = style.WindowPadding.x;

	// Stats bars (left)
	show_stats_bar();

	// Measure right-side items: chunk label + [R]eload + X
	char chunk_label[20] = "";
	float chunk_w = 0.0f;
	{
		const char *hash = nullptr;
		int lvl = -1;
		if (state.current_chunk[0]) {
			hash = state.current_chunk;
			lvl  = state.current_chunk_level;
		} else if (state.num_level0 > 0) {
			hash = state.level0_hashes[state.current_level0_idx];
			lvl  = 0;
		}
		if (hash) {
			if (lvl >= 0)
				snprintf(chunk_label, sizeof(chunk_label), "%d:%.8s", lvl, hash);
			else
				snprintf(chunk_label, sizeof(chunk_label), "%.8s", hash);
			chunk_w = ImGui::CalcTextSize(chunk_label).x + sp;
		}
	}
	float reload_w = ImGui::CalcTextSize("[R]eload").x + fp * 2.0f;
	float settings_w = ImGui::CalcTextSize("[S]ettings").x + fp * 2.0f;
	float x_w      = ImGui::CalcTextSize("X").x       + fp * 2.0f;
	float right_w  = chunk_w + reload_w + sp + settings_w + sp + x_w;
	float right_x  = win_w - pad - right_w;

	ImGui::SameLine();
	float after_x = ImGui::GetCursorPosX();
	ImGui::SetCursorPosX(right_x > after_x ? right_x : after_x);

	if (chunk_label[0]) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", chunk_label);
		ImGui::SameLine();
	}
	if (ImGui::Button("[R]eload"))
		reload_lesson();
	ImGui::SameLine();
	ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
	if (ImGui::Button("[S]ettings")) {
		state.settings_open = !state.settings_open;
		if (state.settings_open) {
			printf("MUTE\n");
			if (!state.alg_params_loaded)
				printf("QUERY_ALG\n");
		} else {
			printf("UNMUTE\n");
		}
		fflush(stdout);
	}
	ImGui::PopItemFlag();
	ImGui::SameLine();
	if (ImGui::Button("X"))
		quit_lesson();
}

// ── Compute the natural width of the bottom bar content ──────────────────────

static float bottom_bar_natural_width(void)
{
	ImGuiStyle &style = ImGui::GetStyle();
	float fp  = style.FramePadding.x;
	float sp  = style.ItemSpacing.x;

	float bpm_field_w  = ImGui::CalcTextSize("9999").x + fp * 2.0f;
	float dec_w        = ImGui::CalcTextSize("--").x + fp * 2.0f;
	float inc_w        = ImGui::CalcTextSize("++").x + fp * 2.0f;
	float kar_w        = ImGui::CalcTextSize("[K]araoke").x + fp * 2.0f;

	return dec_w + 2.0f + bpm_field_w + 2.0f + inc_w + sp + kar_w;
}

// ── Bottom bar: full window width, mirrors web-app ────────────────────────────

static void show_bottom_bar(float row_w, float row_x)
{
	float fp   = ImGui::GetStyle().FramePadding.x;
	float bpm_field_w  = ImGui::CalcTextSize("9999").x + fp * 2.0f;

	// BPM: -- [field] ++
	if (ImGui::Button("--")) {
		state.bpm = fmaxf(1.0f, state.bpm - 5.0f);
		printf("BPM %.2f\n", state.bpm);
		fflush(stdout);
	}
	ImGui::SameLine(0, 2.0f);
	int bpm_int = (int)(state.bpm + 0.5f);
	ImGui::SetNextItemWidth(bpm_field_w);
	if (ImGui::InputInt("##bpm", &bpm_int, 0, 0)) {
		bpm_int   = (bpm_int < 1) ? 1 : (bpm_int > 999) ? 999 : bpm_int;
		state.bpm = (float)bpm_int;
		printf("BPM %.2f\n", state.bpm);
		fflush(stdout);
	}
	ImGui::SameLine(0, 2.0f);
	if (ImGui::Button("++")) {
		state.bpm = fminf(999.0f, state.bpm + 5.0f);
		printf("BPM %.2f\n", state.bpm);
		fflush(stdout);
	}

	// Karaoke toggle
	ImGui::SameLine();
	bool kar_on = state.karaoke_on;
	if (kar_on)
		ImGui::PushStyleColor(ImGuiCol_Button,
				      ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
	if (ImGui::Button("[K]araoke"))
		toggle_karaoke();
	if (kar_on)
		ImGui::PopStyleColor();
}

// ── Settings screen ──────────────────────────────────────────────────────────

static void show_settings(void)
{
	ImGuiStyle &style = ImGui::GetStyle();
	float pad = style.WindowPadding.x;
	float disp_w = ImGui::GetIO().DisplaySize.x;

	if (ImGui::BeginTabBar("##settings_tabs")) {
		// ── MIDI Tab ─────────────────────────────────────────────────────
		if (ImGui::BeginTabItem("MIDI")) {
			state.settings_tab = 0;
			ImGui::Spacing();

			// Refresh button
			ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
			if (ImGui::Button("Refresh Devices")) {
				printf("MIDI DEVICES\n");
				fflush(stdout);
			}
			ImGui::PopItemFlag();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Input device
			float combo_w = fminf(300.0f, disp_w - 2.0f * pad - 120.0f);
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Input Device");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(combo_w);
			{
				const char *lbl = (state.midi_in >= 0 && state.midi_in < state.midi_count)
						      ? state.midi_names[state.midi_in] : "(none)";
				if (ImGui::BeginCombo("##midiin_s", lbl)) {
					for (int i = 0; i < state.midi_count; i++) {
						bool sel = (state.midi_in == i);
						if (ImGui::Selectable(state.midi_names[i], sel)) {
							state.midi_in = i;
							printf("MIDI IN %d\n", i);
							fflush(stdout);
						}
						if (sel) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("MIDI keyboard or controller to receive notes from");

			// Output device
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Output Device");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(combo_w);
			{
				const char *lbl = (state.midi_out >= 0 && state.midi_out < state.midi_count)
						      ? state.midi_names[state.midi_out] : "(none)";
				if (ImGui::BeginCombo("##midiout_s", lbl)) {
					for (int i = 0; i < state.midi_count; i++) {
						bool sel = (state.midi_out == i);
						if (ImGui::Selectable(state.midi_names[i], sel)) {
							state.midi_out = i;
							printf("MIDI OUT %d\n", i);
							fflush(stdout);
						}
						if (sel) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("MIDI output device for playback and synth");

			// Forward toggle
			ImGui::Spacing();
			bool fwd = state.midi_fwd;
			if (ImGui::Checkbox("Forward input to output", &fwd)) {
				state.midi_fwd = fwd;
				printf("MIDI FORWARD %s\n", fwd ? "ON" : "OFF");
				fflush(stdout);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Echo MIDI input directly to the output device (for monitoring)");

			// Synth volume
			ImGui::Spacing();
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Synth Volume");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::SliderFloat("##svol_s", &state.synth_vol, 0.0f, 1.0f, "%.2f")) {
				printf("SET MASTER_GAIN %.3f\n", state.synth_vol);
				fflush(stdout);
				save_gui_settings();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Volume of the built-in synthesizer (0 = muted)");

			// MIDI event log
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::Text("MIDI Events:");
			ImGui::BeginChild("##midi_log", ImVec2(-1, 200), true);
			for (int i = 0; i < state.midi_log_count; i++) {
				int idx = (state.midi_log_start + i) % 16;
				ImGui::TextUnformatted(state.midi_log[idx]);
			}
			if (state.midi_log_count > 0)
				ImGui::SetScrollHereY(1.0f);
			ImGui::EndChild();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Play notes on your MIDI device to see events here");

			ImGui::EndTabItem();
		}

		// ── Algorithm Tab ────────────────────────────────────────────────
		if (ImGui::BeginTabItem("Algorithm")) {
			state.settings_tab = 1;
			ImGui::Spacing();

			if (!state.alg_params_loaded) {
				ImGui::Text("Loading parameters...");
				printf("QUERY_ALG\n");
				fflush(stdout);
			} else {
				ImGui::BeginChild("##alg_scroll", ImVec2(-1, -1), false);
				float label_w = 200.0f;
				float input_w = 150.0f;
				for (int i = 0; i < state.num_alg_params; i++) {
					auto &p = state.alg_params[i];
					ImGui::PushID(i + 2000);
					ImGui::AlignTextToFramePadding();
					ImGui::Text("%s", p.key);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", p.tooltip);
					ImGui::SameLine(label_w);
					if (p.is_string) {
						ImGui::SetNextItemWidth(input_w * 2.0f);
						ImGui::InputText("##v", p.str_value, sizeof(p.str_value));
						if (ImGui::IsItemDeactivatedAfterEdit()) {
							printf("SET_ALG %s %s\n", p.key, p.str_value);
							fflush(stdout);
						}
					} else {
						ImGui::SetNextItemWidth(input_w);
						ImGui::InputFloat("##v", &p.value, 0, 0, "%.4g");
						if (ImGui::IsItemDeactivatedAfterEdit()) {
							printf("SET_ALG %s %.6g\n", p.key, p.value);
							fflush(stdout);
						}
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", p.tooltip);
					ImGui::PopID();
				}
				ImGui::EndChild();
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

static void gui_main(void)
{
	ImGuiIO &io  = ImGui::GetIO();
	float disp_w = io.DisplaySize.x;
	float disp_h = io.DisplaySize.y;

	// ── Top bar (full width) ─────────────────────────────────────────────────
	show_top_bar();

	// ── Settings screen replaces entire content area ─────────────────────────
	if (state.settings_open) {
		show_settings();
		check_new_day();
		return;
	}

	// ── Reserve bottom rows: info line + combined skill/BPM bar ─────────────
	float text_h  = ImGui::GetTextLineHeightWithSpacing();
	float frame_h = ImGui::GetFrameHeightWithSpacing();
	float spacing = ImGui::GetStyle().ItemSpacing.y;
	float bottom_reserved = text_h + spacing + frame_h;

	// ── Centered content child ───────────────────────────────────────────────
	float top_used  = ImGui::GetCursorPosY();
	float content_h = disp_h - top_used - bottom_reserved;
	float content_w = (state.image_width > 0) ? (float)state.image_width
						   : disp_w;
	float offset_x  = (content_w < disp_w)
			       ? floorf((disp_w - content_w) * 0.5f)
			       : 0.0f;
	if (offset_x > 0.0f)
		ImGui::SetCursorPosX(offset_x);

	ImGui::BeginChild(
	    "##content",
	    ImVec2(content_w, content_h > 4.0f ? content_h : 4.0f), false,
	    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	show_music();
	show_squares();
	if (state.explanation[0] != '\0')
		ImGui::TextUnformatted(state.explanation);
	ImGui::EndChild();

	float pad = ImGui::GetStyle().WindowPadding.x;

	// ── Info line: yellow suggestion (left), above the bottom row ────────────
	ImGui::SetCursorPosX(pad);
	show_info_line();
	ImGui::Spacing();

	// ── Combined bottom row: level indicator (left) + BPM/karaoke (right) ───
	{
		float bar_h = ImGui::GetFrameHeight();
		float sp = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX(pad);

		if (state.num_skills > 0 && state.current_level >= 0) {
			float thresh = state.status.mastery_thresh;
			int lvl = state.current_level;

			// Completed levels: green badge rectangles
			ImDrawList *dl = ImGui::GetWindowDrawList();
			for (int i = 0; i < lvl && i < state.num_skills; i++) {
				if (i > 0) ImGui::SameLine(0, 6.0f);
				ImGui::PushID(i + 3000);
				const char *label = state.skill_stats[i].name;
				ImVec2 tsz = ImGui::CalcTextSize(label);
				float pw = 4.0f; // padding
				float bw = tsz.x + pw * 2.0f;
				float bh = bar_h;
				ImVec2 pos = ImGui::GetCursorScreenPos();
				ImVec2 a = ImVec2(pos.x, pos.y);
				ImVec2 b = ImVec2(pos.x + bw, pos.y + bh);
				dl->AddRectFilled(a, b, IM_COL32(30, 160, 60, 255), 3.0f);
				dl->AddRect(a, b, IM_COL32(60, 220, 100, 255), 3.0f, 0, 1.5f);
				ImVec2 tpos = ImVec2(a.x + pw, a.y + (bh - tsz.y) * 0.5f);
				dl->AddText(tpos, IM_COL32(255, 255, 255, 255), label);
				ImGui::Dummy(ImVec2(bw, bh));
				ImGui::PopID();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s (mastered)",
							  skill_display_name(state.skill_stats[i].name));
			}

			// Current level: label + progress bar
			if (lvl < state.num_skills) {
				if (lvl > 0) ImGui::SameLine();
				float m = state.skill_stats[lvl].mastery;
				float frac = fminf(m / thresh, 1.0f);

				const char *label = state.skill_stats[lvl].name;

				// Leave room for BPM/karaoke on the right
				float right_w = bottom_bar_natural_width() + sp * 2.0f;
				float avail = disp_w - 2.0f * pad - right_w;
				float cur = ImGui::GetCursorPosX() - pad;
				float bar_w = avail - cur;
				if (bar_w < 60.0f) bar_w = 60.0f;
				if (bar_w > 200.0f) bar_w = 200.0f;

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
						      ImVec4(0.1f, 0.35f, 0.65f, 1.0f));
				ImGui::ProgressBar(frac, ImVec2(bar_w, bar_h), label);
				ImGui::PopStyleColor(2);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s: %.0f / %.0f%%",
							  skill_display_name(state.skill_stats[lvl].name),
							  m, thresh);
			} else if (state.stats_initialized) {
				// All skills mastered (only show after initial load complete)
				if (lvl > 0) ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f),
						   "All skills mastered!");
			}

			ImGui::SameLine();
		} else {
			ImGui::SetCursorPosX(pad);
		}

		// Daily goal bar (centered between skill bar and BPM)
		{
			const status_line &s = state.status;
			float right_total = bottom_bar_natural_width();
			float right_x = disp_w - pad - right_total;
			float left_x = ImGui::GetCursorPosX();
			float gap = right_x - left_x;
			float goal_w = 120.0f;
			if (goal_w > gap - sp * 2.0f) goal_w = gap - sp * 2.0f;
			if (goal_w < 50.0f) goal_w = 50.0f;
			float goal_x = left_x + (gap - goal_w) * 0.5f;

			ImGui::SameLine();
			ImGui::SetCursorPosX(goal_x);
			char d_label[24];
			if (s.goal_met && s.streak > 0)
				snprintf(d_label, sizeof(d_label), "%d pts  %dd", s.pts, s.streak);
			else
				snprintf(d_label, sizeof(d_label), "%d pts", s.pts);
			float d_frac = (s.goal > 0.0f) ? fminf((float)s.pts / s.goal, 1.0f) : 0.0f;
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
					      s.goal_met ? ImVec4(0.0f, 0.8f, 0.3f, 1.0f)
							 : ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
			ImGui::ProgressBar(d_frac, ImVec2(goal_w, bar_h), d_label);
			ImGui::PopStyleColor(2);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Daily score (goal: %.0f pts)%s",
						  s.goal, s.goal_met && s.streak > 0 ? "  Streak!" : "");
		}

		// Right-align BPM + karaoke
		{
			float right_total = bottom_bar_natural_width();
			float right_x = disp_w - pad - right_total;
			float cur_x = ImGui::GetCursorPosX();
			ImGui::SameLine();
			ImGui::SetCursorPosX(right_x > cur_x ? right_x : cur_x);
			show_bottom_bar(right_total, ImGui::GetCursorPosX());
		}
	}

	show_celebration();
	show_badge_celebration();
	show_perf_feedback();
	show_levelup();
	check_new_day();
}

int main(int, char **)
{
	glfwSetErrorCallback([](int error, const char *description) {
		fprintf(stderr, "GLFW Error %d: %s\n", error, description);
	});
	if (!glfwInit())
		return 1;

	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Create window with graphics context
	GLFWwindow *win =
	    glfwCreateWindow(800, 480, "Trainer", nullptr, nullptr);
	if (win == nullptr)
		return 1;

	// Center the window on the primary monitor
	{
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);
		int win_w, win_h;
		glfwGetWindowSize(win, &win_w, &win_h);
		glfwSetWindowPos(win, (mode->width - win_w) / 2,
				 (mode->height - win_h) / 2);
	}

	glfwMakeContextCurrent(win);
	glfwSetKeyCallback(win, key_callback);
	glfwSwapInterval(1); // Enable vsync

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = "log/imgui.log";

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();
	ImVec4 bg = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(win, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Non-blocking stdin
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

	// Restore saved settings and emit initial volume to synth
	load_gui_settings();
	printf("SET MASTER_GAIN %.3f\n", state.synth_vol);

	// Request MIDI device list so dropdowns are populated on startup
	printf("MIDI DEVICES\n");

	// Request algorithm params and first suggestion immediately; stats.lua
	// is ready at startup.
	printf("QUERY_ALG\n");
	printf("SUGGEST_LESSON\n");
	fflush(stdout);

	// Main loop
	while (!glfwWindowShouldClose(win) && state.running) {
		// Poll GLFW events
		glfwPollEvents();
		ImGui_ImplGlfw_Sleep(16);
		if (glfwGetWindowAttrib(win, GLFW_ICONIFIED) != 0)
			continue;

		// Check stdin for input
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		struct timeval tv = {0, 0}; // non-blocking
		int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
			char buf[MAX_LEN];
			while (fgets(buf, sizeof(buf), stdin))
				parse_line(buf);
			clearerr(stdin); // reset EOF/error from EAGAIN on non-blocking fd
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Place the app inside a single maximized window
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(io.DisplaySize);
		ImGui::Begin("##background", nullptr,
			     ImGuiWindowFlags_NoDecoration |
				 ImGuiWindowFlags_NoMove |
				 ImGuiWindowFlags_NoResize |
				 ImGuiWindowFlags_NoSavedSettings |
				 ImGuiWindowFlags_NoFocusOnAppearing |
				 ImGuiWindowFlags_NoBringToFrontOnFocus);
		gui_main();
		ImGui::End();

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(win, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(bg.x * bg.w, bg.y * bg.w, bg.z * bg.w, bg.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(win);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(win);
	glfwTerminate();
	return 0;
}
