// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "imgui.h"
#include "logic.h"
#include "midi.h"
#include "notes.h"
#include "state.h"
#include "style.h"
#include "util.h"
#include <cstdio>
#include <span>

void init_state(struct state *state)
{
   logic_clear(state);

   refresh_midi_devices(state);
   state_load(state);
   if (!state->config_file.empty())
      init_midi(state);

   set_style();
   set_font(state);
   dark_mode();

   state_status(state, "Ready");
}

static void app_buttons(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();
   const int num_btns = 5;
   const float bw    = (io.DisplaySize.x - 9 * STYLE_PAD_X) / num_btns;
   ImVec2 b(bw, STYLE_BTN_H);

   ImGui::SameLine();
   if (ImGui::Button("Clear", b)) {
      logic_clear(state);
      state_status(state, "Cleared");
   }

   ImGui::SameLine();
   if (ImGui::Button("MIDI Refresh", b)) {
      refresh_midi_devices(state);
      state_status(state, "MIDI devices refreshed");
   }

   ImGui::SameLine();
   if (state->midi_in) {
      if (ImGui::Button("Disconnect", b))
         deinit_midi(state);
   } else {
      if (ImGui::Button("Connect", b))
         init_midi(state);
   }
}

static void app_midi(struct state *state, const float controls_height)
{
   float listbox_height = controls_height - 3 * STYLE_PAD_Y - STYLE_BTN_H;

   if (ImGui::BeginListBox("##midi_list", ImVec2(-FLT_MIN, listbox_height))) {
      for (int i = 0; i < (int)state->midi_devices.size(); i++) {
         bool selected = (state->selected_device == i);
         if (ImGui::Selectable(state->midi_devices[i].c_str(), selected)) {
            state->selected_device = i;
            state_status(state, "Selected MIDI device: ",
                         state->midi_devices[i].c_str());
         }
      }
      ImGui::EndListBox();
   }
}

static void draw_status_bar(const struct state *state, float height)
{
   ImGui::BeginChild("StatusBar", ImVec2(0, height), true);

   // Left-aligned status text
   ImGui::TextUnformatted(state->status.c_str());

   // Right-aligned MIDI device name
   if (state->midi_in && state->selected_device >= 0 &&
       state->selected_device < (int)state->midi_devices.size()) {
      const char *dev_name =
          state->midi_devices[state->selected_device].c_str();
      float avail_width = ImGui::GetContentRegionAvail().x;
      float text_width  = ImGui::CalcTextSize(dev_name).x;

      // Move cursor to right edge minus text width
      ImGui::SameLine(avail_width - text_width);
      ImGui::TextUnformatted(dev_name);
   }

   ImGui::EndChild();
}

void render_ui(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();

   ImGui::SetNextWindowPos(ImVec2(0, 0));
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("Main", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

   float status_height = STYLE_BTN_H + STYLE_PAD_Y;
   float avail_height  = io.DisplaySize.y - 3 * STYLE_PAD_Y -
                        8 * STYLE_PAD_BORDER - status_height;
   float controls_height = avail_height * 0.4F;
   if (state->midi_in)
      controls_height = STYLE_BTN_H + 2 * STYLE_PAD_Y;
   float staff_height = avail_height - controls_height;

   // Controls
   ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);
   app_buttons(state);
   if (!state->midi_in)
      app_midi(state, controls_height);
   ImGui::EndChild();

   // Staff
   ImGui::BeginChild("Staff", ImVec2(0, staff_height), false);
   notes_staff(state);
   notes_draw(state);
   ImGui::EndChild();

   // Status bar
   draw_status_bar(state, status_height);

   ImGui::End();
}
