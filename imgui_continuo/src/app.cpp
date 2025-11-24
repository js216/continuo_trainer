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

bool midi_popup_shown = false;

static void app_midi_popup(struct state *state)
{
   const float bw    = 100.0F;
   const ImGuiIO &io = ImGui::GetIO();

   // Fullscreen child (fills the main window)
   ImGui::BeginChild("MIDIFullScreen", io.DisplaySize, true);

   // MIDI Refresh
   if (ImGui::Button("MIDI Refresh", ImVec2(bw, 0))) {
      refresh_midi_devices(state);
      state->status    = "MIDI devices refreshed";
      midi_popup_shown = true; // open popup
   }

   // Connect / Disconnect MIDI
   const char *midi_btn_label = state->midi_in ? "Disconnect" : "Connect";
   ImGui::SameLine();
   if (ImGui::Button(midi_btn_label, ImVec2(bw, 0))) {
      if (state->midi_in)
         deinit_midi(state);
      else
         init_midi(state);
   }

   // Back button at top-left
   ImGui::SameLine();
   if (ImGui::Button("Back", ImVec2(bw, 0))) {
      midi_popup_shown = false;
   }

   ImGui::Separator();

   // MIDI device list
   float listbox_height = io.DisplaySize.y - STYLE_BTN_H;
   ImVec2 size(ImGui::GetContentRegionAvail().x, listbox_height);

   if (ImGui::BeginListBox("##midi_list", size)) {
      for (int i = 0; i < (int)state->midi_devices.size(); i++) {
         bool selected = (state->selected_device == i);
         if (ImGui::Selectable(state->midi_devices[i].c_str(), selected)) {
            state->selected_device = i;
            state->status = "Selected MIDI device: " + state->midi_devices[i];
         }
      }
      ImGui::EndListBox();
   }

   ImGui::EndChild();
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

static void app_buttons(struct state *state)
{
   float bw = ImGui::GetContentRegionAvail().x / 3;

   ImGui::PushItemWidth(bw);
   if (ImGui::InputInt("##lesson_id", &state->lesson_id)) {
      state->lesson_id = std::clamp(state->lesson_id, 1, 99999);
      logic_clear(state);
   }
   ImGui::PopItemWidth();

   ImGui::SameLine();
   ImGui::PushItemWidth(bw);
   app_key_sig_selector(state);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   ImGui::PushItemWidth(bw);
   ImGui::DragFloat("##tune", &global_tune, 0.05F, 1, 5);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   ImGui::PushItemWidth(bw);
   if (ImGui::Button("MIDI ...")) {
      midi_popup_shown = true;
   }
   ImGui::PopItemWidth();

   // next line
   bw = ImGui::GetContentRegionAvail().x / 4;

   ImGui::PushItemWidth(2 * bw);
   ImGui::InputText("##lesson_title", state->lesson_title, MAX_STRING);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   if (ImGui::Button("Reload", ImVec2(bw, 0))) {
      logic_clear(state);
   }

   ImGui::SameLine();
   const char *btn_label = state->edit_lesson ? "Save" : "Edit";
   if (ImGui::Button(btn_label, ImVec2(bw, 0))) {
      if (state->edit_lesson) {
         state_write_lesson(state);
         state->edit_lesson = false;
      } else {
         state->edit_lesson = true;
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

static void app_main_screen(struct state *state)
{
   ImGui::BeginChild("Controls", ImVec2(0, 2 * STYLE_BTN_H + 2 * STYLE_PAD_Y),
                     true);
   app_buttons(state);
   ImGui::EndChild();

   ImGui::BeginChild("Staff", ImVec2(0, 250.0F), true);
   notes_staff(state);
   notes_draw(state);
   ImGui::EndChild();

   ImGui::BeginChild("Stats", ImVec2(0, 150.0F), true);
   app_stats(state);
   ImGui::EndChild();

   ImGui::BeginChild("StatusBar", ImVec2(0, STYLE_BTN_H), true);
   ImGui::TextUnformatted(state->status.c_str());
   ImGui::EndChild();
}

void render_ui(struct state *state)
{
   ImGui::SetNextWindowPos(ImVec2(0, 0));
   const ImGuiIO &io = ImGui::GetIO();
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("MainWindow", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoNavFocus);

   if (midi_popup_shown)
      app_midi_popup(state);
   else
      app_main_screen(state);

   ImGui::End(); // End MainWindow
}
