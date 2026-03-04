#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_LINES 128
#define MAX_LEN 1024

struct Square {
   bool ok;
};

struct status_line {
   bool  valid    = false;
   float acc      = 0.0f;
   float slowest  = 0.0f;
   int   pts      = 0;
   bool  goal_met = false;
   int   streak   = 0;
};

struct state {
   bool       running        = true;
   int        current_lesson = 1;
   struct Square     squares[MAX_LINES];
   int        num_squares    = 0;
   char       explanation[MAX_LINES * MAX_LEN];
   struct status_line status;
   bool       pending_clear  = false;
} state;

static void clear_status(void)
{
   state.num_squares  = 0;
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
   for (int i = 1; i <= n + 8; i++) {  // check a few past the end
      char path[64];
      snprintf(path, sizeof(path), "seq/%d.png", i);
      bool exists = (access(path, F_OK) == 0);
      bool expected = (i <= n);
      if (exists && !expected) {
         fprintf(stderr, "ERROR: gap in lesson files: seq/%d.png missing but seq/%d.png exists\n", n + 1, i);
         exit(1);
      }
   }
   return n;
}

static void next_lesson(void)
{
   int total = count_lessons();
   if (total < 1) return;
   state.current_lesson = (state.current_lesson % total) + 1;
   printf("LOAD_LESSON %d\n", state.current_lesson);
   clear_status();
}

static void prev_lesson(void)
{
   int total = count_lessons();
   if (total < 1) return;
   state.current_lesson = ((state.current_lesson - 2 + total) % total) + 1;
   printf("LOAD_LESSON %d\n", state.current_lesson);
   clear_status();
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
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
      }
   }
}



static void check_result(const char *buf)
{
   // RESULT <id> TIME:<t> <OK|FAIL> [error message]
   int id;
   if (sscanf(buf, "RESULT %d", &id) != 1)
      return;

   bool ok = (strstr(buf, " OK") != NULL || strstr(buf, "\tOK") != NULL ||
              strstr(buf, "mOK") != NULL);

   if (state.num_squares < MAX_LINES) {
      if (state.pending_clear) {
         state.num_squares    = 0;
         state.explanation[0] = '\0';
         state.pending_clear  = false;
      }
      state.squares[state.num_squares].ok = ok;
      state.num_squares++;
   }

   if (!ok) {
      // Append "id\terror message\n" to explanation
      const char *fail = strstr(buf, "FAIL");
      const char *msg = fail ? strchr(fail, ' ') : NULL;
      if (msg) {
         msg++; // skip space after FAIL
         size_t len = strlen(state.explanation);
         snprintf(state.explanation + len, sizeof(state.explanation) - len - 1,
                  "%d\t%s", id, msg);
         // msg already ends with \n from fgets
      }
   }

   if (state.num_squares == 1)
      state.status.slowest = 0;
}

static void check_score(const char *buf)
{
   // SCORE time=<t> accuracy=<n> slowest=<s.ss> ...
   if (strncmp(buf, "SCORE", 5) != 0)
      return;

   const char *p;
   p = strstr(buf, "accuracy=");
   if (p) sscanf(p, "accuracy=%f", &state.status.acc);

   p = strstr(buf, "slowest=");
   if (p) sscanf(p, "slowest=%f", &state.status.slowest);
}

static void check_stats(const char *buf)
{
   // STATS time=<t> total_today=<n.nn> goal=<n.nn> streak=<n> ...
   if (strncmp(buf, "STATS", 5) != 0)
      return;

   float total = 0.0f, goal = 0.0f;
   const char *p;

   p = strstr(buf, "total_today=");
   if (p) sscanf(p, "total_today=%f", &total);

   p = strstr(buf, "goal=");
   if (p) sscanf(p, "goal=%f", &goal);

   p = strstr(buf, "streak=");
   if (p) sscanf(p, "streak=%d", &state.status.streak);

   state.status.pts      = (int)(total + 0.5f);
   state.status.goal_met = (total >= goal && goal > 0.0f);

   // Trigger the next repetition of this lesson. We emit the command here,
   // immediately after parsing, without touching display state — squares,
   // explanation and status remain visible until the backend responds with
   // a LESSON line, which is when parse_lesson() clears them.
   printf("LOAD_LESSON %d\n", state.current_lesson);
   fflush(stdout);
   state.pending_clear = true;
}

static void parse_lesson(const char *buf)
{
   if (strncmp(buf, "LESSON", 6) != 0)
      return;

   clear_status();
}

static void parse_line(const char *buf)
{
   parse_lesson(buf);
   check_result(buf);
   check_score(buf);
   check_stats(buf);
}

static int LoadImage(const char* fname, GLuint* img, int* w, int* h)
{
   // Load from file to memory
   FILE* f = fopen(fname, "rb");
   if (f == NULL)
      return -1;
   fseek(f, 0, SEEK_END);
   long size = ftell(f);
   if (size < 0)
      return -1;
   fseek(f, 0, SEEK_SET);
   void* data = IM_ALLOC((size_t)size);
   fread(data, 1, (size_t)size, f);
   fclose(f);

   // Load from memory
   unsigned char* image_data = stbi_load_from_memory(
         (const unsigned char*)data, (int)size,
         w, h,
         NULL, 4);
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
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, image_data);
   stbi_image_free(image_data);

   IM_FREE(data);
   return 0;
}

static void show_music(void)
{
   static int iw = 0;
   static int ih = 0;
   static int img_disp = 0;
   static GLuint img = 0;
   if (!img || (img_disp != state.current_lesson)) {
      char filename[64];
      snprintf(filename, sizeof(filename), "seq/%d.png", state.current_lesson);
      if (LoadImage(filename, &img, &iw, &ih))
         return;
      img_disp = state.current_lesson;
   }

   ImGui::Image((ImTextureID)(intptr_t)img, ImVec2(iw, ih));
}

static void DrawSquare(ImDrawList* draw_list, ImVec2 pos, float sz, ImU32 col, int index, bool filled)
{
    // Increase width slightly to comfortably fit two digits (e.g., "99")
    float width = sz * 1.4f;
    float height = sz;

    ImVec2 a = ImVec2(pos.x + 1, pos.y + 1);
    ImVec2 b = ImVec2(pos.x + width - 1, pos.y + height - 1);

    if (filled) {
        draw_list->AddRectFilled(a, b, col, 2.0f); // Slight rounding for style
    } else {
        draw_list->AddRect(a, b, col, 2.0f, 0, 1.5f);
    }

    char buf[12];
    snprintf(buf, sizeof(buf), "%d", index);

    ImVec2 text_size = ImGui::CalcTextSize(buf);
    ImVec2 text_pos = ImVec2(
        a.x + ((b.x - a.x) - text_size.x) * 0.5f,
        a.y + ((b.y - a.y) - text_size.y) * 0.5f
    );

    // Black text for filled squares, themed color for unfilled
    ImU32 text_col = filled ? IM_COL32(0, 0, 0, 255) : col;
    draw_list->AddText(text_pos, text_col, buf);

    // Advance cursor by the custom width
    ImGui::Dummy(ImVec2(width, height));
}

static void show_squares(void)
{
   if (state.num_squares == 0)
      return;

   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   float sz = ImGui::GetFontSize() * 1.2f;
   float square_width = sz * 1.4f + 2.0f;

   float window_x    = ImGui::GetWindowPos().x;
   float window_w    = ImGui::GetWindowWidth();

   for (int i = 0; i < state.num_squares; i++) {
      // Wrap to next line if this square would overflow
      float cursor_x = ImGui::GetCursorScreenPos().x;
      if (cursor_x + square_width > window_x + window_w) {
         ImGui::NewLine();
      }

      ImU32 col = state.squares[i].ok
         ? IM_COL32(0, 220, 80, 255)    // green
         : IM_COL32(220, 50, 50, 255);  // red

      DrawSquare(draw_list, ImGui::GetCursorScreenPos(), sz, col, i, true);
      ImGui::SameLine(0, 2.0f);
   }
   ImGui::NewLine();
}


static void show_status_line(void)
{
   const status_line &s = state.status;

   ImGui::SameLine();
   if (s.goal_met)
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f),
                         "| %g%% %.1fs  pts=%d  streak=%d",
                         s.acc, s.slowest, s.pts, s.streak);
   else if (s.slowest != 0)
      ImGui::Text("%d | pts=%d | %g%% %.1fs",
               state.current_lesson, s.pts, s.acc, s.slowest);
   else
      ImGui::Text("%d | pts=%d",
               state.current_lesson, s.pts);
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

   show_status_line();
   show_music();
   show_squares();

   if (state.explanation[0] != '\0')
      ImGui::TextUnformatted(state.explanation);
}

int main(int, char**)
{
   glfwSetErrorCallback([](int error, const char* description) {
         fprintf(stderr, "GLFW Error %d: %s\n", error, description);
         });
   if (!glfwInit())
      return 1;

   // GL 3.0 + GLSL 130
   const char* glsl_version = "#version 130";
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

   // Create window with graphics context
   GLFWwindow* win = glfwCreateWindow(1280, 800, "Trainer", nullptr, nullptr);
   if (win == nullptr)
      return 1;
   glfwMakeContextCurrent(win);
   glfwSetKeyCallback(win, key_callback);
   glfwSwapInterval(1); // Enable vsync

   // Setup Dear ImGui context
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO& io = ImGui::GetIO(); (void)io;
   io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
   io.IniFilename = "log/imgui.log";

   // Setup Dear ImGui style
   ImGui::StyleColorsDark();
   //ImGui::StyleColorsLight();
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
