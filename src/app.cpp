/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "util.h"
#include <stdio.h>

void init_state(struct app_state *state)
{
   snprintf(state->text, sizeof(state->text), "Type here");
   snprintf(state->status, sizeof(state->status), "Ready");
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
      float note_y = pos.y + spacing + spacing * (float)state->notes[i] / 2.0F;
      float note_x = pos.x + 50 + (float)i * 60;
      draw_list->AddCircleFilled(ImVec2(note_x, note_y), 8.0F, color);
   }
}

void add_note(struct app_state *state, int position, const char *name)
{
   if (state->note_count < 10) {
      state->notes[state->note_count++] = position;
      snprintf(state->status, sizeof(state->status), "Added %s", name);
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
   snprintf(state->status, sizeof(state->status), "Cleared");
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
