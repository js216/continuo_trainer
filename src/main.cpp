#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "debug.h"
#include "imgui.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>

struct app_state {
   char text[128];
   char status[128];
   int notes[10]; // Array of note positions
   int note_count;
};

static void safe_snprintf(char *buf, size_t size, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   int ret = vsnprintf(buf, size, fmt, args);
   va_end(args);

   if (ret < 0) {
      ERROR("snprintf encoding error");
   } else if ((size_t)ret >= size) {
      ERROR("snprintf truncated output");
   }
}

void init_state(struct app_state *state)
{
   safe_snprintf(state->text, sizeof(state->text), "Type here");
   safe_snprintf(state->status, sizeof(state->status), "Ready");
   state->note_count = 0;
}

void draw_staff(ImDrawList *draw_list, ImVec2 pos, float width, float spacing,
                ImU32 color)
{
   for (int i = 0; i < 5; i++) {
      float y = pos.y + spacing + (float)i * spacing;
      draw_list->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + width, y), color,
                         2.0F);
   }
}

void draw_notes(ImDrawList *draw_list, ImVec2 pos, float spacing,
                struct app_state *state, ImU32 color)
{
   for (int i = 0; i < state->note_count; i++) {
      float note_y =
          (float)(pos.y + spacing + spacing * (float)state->notes[i] / 2.0F);
      float note_x = pos.x + 50 + (float)i * 60;
      draw_list->AddCircleFilled(ImVec2(note_x, note_y), 8.0F, color);
   }
}

void add_note(struct app_state *state, int position, const char *name)
{
   if (state->note_count < 10) {
      state->notes[state->note_count++] = position;
      safe_snprintf(state->status, sizeof(state->status), "Added %s", name);
   }
}

void action_add_c(struct app_state *state)
{
   add_note(state, 6, "C");
}

void action_add_g(struct app_state *state)
{
   add_note(state, 2, "G");
}

void action_clear(struct app_state *state)
{
   state->note_count = 0;
   safe_snprintf(state->status, sizeof(state->status), "Cleared");
}

bool handle_events(struct app_state *state)
{
   SDL_Event event;
   if (SDL_WaitEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
         return false;
      if (event.type == SDL_KEYDOWN) {
         if (event.key.keysym.sym == SDLK_F1)
            action_add_c(state);
         else if (event.key.keysym.sym == SDLK_F2)
            action_add_g(state);
         else if (event.key.keysym.sym == SDLK_F3)
            action_clear(state);
      }
      while (SDL_PollEvent(&event)) {
         ImGui_ImplSDL2_ProcessEvent(&event);
         if (event.type == SDL_QUIT)
            return false;
      }
   }
   return true;
}

void render_ui(struct app_state *state)
{
   ImGui::SetNextWindowPos(ImVec2(0, 0));
   ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
   ImGui::Begin("Main", NULL,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

   const ImGuiIO &io  = ImGui::GetIO();
   float total_height = io.DisplaySize.y;
   float staff_height = total_height * 0.8F;

   // Top 80%: Staff area
   ImGui::BeginChild("Staff", ImVec2(0, staff_height), true);
   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();
   float width           = ImGui::GetContentRegionAvail().x;
   float spacing         = staff_height / 6.0F;
   ImU32 line_color      = ImGui::GetColorU32(ImGuiCol_Text);

   draw_staff(draw_list, p, width, spacing, line_color);
   draw_notes(draw_list, p, spacing, state, line_color);

   ImGui::EndChild();

   // Bottom 20%: Controls
   if (ImGui::Button("C"))
      action_add_c(state);
   ImGui::SameLine();
   if (ImGui::Button("G"))
      action_add_g(state);
   ImGui::SameLine();
   if (ImGui::Button("Clear"))
      action_clear(state);

   ImGui::Separator();
   ImGui::Text("Status: %s", state->status);

   ImGui::End();
}

int main(void)
{
   SDL_Init(SDL_INIT_VIDEO);
   SDL_Window *window = SDL_CreateWindow(
       "ImGui Example", SDL_WINDOWPOS_CENTERED_DISPLAY(0U),
       SDL_WINDOWPOS_CENTERED_DISPLAY(0U), 800, 480, SDL_WINDOW_OPENGL);
   SDL_GLContext gl_context = SDL_GL_CreateContext(window);

   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO &io    = ImGui::GetIO();
   io.IniFilename = nullptr;
   ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
   ImGui_ImplOpenGL3_Init("#version 130");

   struct app_state state = {};
   init_state(&state);

   while (handle_events(&state)) {
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      render_ui(&state);

      ImGui::Render();
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      SDL_GL_SwapWindow(window);
   }

   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplSDL2_Shutdown();
   ImGui::DestroyContext();
   SDL_GL_DeleteContext(gl_context);
   SDL_DestroyWindow(window);
   SDL_Quit();

   return 0;
}
