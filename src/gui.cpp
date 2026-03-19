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
//     SPACE       Emit SUGGEST_LESSON to get the best item to practice next.
//     K           Toggle karaoke mode (emits KARAOKE_ON / KARAOKE_OFF).
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
#define MAX_LEN 1024

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
	char suggestion[32] = "";
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
} state;

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
			suggest_lesson();
			break;
		case GLFW_KEY_K:
			toggle_karaoke();
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
	if (strncmp(buf, "KARAOKE_DONE", 12) == 0) {
		state.karaoke_on = false;
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
	if (strcmp(s, "no_lessons_available") == 0)
		return "No lessons found in seq/.";
	if (strcmp(s, "all_up_to_date") == 0)
		return "All caught up! Come back later.";
	if (strcmp(s, "overdue") == 0)
		return "Due for review.";
	if (strcmp(s, "needs_work") == 0)
		return "Needs more practice.";
	if (strcmp(s, "new_lesson") == 0)
		return "New lesson!";
	return s; // fallback: show the raw token
}

static void show_stats_bar(void)
{
	const status_line &s = state.status;

	float bar_w = 70.0f;
	float bar_h = ImGui::GetFrameHeight();

	// Mastery bar: green if at/above thresh, amber otherwise
	char m_label[16];
	snprintf(m_label, sizeof(m_label), "M %.1f", s.mastery);
	float m_frac = fminf(s.mastery / 100.0f, 1.0f);
	bool m_ok = s.mastery >= s.mastery_thresh;
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
			      m_ok ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f)
				   : ImVec4(0.8f, 0.5f, 0.1f, 1.0f));
	ImGui::ProgressBar(m_frac, ImVec2(bar_w, bar_h), m_label);
	ImGui::PopStyleColor();

	// Power bar: green if at/above thresh, amber otherwise
	char p_label[16];
	snprintf(p_label, sizeof(p_label), "P %.1f", s.power);
	float p_frac = fminf(s.power / 100.0f, 1.0f);
	bool p_ok = s.power >= s.power_thresh;
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
			      p_ok ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f)
				   : ImVec4(0.9f, 0.6f, 0.1f, 1.0f));
	ImGui::ProgressBar(p_frac, ImVec2(bar_w, bar_h), p_label);
	ImGui::PopStyleColor();

	// Daily goal: always show progress bar; streak to the right when met
	char d_label[16];
	snprintf(d_label, sizeof(d_label), "%d pts", s.pts);
	float d_frac =
	    (s.goal > 0.0f) ? fminf((float)s.pts / s.goal, 1.0f) : 0.0f;
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
			      s.goal_met ? ImVec4(0.0f, 0.8f, 0.3f, 1.0f)
					 : ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
	ImGui::ProgressBar(d_frac, ImVec2(bar_w, bar_h), d_label);
	ImGui::PopStyleColor();
	if (s.goal_met && s.streak > 0) {
		char streak_buf[32];
		snprintf(streak_buf, sizeof(streak_buf), "Streak: %d",
			 s.streak);
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "%s",
				   streak_buf);
	}
}

static void show_info_line(void)
{
	const status_line &s = state.status;
	double age = glfwGetTime() - s.suggestion_time;
	bool show_msg = (s.suggestion[0] != '\0' && age < 3.0);
	if (show_msg) {
		const char *text = expand_suggestion(s.suggestion);
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "%s",
				   text);
		ImGui::SameLine();
	}

	const char *hash;
	int lvl;
	if (state.current_chunk[0]) {
		hash = state.current_chunk;
		lvl = state.current_chunk_level;
	} else if (state.num_level0 > 0) {
		hash = state.level0_hashes[state.current_level0_idx];
		lvl = 0;
	} else {
		return;
	}

	char label[20];
	if (lvl >= 0)
		snprintf(label, sizeof(label), "%d:%.8s", lvl, hash);
	else
		snprintf(label, sizeof(label), "%.8s", hash);

	float text_w = ImGui::CalcTextSize(label).x;
	float margin = ImGui::GetStyle().WindowPadding.x;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - margin - text_w);
	ImGui::TextDisabled("%s", label);
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

static void gui_main(void)
{
	ImGuiIO &io = ImGui::GetIO();
	float display_w = io.DisplaySize.x;

	// Center content using the previous-frame image width as column width.
	float content_w =
	    (state.image_width > 0) ? (float)state.image_width : display_w;
	float offset_x = (content_w < display_w)
			      ? floorf((display_w - content_w) * 0.5f)
			      : 0.0f;
	if (offset_x > 0.0f)
		ImGui::SetCursorPosX(offset_x);

	ImGui::BeginChild("##content", ImVec2(content_w, 0), false,
			  ImGuiWindowFlags_NoScrollbar |
			      ImGuiWindowFlags_NoScrollWithMouse);

	// ── Row 1: stats bars (left) + buttons (right)
	// ───────────────────────────────────────────────
	show_stats_bar();

	// Right-align the buttons so X's right edge meets the image right edge,
	// falling back to just after the stats bar if the image is too narrow.
	ImGuiStyle &style = ImGui::GetStyle();
	float sp = style.ItemSpacing.x;
	float fp = style.FramePadding.x;
	float reload_w = ImGui::CalcTextSize("[R]eload").x + fp * 2;
	float karaoke_w = ImGui::CalcTextSize("[K]araoke").x + fp * 2;
	float x_w = ImGui::CalcTextSize("X").x + fp * 2;
	float total_btn_w = reload_w + sp + karaoke_w + sp + x_w;
	float image_right_x = ImGui::GetWindowContentRegionMin().x +
			      (float)state.image_width;
	float ideal_x = image_right_x - total_btn_w;
	ImGui::SameLine();
	float after_bars_x = ImGui::GetCursorPosX();
	ImGui::SetCursorPosX((ideal_x > after_bars_x) ? ideal_x : after_bars_x);

	if (ImGui::Button("[R]eload"))
		reload_lesson();

	ImGui::SameLine();
	bool karaoke_was_on = state.karaoke_on;
	if (karaoke_was_on)
		ImGui::PushStyleColor(ImGuiCol_Button,
				      ImVec4(0.0f, 0.7f, 0.2f, 1.0f));
	if (ImGui::Button("[K]araoke"))
		toggle_karaoke();
	if (karaoke_was_on)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button("X"))
		quit_lesson();

	// ── Content
	// ───────────────────────────────────────────────────────────
	show_music();
	show_squares();

	if (state.explanation[0] != '\0')
		ImGui::TextUnformatted(state.explanation);

	// ── Push info line to bottom
	// ──────────────────────────────────────────
	float line_h = ImGui::GetTextLineHeightWithSpacing();
	float pad_y = ImGui::GetStyle().WindowPadding.y;
	float win_bottom =
	    ImGui::GetWindowPos().y + ImGui::GetWindowSize().y - pad_y;
	float cursor_y = ImGui::GetCursorScreenPos().y;
	float remaining = win_bottom - cursor_y - line_h;
	if (remaining > 0.0f)
		ImGui::Dummy(ImVec2(0.0f, remaining));

	show_info_line();

	ImGui::EndChild();

	show_celebration();
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

	// Request the first suggestion immediately; stats.lua is ready at
	// startup.
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
