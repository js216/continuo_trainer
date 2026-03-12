#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <ctime>
#include <ctype.h>
#include <fcntl.h>
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
	int pts_delta = 0; // points earned on the last completed lesson
	double pts_delta_time =
	    -1e9; // initialised far in the past; never triggers on startup
	bool goal_met = false;
	int streak = 0;

	// coaching suggestion from last STATS line
	char suggestion[32] = "";
	double suggestion_time = -1e9;
};

struct state {
	bool running = true;
	int current_lesson = 1;
	char current_chunk[64] = ""; // non-empty while a chunk session is active
	struct Square squares[MAX_LINES];
	int num_squares = 0;
	int num_notes = 0;
	char explanation[MAX_LINES * MAX_LEN];
	struct status_line status;
	int image_width = 0;
	bool triggered_today = false;
	bool stats_initialized = false; // true after first STATS line;
					// suppresses startup celebration
} state;

static void clear_status(void)
{
	state.num_squares = 0;
	state.explanation[0] = '\0';
	state.status.slowest = 0;
}

static void quit_lesson(void)
{
	clear_status();
	state.running = false;
}

static void reload_lesson(void)
{
	state.current_chunk[0] = '\0';
	printf("LOAD_LESSON %d\n", state.current_lesson);
	clear_status();
}

static int count_lessons(void)
{
	// Count contiguous seq/1.png, seq/2.png, ... with no gaps.
	int n = 0;
	for (;;) {
		char path[64];
		snprintf(path, sizeof(path), "seq/%d.png", n + 1);
		if (access(path, F_OK) != 0)
			break;
		n++;
	}
	if (n == 0)
		return 0;

	// Check for gaps: any file beyond n that exists is a gap.
	for (int i = 1; i <= n + 8; i++) { // check a few past the end
		char path[64];
		snprintf(path, sizeof(path), "seq/%d.png", i);
		bool exists = (access(path, F_OK) == 0);
		bool expected = (i <= n);
		if (exists && !expected) {
			fprintf(stderr,
				"ERROR: gap in lesson files: seq/%d.png "
				"missing but seq/%d.png exists\n",
				n + 1, i);
			exit(1);
		}
	}
	return n;
}

static void next_lesson(void)
{
	int total = count_lessons();
	if (total < 1)
		return;
	state.current_lesson = (state.current_lesson % total) + 1;
	state.current_chunk[0] = '\0';
	printf("LOAD_LESSON %d\n", state.current_lesson);
	clear_status();
}

static void prev_lesson(void)
{
	int total = count_lessons();
	if (total < 1)
		return;
	state.current_lesson = ((state.current_lesson - 2 + total) % total) + 1;
	state.current_chunk[0] = '\0';
	printf("LOAD_LESSON %d\n", state.current_lesson);
	clear_status();
}

static void suggest_lesson(void)
{
	printf("SUGGEST_LESSON\n");
	fflush(stdout);
}

static void suggest_chunk(void)
{
	printf("SUGGEST_CHUNK\n");
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
		case GLFW_KEY_Q:
			quit_lesson();
			break;
		case GLFW_KEY_R:
			reload_lesson();
			break;
		case GLFW_KEY_P:
			prev_lesson();
			break;
		case GLFW_KEY_N:
			next_lesson();
			break;
		case GLFW_KEY_S:
			suggest_lesson();
			break;
		case GLFW_KEY_C:
			suggest_chunk();
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
		else
			printf("LOAD_LESSON %d\n", state.current_lesson);
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
	if (p)
		sscanf(p, "goal=%f", &goal);

	p = strstr(buf, "streak=");
	if (p)
		sscanf(p, "streak=%d", &state.status.streak);

	// mastery= and power= live inside the lesson bracket, e.g.
	// lesson=3[ivl=6,ease=2.60,tot_dur=12.345,mastery=18.50,power=9.20]
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
	// SUGGESTION chunk=<hash> lesson=<n> skills=<s> reason=<r>
	// SUGGESTION lesson=<id> reason=<r>
	// SUGGESTION none reason=<token>
	if (strncmp(buf, "SUGGESTION", 10) != 0)
		return;

	const char *cp = strstr(buf, "chunk=");
	if (cp) {
		char hash[64] = "";
		sscanf(cp, "chunk=%63s", hash);
		strncpy(state.current_chunk, hash, sizeof(state.current_chunk) - 1);
		state.current_chunk[sizeof(state.current_chunk) - 1] = '\0';
		printf("LOAD_CHUNK %s\n", hash);
		fflush(stdout);
		// Flash skills as the status message
		char skills[32] = "?";
		const char *sp = strstr(buf, "skills=");
		if (sp)
			sscanf(sp, "skills=%31s", skills);
		snprintf(state.status.suggestion, sizeof(state.status.suggestion),
			 "chunk: %s", skills);
		state.status.suggestion_time = glfwGetTime();
		return;
	}

	const char *p = strstr(buf, "lesson=");
	if (p) {
		int id = 0;
		if (sscanf(p, "lesson=%d", &id) == 1 && id > 0) {
			state.current_lesson = id;
			printf("LOAD_LESSON %d\n", id);
			fflush(stdout);
			clear_status();
		}
		// Flash the reason regardless of whether the lesson jump
		// succeeded.
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

static void parse_line(const char *buf)
{
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

	char desired[128];
	if (state.current_chunk[0])
		snprintf(desired, sizeof(desired), "chn/%s.png", state.current_chunk);
	else
		snprintf(desired, sizeof(desired), "seq/%d.png", state.current_lesson);

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
		return "Great effort! Try another lesson now.";
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

static void show_status_line(void)
{
	const status_line &s = state.status;

	// Lesson label: "L3" when no lesson data yet, "L3 12.3/5.4" when
	// available.
	char lesson_buf[48];
	if (s.mastery > 0.0f || s.power > 0.0f)
		snprintf(lesson_buf, sizeof(lesson_buf), "L%d %.1f/%.1f",
			 state.current_lesson, s.mastery, s.power);
	else
		snprintf(lesson_buf, sizeof(lesson_buf), "L%d",
			 state.current_lesson);

	// Points field.
	char pts_buf[48];
	snprintf(pts_buf, sizeof(pts_buf), "%d pts", s.pts);

	// Build the full status text.
	char text_buf[256];
	if (s.goal_met) {
		if (s.slowest != 0.0f)
			snprintf(text_buf, sizeof(text_buf),
				 "%s: %g%% %.1fs | %s | streak: %d", lesson_buf,
				 s.acc, s.slowest, pts_buf, s.streak);
		else
			snprintf(text_buf, sizeof(text_buf),
				 "%s | %s | streak: %d", lesson_buf, pts_buf,
				 s.streak);
	} else {
		if (s.slowest != 0.0f)
			snprintf(text_buf, sizeof(text_buf),
				 "%s: %g%% %.1fs | %s", lesson_buf, s.acc,
				 s.slowest, pts_buf);
		else
			snprintf(text_buf, sizeof(text_buf), "%s | %s",
				 lesson_buf, pts_buf);
	}

	float text_w = ImGui::CalcTextSize(text_buf).x;

	// If a suggestion arrived recently, override the status text for 1
	// second.
	const char *display_text = text_buf;
	char suggestion_buf[64] = "";
	double age = glfwGetTime() - s.suggestion_time;
	if (s.suggestion[0] != '\0' && age < 1.0) {
		strncpy(suggestion_buf, expand_suggestion(s.suggestion),
			sizeof(suggestion_buf) - 1);
		display_text = suggestion_buf;
		text_w = ImGui::CalcTextSize(display_text).x;
	}

	// Ideal position: right edge of text aligns with right edge of the
	// image. The image is rendered at the window's content origin (x =
	// window_x + padding), so its right edge is at that same x plus
	// state.image_width.
	float content_x =
	    ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
	float right_edge = content_x + (float)state.image_width;
	float ideal_x = right_edge - text_w;

	// Fallback position: just after the buttons (current cursor after
	// SameLine).
	ImGui::SameLine();
	float after_buttons_x = ImGui::GetCursorScreenPos().x;

	// Use whichever is further right (i.e. don't collide into buttons).
	float text_x = (ideal_x > after_buttons_x) ? ideal_x : after_buttons_x;

	ImGui::SetCursorScreenPos(
	    ImVec2(text_x, ImGui::GetCursorScreenPos().y));

	if (s.suggestion[0] != '\0' && age < 1.0)
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "%s",
				   display_text);
	else if (s.goal_met)
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "%s",
				   display_text);
	else
		ImGui::TextUnformatted(display_text);
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
	ImGui::SameLine();
	if (ImGui::Button("[Q]uit"))
		quit_lesson();

	ImGui::SameLine();
	if (ImGui::Button("[R]eload"))
		reload_lesson();

	ImGui::SameLine();
	if (ImGui::Button("[P]rev"))
		prev_lesson();

	ImGui::SameLine();
	if (ImGui::Button("[N]ext"))
		next_lesson();

	ImGui::SameLine();
	if (ImGui::Button("[S]uggest"))
		suggest_lesson();

	ImGui::SameLine();
	if (ImGui::Button("[C]hunk"))
		suggest_chunk();

	show_status_line();
	show_music();
	show_squares();

	if (state.explanation[0] != '\0')
		ImGui::TextUnformatted(state.explanation);

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
	    glfwCreateWindow(1280, 800, "Trainer", nullptr, nullptr);
	if (win == nullptr)
		return 1;
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

	reload_lesson();

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
