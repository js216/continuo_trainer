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
#include "time_utils.h"
#include "util.h"
#include <algorithm>
#include <cstdio>
#include <span>

void init_state(struct state *state)
{
   global_tune = 1;

   logic_clear(state);

   refresh_midi_devices(state);
   state_load(state);
   if (!state->config_file.empty())
      init_midi(state);

   set_style();
   set_font(state);
   dark_mode();

   state->status = "Ready";
}

static void app_buttons(struct state *state)
{
   const ImGuiIO &io  = ImGui::GetIO();
   const int num_btns = 3;
   const float bw     = (io.DisplaySize.x - 9 * STYLE_PAD_X) / num_btns;

   ImGui::PushItemWidth(bw);

   if (ImGui::Button("Reload")) {
      logic_clear(state);
   }

   ImGui::SameLine();
   if (ImGui::Button("MIDI Refresh")) {
      refresh_midi_devices(state);
      state->status = "MIDI devices refreshed";
   }

   ImGui::SameLine();
   if (state->midi_in) {
      if (ImGui::Button("Disconnect"))
         deinit_midi(state);
   } else {
      if (ImGui::Button("Connect"))
         init_midi(state);
   }

   ImGui::SameLine();
   ImGui::DragFloat("##tune", &global_tune, 1, 200, 400);

   ImGui::PopItemWidth();
}

static void app_midi(struct state *state, const float controls_height)
{
   float listbox_height = controls_height - 3 * STYLE_PAD_Y - STYLE_BTN_H;

   if (ImGui::BeginListBox("##midi_list", ImVec2(-FLT_MIN, listbox_height))) {
      for (int i = 0; i < (int)state->midi_devices.size(); i++) {
         bool selected = (state->selected_device == i);
         if (ImGui::Selectable(state->midi_devices[i].c_str(), selected)) {
            state->selected_device = i;
            state->status = "Selected MIDI device: " + state->midi_devices[i];
         }
      }
      ImGui::EndListBox();
   }
}

static void app_key_sig_selector(state *state)
{
   if (ImGui::BeginCombo("##keysig", key_sig_to_string(state->key).c_str())) {
      for (int i = 0; i < KEY_NUM; ++i) {
         bool is_selected = (state->key == i);
         if (ImGui::Selectable(
                 key_sig_to_string(static_cast<key_sig>(i)).c_str(),
                 is_selected))
            state->key = static_cast<key_sig>(i);
         if (is_selected)
            ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
   }
}

static void app_lesson(struct state *state)
{
   ImGui::PushItemWidth(150);
   if (ImGui::InputInt("##lesson_id", &state->lesson_id)) {
      state->lesson_id = std::clamp(state->lesson_id, 1, 99999);
      logic_clear(state);
   }
   ImGui::PopItemWidth();

   ImGui::SameLine();
   ImGui::PushItemWidth(250);
   ImGui::InputText("##lesson_title", state->lesson_title, MAX_STRING);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   ImGui::PushItemWidth(100);
   app_key_sig_selector(state);
   ImGui::PopItemWidth();

   if (state->edit_lesson) {
      ImGui::SameLine();
      if (ImGui::Button("Save")) {
         state_write_lesson(state);
         state->edit_lesson = false;
      }
   }
}

static void app_stats(struct state *state)
{
   ImGui::Text("Score: %.3f", state->score);

   // total practice duration today
   ImGui::TextUnformatted(
       ("Duration: " + time_format(state->duration_today)).c_str());
}

void render_ui(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();

   ImGui::SetNextWindowPos(ImVec2(0, 0));
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("Main", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

   const float status_height = STYLE_BTN_H + STYLE_PAD_Y;
   const float avail_height  = io.DisplaySize.y - 3 * STYLE_PAD_Y -
                              8 * STYLE_PAD_BORDER - status_height;
   float controls_height = avail_height * 0.4F;
   if (state->midi_in)
      controls_height = 2 * STYLE_BTN_H + 2 * STYLE_PAD_Y;
   const float staff_height = 244.0F;
   const float stats_h      = avail_height - controls_height - staff_height;

   // Controls
   ImGui::BeginChild("Controls", ImVec2(0, controls_height), true);
   app_buttons(state);
   if (!state->midi_in)
      app_midi(state, controls_height);
   app_lesson(state);
   ImGui::EndChild();

   // Staff
   ImGui::BeginChild("Staff", ImVec2(0, staff_height), false);
   notes_staff(state);
   notes_draw(state);
   ImGui::EndChild();

   // Stats
   ImGui::BeginChild("Stats", ImVec2(0, stats_h), false);
   app_stats(state);
   ImGui::EndChild();

   // Status bar
   ImGui::BeginChild("StatusBar", ImVec2(0, status_height), true);
   ImGui::TextUnformatted(state->status.c_str());
   ImGui::EndChild();

   ImGui::End();
}
