#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct state {
   int current_lesson = 0;
} state;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
   if (action == GLFW_PRESS) {
      if (key == GLFW_KEY_ESCAPE)
         glfwSetWindowShouldClose(window, GLFW_TRUE);
      printf("Key pressed: %d\n", key);
   } else if (action == GLFW_RELEASE) {
      printf("Key released: %d\n", key);
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

static void parse_line(const char *buf)
{
   check_load_lesson(buf);
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

static void gui_main(void)
{
   show_music();

   if (ImGui::Button("Save"))
      printf("Saved\n");
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
   //ImGui::StyleColorsDark();
   ImGui::StyleColorsLight();
   ImVec4 bg = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

   // Setup Platform/Renderer backends
   ImGui_ImplGlfw_InitForOpenGL(win, true);
   ImGui_ImplOpenGL3_Init(glsl_version);

   // Main loop
   while (!glfwWindowShouldClose(win)) {
      // Poll GLFW events
      glfwPollEvents();
      ImGui_ImplGlfw_Sleep(33);
      if (glfwGetWindowAttrib(win, GLFW_ICONIFIED) != 0)
         continue;

      // Check stdin for input
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(STDIN_FILENO, &fds);
      struct timeval tv = {0, 0}; // non-blocking
      int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
      if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
         char buf[1024];
         if (fgets(buf, sizeof(buf), stdin))
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
