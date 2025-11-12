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
   set_style();
   dark_mode();
   set_status(state, "Ready");
}

static void render_staff(void)
{
   ImVec2 avail = ImGui::GetContentRegionAvail();

   ImGui::BeginChild("Staff", avail, true);

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();
   ImVec2 size           = ImGui::GetContentRegionAvail();

   const float top     = p.y + size.y * 0.2F;
   const float bottom  = p.y + size.y * 0.8F;
   const float spacing = (bottom - top) / 4.0F; // 5 lines → 4 intervals

   for (int i = 0; i < 5; i++) {
      float y = top + (float)i * spacing;
      draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + size.x, y), STYLE_WHITE,
                         1.0F);
   }

   ImGui::EndChild();
}

void render_ui(struct app_state *state)
{
   const ImGuiIO &io = ImGui::GetIO();

   ImGui::SetNextWindowPos(ImVec2(0, 0));
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("Main", NULL,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

   // Bottom: Controls (fixed height)
   float controls_height = ImGui::GetFrameHeightWithSpacing() * 3.0F;
   ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);
   if (ImGui::Button("Clear"))
      set_status(state, "Cleared");
   ImGui::Separator();
   ImGui::Text("Status: %s", state->status);
   ImGui::EndChild();

   render_staff();

   ImGui::End();
}
