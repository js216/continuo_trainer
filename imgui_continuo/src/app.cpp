// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "db.h"
#include "imgui.h"
#include "logic.h"
#include "midi.h"
#include "misc/cpp/imgui_stdlib.h"
#include "notes.h"
#include "state.h"
#include "style.h"
#include "theory.h"
#include "time_utils.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

void app_init(struct state *state)
{
   global_tune = 1;

   state->lesson_id = db_load_last_lesson_id();
   logic_clear(state);

   refresh_midi_devices(state);
   state_load_settings(state);

   init_midi_in(state);
   init_midi_out(state);

   set_style();
   set_font(state);
   dark_mode();

   state->status = "Ready";
}

static void draw_midi_top_row(struct state *state, const float bw)
{
   if (ImGui::Button("MIDI Refresh", ImVec2(bw, 0))) {
      refresh_midi_devices(state);
      state->status         = "MIDI devices refreshed";
      state->midi_menu_open = true;
   }

   ImGui::SameLine();
   ImGui::Checkbox("Forward In -> Out", &state->midi_forward);

   ImGui::SameLine(ImGui::GetContentRegionAvail().x - bw);
   if (ImGui::Button("Back", ImVec2(bw, 0))) {
      state->midi_menu_open = false;
   }
}

static void draw_midi_in_row(struct state *state, const float bw)
{
   const bool connected = (bool)state->midi_in;
   const char *label    = connected ? "Disconnect In" : "Connect In";

   if (ImGui::Button(label, ImVec2(bw, 0))) {
      if (connected) {
         deinit_midi_in(state);
      } else {
         state->in_dev = state->selected_device;
         init_midi_in(state);
      }
   }

   ImGui::SameLine();
   ImGui::TextUnformatted(state->in_dev.empty() ? "(No input device selected.)"
                                                : state->in_dev.c_str());
}

static void draw_midi_out_row(struct state *state, const float bw)
{
   const bool connected = (bool)state->midi_out;
   const char *label    = connected ? "Disconnect Out" : "Connect Out";

   if (ImGui::Button(label, ImVec2(bw, 0))) {
      if (connected) {
         deinit_midi_out(state);
      } else {
         state->out_dev = state->selected_device;
         init_midi_out(state);
         test_midi_out(state);
      }
   }

   ImGui::SameLine();
   ImGui::TextUnformatted(state->out_dev.empty()
                              ? "(No output device selected.)"
                              : state->out_dev.c_str());
}

static void draw_midi_device_list(struct state *state)
{
   const ImGuiIO &io    = ImGui::GetIO();
   float listbox_height = io.DisplaySize.y - 6 * STYLE_BTN_H;

   if (ImGui::BeginListBox(
           "##midi_list",
           ImVec2(ImGui::GetContentRegionAvail().x, listbox_height))) {

      for (auto &dev : state->midi_devices) {
         bool selected = (dev == state->selected_device);

         if (ImGui::Selectable(dev.c_str(), selected)) {
            state->selected_device = dev;
            state->status          = "Selected MIDI device: " + dev;
         }
      }

      ImGui::EndListBox();
   }
}

static void app_status_bar(struct state *state)
{
   ImGui::BeginChild("StatusBar", ImVec2(0, STYLE_BTN_H), true);
   ImGui::TextUnformatted(state->status.c_str());
   ImGui::EndChild();
}

static void app_midi_menu(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();
   ImGui::BeginChild("MIDIFullScreen", io.DisplaySize, true);

   const float bw = 150.0F;

   draw_midi_top_row(state, bw);
   ImGui::Separator();
   draw_midi_in_row(state, bw);
   draw_midi_out_row(state, bw);
   ImGui::Separator();
   draw_midi_device_list(state);

   app_status_bar(state);

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

static void app_figures_entry(state *state)
{
   if (ImGui::InputText("##figs_entry", &state->figs_entry)) {
      if (state->chords.empty())
         return;

      if (state->active_col - 1 >= state->chords.size())
         return;

      struct column &col = state->chords[state->active_col - 1];
      col.figures        = db_parse_figures_from_str(state->figs_entry);
   }
}

static void app_buttons(struct state *state)
{
   float bw = ImGui::GetContentRegionAvail().x / 6;

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
   app_figures_entry(state);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   if (ImGui::Button("x", ImVec2(bw, 0))) {
      state_pop_lesson(state);
   }

   ImGui::SameLine();
   ImGui::PushItemWidth(bw);
   ImGui::DragFloat("##tune", &global_tune, 0.05F, 1, 10);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   if (ImGui::Button("MIDI ...", ImVec2(bw, 0))) {
      state->midi_menu_open = true;
   }

   // next line
   bw = ImGui::GetContentRegionAvail().x / 4;

   ImGui::PushItemWidth(2 * bw);
   ImGui::InputText("##lesson_title", &state->lesson_title);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   const char *rel_label = state->edit_lesson ? "Discard" : "Reload";
   if (ImGui::Button(rel_label, ImVec2(bw, 0))) {
      logic_clear(state);
   }

   ImGui::SameLine();
   const char *btn_label = state->edit_lesson ? "Save" : "Edit";
   if (ImGui::Button(btn_label, ImVec2(bw, 0))) {
      if (state->edit_lesson) {
         state_store_lesson(state);
         state->edit_lesson = false;
      } else {
         state->edit_lesson = true;
      }
   }
}

static void stats_this_lesson(struct state *state)
{
   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted("THIS LESSON");

   // Lesson streak
   ImGui::Text("Streak: %d", state->lesson_streak);
}

static void stats_today(struct state *state)
{
   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted("TODAY");

   const float bar_h = 18.0F;

   // Score progress bar
   ImGui::TextUnformatted("Score");
   double max_score      = 1000.0F;
   std::string score_str = std::to_string(int(state->score));
   ImGui::ProgressBar((float)std::clamp(state->score / max_score, 0.0, 1.0),
                      ImVec2(-1, bar_h),
                      score_str.c_str() // numeric overlay
   );

   // Duration progress bar
   ImGui::TextUnformatted("Duration");
   std::string duration_str = time_format(state->duration_today);
   double max_duration      = 3600.0; // 1 hour for progress bar
   ImGui::ProgressBar(
       (float)std::clamp(state->duration_today / max_duration, 0.0, 1.0),
       ImVec2(-1, bar_h),
       duration_str.c_str() // overlay text
   );
}

static void stats_overall(struct state *state)
{
   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted("OVERALL");

   // Example placeholder for future streak stats
   ImGui::Text("Streak: %d", state->practice_streak);
}

static void app_stats(struct state *state)
{
   if (!ImGui::BeginTable("stats", 3, ImGuiTableFlags_SizingStretchSame))
      return;

   // Row 1: Titles and first-row content
   ImGui::TableNextRow();
   ImGui::TableSetColumnIndex(0);
   stats_this_lesson(state);

   ImGui::TableSetColumnIndex(1);
   stats_today(state);

   ImGui::TableSetColumnIndex(2);
   stats_overall(state);

   ImGui::EndTable();
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

   ImGui::BeginChild("Stats", ImVec2(0, 200.0F), true);
   app_stats(state);
   ImGui::EndChild();

   app_status_bar(state);
}

void app_render(struct state *state)
{
   ImGui::SetNextWindowPos(ImVec2(0, 0));
   const ImGuiIO &io = ImGui::GetIO();
   ImGui::SetNextWindowSize(io.DisplaySize);
   ImGui::Begin("MainWindow", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoNavFocus);

   if (state->midi_menu_open)
      app_midi_menu(state);
   else
      app_main_screen(state);

   ImGui::End(); // End MainWindow
}
