/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "imgui.h"
#include "logic.h"
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
   set_style();
   set_font("fonts/Roboto-Regular.ttf", 18.0F);
   dark_mode();
   set_status(state, "Ready");
}

static void app_controls(struct app_state *state)
{
   float controls_height = ImGui::GetFrameHeightWithSpacing() * 3.0F;
   ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);

   if (ImGui::Button("One")) {
      logic_one(state->chords);
      set_status(state, "One placed");
   }

   ImGui::SameLine();
   if (ImGui::Button("Populate")) {
      logic_pop(state->bassline, MAX_CHORDS);
      set_status(state, "All populated");
   }

   ImGui::SameLine();
   if (ImGui::Button("Clear")) {
      logic_clear(state->bassline, MAX_CHORDS);
      set_status(state, "Cleared");
   }

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

   app_controls(state);
   notes_staff();
   notes_dots(state->bassline, MAX_CHORDS, STYLE_WHITE);
   notes_chords(state->chords, STYLE_GREEN);

   ImGui::End();
}
