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

static void add_note(struct app_state *state, int position, const char *name)
{
   if (state->note_count < 10) {
      state->notes[state->note_count++] = position;
      set_status(state, "Added %s", name);
   }
}

void clear_notes(struct app_state *state)
{
   state->note_count = 0;
   set_status(state, "Cleared");
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
                       struct app_state *state, ImU32 color)
{
   for (int i = 0; i < state->note_count; i++) {
      float note_y = pos.y + spacing + spacing * (float)state->notes[i] / 2.0F;
      float note_x = pos.x + 50 + (float)i * 60;
      draw_list->AddCircleFilled(ImVec2(note_x, note_y), 8.0F, color);
   }
}

void init_state(struct app_state *state)
{
   snprintf(state->text, sizeof(state->text), "Type here");
   set_status(state, "Ready");
   state->note_count = 0;
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
   ImGui::BeginChild("Controls", ImVec2(0, 0), true);
   for (size_t i = 0; i < NOTE_COUNT; i++) {
      if (ImGui::Button(NOTES[i].name))
         add_note(state, NOTES[i].position, NOTES[i].name);
      if (i < NOTE_COUNT - 1)
         ImGui::SameLine();
   }
   if (ImGui::Button("Clear"))
      clear_notes(state);
   ImGui::EndChild();

   ImGui::Separator();
   ImGui::Text("Status: %s", state->status);

   ImGui::End();
}
