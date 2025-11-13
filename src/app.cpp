/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "imgui.h"
#include "notes.h"
#include "style.h"
#include "util.h"
#include <stdarg.h>
#include <stdio.h>

static void set_status(struct app_state *state, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   vsnprintf(state->status, sizeof(state->status), fmt, args);
   va_end(args);
}

void init_state(struct app_state *state)
{
   for (int i = 0; i < APP_MAX_NOTES; i++)
      state->bassline[i] = (enum midi_note)(NOTES_E2 + i);

   set_style();
   dark_mode();
   set_status(state, "Ready");
}

static void app_buttons(struct app_state *state)
{
   float controls_height = ImGui::GetFrameHeightWithSpacing() * 3.0F;
   ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);
   if (ImGui::Button("Clear"))
      set_status(state, "Cleared");
   ImGui::Separator();
   ImGui::Text("Status: %s", state->status);
   ImGui::EndChild();
}

void render_ui(struct app_state *state)
{
   const ImGuiIO &io = ImGui::GetIO();

   ImGui::SetNextWindowPos(ImVec2(0, 0));
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("Main", NULL,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

   app_buttons(state);
   notes_staff();
   notes_dots(state->bassline, APP_MAX_NOTES);

   ImGui::End();
}
