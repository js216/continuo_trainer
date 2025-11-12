/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "imgui.h"
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

void clear_notes(struct app_state *state)
{
   state->note_count  = 0;
   state->chord_count = 0;
   set_status(state, "Cleared");
}

void select_note(struct app_state *state, int position, const char *name)
{
   state->note_count  = 1;
   state->notes[0]    = position;
   state->chord_count = 0;
   set_status(state, "Selected %s", name);
}

void add_chord(struct app_state *state, int position, const char *name)
{
   if (state->chord_count < 10) {
      state->chords[state->chord_count++] = position;
      set_status(state, "Added chord %s", name);
   }
}

static void draw_staff(ImDrawList *draw_list, ImVec2 pos, float width,
                       float spacing, ImU32 color)
{
   for (int i = 0; i < 5; i++) {
      float y = pos.y + spacing + (float)i * spacing;
      draw_list->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + width, y), color,
                         2.0F);
   }
}

static void draw_notes(ImDrawList *draw_list, ImVec2 pos, float spacing,
                       struct app_state *state, ImU32 main_color,
                       ImU32 chord_color)
{
   // Draw main note
   if (state->note_count > 0) {
      float note_y = pos.y + spacing + spacing * (float)state->notes[0] / 2.0F;
      float note_x = pos.x + 50;
      draw_list->AddCircleFilled(ImVec2(note_x, note_y), 10.0F, main_color);
   }

   // Draw chords (same horizontal as main note)
   for (int i = 0; i < state->chord_count; i++) {
      float note_y = pos.y + spacing + spacing * (float)state->chords[i] / 2.0F;
      float note_x = pos.x + 50;  // same x as main note
      draw_list->AddCircleFilled(ImVec2(note_x, note_y), 8.0F, chord_color);
   }
}

void init_state(struct app_state *state)
{
   snprintf(state->text, sizeof(state->text), "Type here");
   set_status(state, "Ready");
   state->note_count  = 0;
   state->chord_count = 0;

   select_note(state, 5, "C3");
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
   ImU32 main_color      = ImGui::GetColorU32(ImGuiCol_Text);
   ImU32 chord_color     = IM_COL32(255, 100, 100, 255);

   draw_staff(draw_list, p, width, spacing, main_color);
   draw_notes(draw_list, p, spacing, state, main_color, chord_color);

   ImGui::EndChild();

   // Bottom 20%: Controls + status
   ImGui::BeginChild("Controls", ImVec2(0, total_height * 0.2F), true);
   for (size_t i = 0; i < NOTE_COUNT; i++) {
      if (ImGui::Button(NOTES[i].name))
         add_chord(state, NOTES[i].position, NOTES[i].name);
      if (i < NOTE_COUNT - 1)
         ImGui::SameLine();
   }
   if (ImGui::Button("Clear"))
      clear_notes(state);

   // Status at the bottom of this child
   ImGui::Separator();
   ImGui::Text("Status: %s", state->status);

   ImGui::EndChild();

   ImGui::End();
}
