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
   if (state->config_file.empty())
      init_midi(state);

   set_style();
   set_font();
   dark_mode();

   state_status(state, "Ready");
}

static void app_buttons(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();
   const float bw    = (io.DisplaySize.x - 9 * STYLE_PAD_X) / 6.0F;
   ImVec2 b(bw, STYLE_BTN_H);

   if (ImGui::Button("Good", b)) {
      logic_good(state);
      state_status(state, "One good placed");
   }

   ImGui::SameLine();
   if (ImGui::Button("Bad", b)) {
      logic_bad(state);
      state_status(state, "One bad placed");
   }

   ImGui::SameLine();
   if (ImGui::Button("Populate", b)) {
      logic_populate(state);
      state_status(state, "All populated");
   }

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
   if (ImGui::Button("Connect", b)) {
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
   float staff_height    = avail_height * 0.6F;

   // Controls
   ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);
   app_buttons(state);
   app_midi(state, controls_height);
   ImGui::EndChild();

   // Staff
   ImGui::BeginChild("Staff", ImVec2(0, staff_height), false);
   notes_staff();
   notes_single(state->bassline, STYLE_WHITE);
   notes_chords(state->chords_ok, STYLE_GREEN);
   notes_chords(state->chords_bad, STYLE_RED);
   notes_figures(state->bassline, state->figures, STYLE_WHITE);
   ImGui::EndChild();

   // Status bar
   ImGui::BeginChild("StatusBar", ImVec2(0, status_height), true);
   ImGui::TextUnformatted(("Status: " + state->status).c_str());
   ImGui::EndChild();

   ImGui::End();
}
