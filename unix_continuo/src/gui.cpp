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

#define MAX_LEN 1024
#define MAX_LINES 1024

struct state {
   bool running = true;
   int current_lesson = 1;
   char msg[MAX_LEN];
   char explanation[MAX_LEN];
} state;

static void clear_status(void)
{
   state.msg[0] = '\0';
   state.explanation[0] = '\0';
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

static void next_lesson(void)
{
   state.current_lesson++;
   printf("LOAD_LESSON %d\n", state.current_lesson);
   clear_status();
}

static void prev_lesson(void)
{
   state.current_lesson--;
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

static void check_load_lesson(const char *buf)
{
   // Check for prefix
   const char *prefix = "LOAD_LESSON ";
   size_t prefix_len = strlen(prefix);

   if (strncmp(buf, prefix, prefix_len) != 0)
      return; // Not found

   // Parse the integer after the prefix
   const char *num_str = buf + prefix_len;
   char *endptr = NULL;
   long n = strtol(num_str, &endptr, 10);

   // Validate: must have consumed at least one digit
   if (endptr == num_str)
      return; // no number found

   // Assign to state
   state.current_lesson = (int)n;
}

static void check_msg(const char *buf)
{
   const char *found = strstr(buf, "MSG ");
   if (found) {
      const char *src = found + 4;
      strncpy(state.msg, src, MAX_LEN - 1);
      state.msg[MAX_LEN - 1] = '\0';

      // Strip trailing newline to prevent layout issues
      size_t len = strlen(state.msg);
      if (len > 0 && state.msg[len - 1] == '\n') {
         state.msg[len - 1] = '\0';
      }
   }
}

static void check_result(const char *buf)
{
   if (!strstr(buf, "FAIL"))
      return;

   int id;
   if (sscanf(buf, "RESULT %d", &id) != 1)
      return;

   const char *p = strstr(buf, "FAIL");
   if (!p)
      return;

   const char *msg = strchr(p, ' ');
   if (!msg)
      return;

   msg++;   /* skip space after FAIL */

   size_t len = strlen(state.explanation);
   size_t remaining = sizeof(state.explanation) - len - 1;
   if (!remaining)
      return;

   snprintf(state.explanation + len, remaining,
         "%d\t%s\n", id, msg);
}

static void parse_line(const char *buf)
{
   check_load_lesson(buf);
   check_msg(buf);
   check_result(buf);
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
      clear_status();
   }

   ImGui::Image((ImTextureID)(intptr_t)img, ImVec2(iw, ih));
}

static void TextAnsi(const char* text)
{
   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Text];
   const char* p = text;

   while (*p != '\0') {
      if (*p == '\x1B') { // Handle ANSI Colors
         p++;
         if (*p == '[') {
            p++;
            int code = 0;
            while (isdigit(*p)) { code = code * 10 + (*p - '0'); p++; }
            if (code == 32)      color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            else if (code == 31) color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            else if (code == 0)  color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            while (*p != '\0' && !isalpha(*p)) p++;
            if (*p != '\0') p++;
         }
      }
      // Detect UTF-8 ■ (\xE2\x96\xA0) or □ (\xE2\x96\xA1)
      else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x96 &&
            ((unsigned char)p[2] == 0xA0 || (unsigned char)p[2] == 0xA1)) {

         bool filled = ((unsigned char)p[2] == 0xA0);
         ImVec2 pos = ImGui::GetCursorScreenPos();
         float sz = ImGui::GetFontSize();
         ImU32 col = ImGui::ColorConvertFloat4ToU32(color);

         ImVec2 a = ImVec2(pos.x + 2, pos.y + 2);
         ImVec2 b = ImVec2(pos.x + sz - 2, pos.y + sz - 2);

         if (filled) draw_list->AddRectFilled(a, b, col);
         else        draw_list->AddRect(a, b, col, 0.0f, 0, 1.5f); // 1.5f thickness

         ImGui::Dummy(ImVec2(sz, sz));
         ImGui::SameLine(0, 0);
         p += 3; // Advance past 3-byte UTF-8 sequence
      }
      else {
         // Print everything else until the next ESC or UTF-8 box start
         const char* next = p + 1;
         while (*next != '\0' && *next != '\x1B' && (unsigned char)*next != 0xE2) next++;

         int len = (int)(next - p);
         ImGui::TextColored(color, "%.*s", len, p);
         if (*next != '\0' && *next != '\n') ImGui::SameLine(0, 0);
         p = next;
      }
   }
   ImGui::NewLine();
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

   show_music();

   TextAnsi(state.msg);
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
