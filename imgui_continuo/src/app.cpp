// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file app.cpp
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#include "app.h"
#include "calc.h"
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
#include <cmath>
#include <memory>
#include <string>
#include <vector>

void app_init(struct state *state)
{
   global_tune = 1;

   state_load_settings(state->settings);

   refresh_midi_devices(state);
   init_midi_in(state);
   init_midi_out(state);

   state_reload_stats(state);

   state->lesson.lesson_id = db_load_last_lesson_id();
   logic_clear(state);

   set_style();
   set_font(state);
   dark_mode();

   state->ui.status = "Ready";
}

static void draw_midi_top_row(struct state *state, const float bw)
{
   if (ImGui::Button("MIDI Refresh", ImVec2(bw, 0))) {
      refresh_midi_devices(state);
      state->ui.status = "MIDI devices refreshed";
   }

   ImGui::SameLine();
   ImGui::Checkbox("Forward In -> Out", &state->settings.midi_forward);

   ImGui::SameLine(ImGui::GetContentRegionAvail().x - bw);
}

static void draw_midi_in_row(struct state *state, const float bw)
{
   const bool connected = (bool)state->midi.midi_in;
   const char *label    = connected ? "Disconnect In" : "Connect In";

   if (ImGui::Button(label, ImVec2(bw, 0))) {
      if (connected) {
         deinit_midi_in(state);
      } else {
         state->settings.in_dev = state->ui.selected_device;
         init_midi_in(state);
      }
   }

   ImGui::SameLine();
   ImGui::TextUnformatted(state->settings.in_dev.empty()
                              ? "(No input device selected.)"
                              : state->settings.in_dev.c_str());
}

static void draw_midi_out_row(struct state *state, const float bw)
{
   const bool connected = (bool)state->midi.midi_out;
   const char *label    = connected ? "Disconnect Out" : "Connect Out";

   if (ImGui::Button(label, ImVec2(bw, 0))) {
      if (connected) {
         deinit_midi_out(state);
      } else {
         state->settings.out_dev = state->ui.selected_device;
         init_midi_out(state);
         test_midi_out(state);
      }
   }

   ImGui::SameLine();
   ImGui::TextUnformatted(state->settings.out_dev.empty()
                              ? "(No output device selected.)"
                              : state->settings.out_dev.c_str());
}

static void draw_midi_device_list(struct state *state)
{
   const ImGuiIO &io    = ImGui::GetIO();
   float listbox_height = io.DisplaySize.y - 6 * STYLE_BTN_H;

   if (ImGui::BeginListBox(
           "##midi_list",
           ImVec2(ImGui::GetContentRegionAvail().x, listbox_height))) {

      for (auto &dev : state->midi.midi_devices) {
         bool selected = (dev == state->ui.selected_device);

         if (ImGui::Selectable(dev.c_str(), selected)) {
            state->ui.selected_device = dev;
            state->ui.status          = "Selected MIDI device: " + dev;
         }
      }

      ImGui::EndListBox();
   }
}

static void app_status_bar(struct state *state)
{
   ImGui::BeginChild("StatusBar", ImVec2(0, STYLE_BTN_H), true);
   ImGui::TextUnformatted(state->ui.status.c_str());
   ImGui::EndChild();
}

static void app_midi_menu(struct state *state)
{
   const ImGuiIO &io = ImGui::GetIO();
   ImGui::BeginChild("MIDIFullScreen", io.DisplaySize, true);

   const float bw = 150.0F;

   draw_midi_top_row(state, bw);
   draw_midi_in_row(state, bw);
   draw_midi_out_row(state, bw);
   draw_midi_device_list(state);

   app_status_bar(state);

   ImGui::EndChild();
}

static void app_close_settings(struct state *state)
{
   ImGui::SameLine();

   // Compute width of button
   const char *label = "X";
   float bw          = 50.0F;
   const ImGuiIO &io = ImGui::GetIO();
   ImGui::SetCursorPosX(io.DisplaySize.x - bw - STYLE_PAD_X);

   if (ImGui::Button(label, ImVec2(bw, 0))) {
      state->ui.settings_open = false; // close settings
   }
}

static void app_settings(struct state *state)
{
   if (ImGui::BeginTabBar("SettingsTabBar")) {
      app_close_settings(state);

      if (ImGui::BeginTabItem("MIDI")) {
         app_midi_menu(state);
         ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Display")) {
         ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Audio")) {
         ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Algorithm")) {
         ImGui::SliderInt("Daily score goal", &state->settings.score_goal, 1000,
                          10000);
         ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
   }
}

static void app_key_sig_selector(state *state)
{
   if (ImGui::BeginCombo("##keysig",
                         th_key_sig_to_string(state->lesson.key).c_str())) {
      for (int i = 0; i < KEY_NUM; ++i) {
         bool is_selected = (state->lesson.key == i);
         if (ImGui::Selectable(
                 th_key_sig_to_string(static_cast<key_sig>(i)).c_str(),
                 is_selected))
            state->lesson.key = static_cast<key_sig>(i);
         if (is_selected)
            ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
   }
}

static void app_figures_entry(state *state)
{
   if (!state->ui.edit_lesson) {
      ImGui::InputText("##figs_entry", &state->ui.figs_entry,
                       ImGuiInputTextFlags_ReadOnly);
      return;
   }

   if (ImGui::InputText("##figs_entry", &state->ui.figs_entry)) {
      if (state->lesson.chords.empty())
         return;

      if (state->ui.active_col >= state->lesson.chords.size())
         return;

      struct column &col = state->lesson.chords[state->ui.active_col];
      col.figures        = th_parse_figures_from_str(state->ui.figs_entry);
   }
}

static bool color_button(const char *label, uint32_t color, float bw)
{
   ImGui::PushStyleColor(ImGuiCol_Button, color);
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

   bool pressed = ImGui::Button(label, ImVec2(bw, 0));

   ImGui::PopStyleColor(3);
   return pressed;
}

static void app_save_discard(struct state *state, const float bw)
{
   ImGui::SameLine();
   const char *rel_label = state->ui.edit_lesson ? "Discard" : "Reload";

   if (state->ui.edit_lesson && !state->lesson.chords.empty()) {
      if (color_button(rel_label, IM_COL32(200, 0, 0, 255), bw)) {
         logic_clear(state);
      }
   } else {
      if (ImGui::Button(rel_label, ImVec2(bw, 0)))
         logic_clear(state);
   }
}

static void app_buttons(struct state *state)
{
   float bw = ImGui::GetContentRegionAvail().x / 5 - 8.0F;

   ImGui::PushItemWidth(bw);
   if (ImGui::InputInt("##lesson_id", &state->lesson.lesson_id)) {
      state->lesson.lesson_id = std::clamp(state->lesson.lesson_id, 1, 99999);
      logic_clear(state);
   }
   ImGui::PopItemWidth();

   app_save_discard(state, bw);

   ImGui::SameLine();
   const char *btn_label = state->ui.edit_lesson ? "Save" : "Edit";
   if (ImGui::Button(btn_label, ImVec2(bw, 0))) {
      if (state->ui.edit_lesson) {
         state_store_lesson(state);
         state->ui.edit_lesson = false;
      } else {
         state->ui.edit_lesson = true;
      }
   }

   ImGui::SameLine();
   ImGui::PushItemWidth(bw);
   ImGui::DragFloat("##tune", &global_tune, 0.4F, 1, 100);
   ImGui::PopItemWidth();

   ImGui::SameLine();
   if (ImGui::Button("Settings", ImVec2(bw, 0))) {
      state->ui.settings_open = true;
   }

   // next line
   bw = ImGui::GetContentRegionAvail().x / 8 - 5.0F;

   // green "Next" button when next lesson is available
   bool next_pressed = false;
   const int next_lesson = state_choose_next(state);
   if (state->lesson.lesson_id == next_lesson)
      next_pressed = ImGui::Button("Next", ImVec2(bw, 0));
   else
      next_pressed = color_button("Next", IM_COL32(0, 200, 0, 255), bw);
   if (next_pressed) {
      state->lesson.lesson_id = state_choose_next(state);
      logic_clear(state);
   }

   ImGui::SameLine();
   ImGui::PushItemWidth(4 * bw);
   ImGui::InputText("##lesson_title", &state->lesson.lesson_title);
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
   if (ImGui::Button("X", ImVec2(bw, 0))) {
      state_pop_lesson(state);
   }
}

static void draw_streak_boxes(int streak)
{
   const ImVec2 box_size(12, 12);
   const float spacing = 4.0F;

   // Choose color based on streak
   ImU32 fill_col = 0;
   if (streak >= 5)
      fill_col = IM_COL32(51, 204, 51, 255); // green
   else if (streak == 4)
      fill_col = IM_COL32(180, 220, 60, 255); // lime
   else if (streak == 3)
      fill_col = IM_COL32(220, 180, 60, 255); // yellow
   else if (streak == 2)
      fill_col = IM_COL32(200, 140, 60, 255); // warm orange
   else if (streak == 1)
      fill_col = IM_COL32(180, 120, 60, 255); // dull orange-brown
   else
      fill_col = IM_COL32(255, 0, 0, 255); // fallback

   ImU32 empty_col = IM_COL32(128, 128, 128, 255);

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();

   for (int i = 0; i < FULL_STREAK; i++) {
      ImU32 col = (i < streak) ? fill_col : empty_col;
      draw_list->AddRectFilled(p, ImVec2(p.x + box_size.x, p.y + box_size.y),
                               col);
      p.x += box_size.x + spacing;
   }

   ImGui::Dummy(ImVec2(0, box_size.y + spacing));
}

static void draw_speedometer_arc(ImDrawList *draw_list, const ImVec2 &center,
                                 float radius, float thickness)
{
   draw_list->PathClear();
   draw_list->PathArcTo(center, radius, M_PI, M_PI + M_PI, 64);
   draw_list->PathStroke(IM_COL32(128, 128, 128, 100), false, thickness);

   float t0 = M_PI;
   float t1 = M_PI + ((0.25F / 0.5F) / 2.0F) * M_PI;
   draw_list->PathClear();
   draw_list->PathArcTo(center, radius, t0, t1, 32);
   draw_list->PathStroke(IM_COL32(200, 50, 50, 255), false, thickness);

   t0 = t1;
   t1 = M_PI + ((0.4F / 0.5F) / 2.0F) * M_PI;
   draw_list->PathClear();
   draw_list->PathArcTo(center, radius, t0, t1, 32);
   draw_list->PathStroke(IM_COL32(240, 200, 50, 255), false, thickness);

   t0 = t1;
   t1 = M_PI + ((0.6F / 0.5F) / 2.0F) * M_PI;
   draw_list->PathClear();
   draw_list->PathArcTo(center, radius, t0, t1, 32);
   draw_list->PathStroke(IM_COL32(51, 204, 51, 255), false, thickness);
}

static void draw_speedometer_needle(float speed, ImDrawList *draw_list,
                                    const ImVec2 &center, float radius)
{
   if (speed < 0.0F)
      speed = 0.0F;
   if (speed > 5.0F)
      speed = 5.0F;

   float t = 0.0F;
   if (speed <= 0.5F)
      t = speed / 0.5F;
   else
      t = 1.0F + logf((speed - 0.5F) + 1.0F) / logf(5.0F);
   if (t > 2.0F)
      t = 2.0F;

   float angle = M_PI + (t / 2.0F) * M_PI;

   ImVec2 needle = ImVec2(center.x + cosf(angle) * radius * 0.9F,
                          center.y + sinf(angle) * radius * 0.9F);
   draw_list->AddLine(center, needle, IM_COL32_WHITE, 3.0F);
}

static void draw_speedometer_labels(float speed, const ImVec2 &center,
                                    float radius)
{
   float text_height = ImGui::GetTextLineHeight();
   ImVec2 num_pos    = ImVec2(center.x, center.y + radius * 0.10F);
   ImGui::SetCursorScreenPos(ImVec2(num_pos.x - 10, num_pos.y));
   ImGui::Text("%.2F", speed);

   ImGui::SetCursorScreenPos(ImVec2(center.x - 20, num_pos.y + text_height));
   ImGui::TextUnformatted("Speed");
}

static void draw_speedometer(float speed)
{
   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   const ImVec2 pos      = ImGui::GetCursorScreenPos();
   const float radius    = 50.0F;
   const float thickness = 8.0F;
   ImVec2 center         = ImVec2(pos.x + radius, pos.y + radius);

   draw_speedometer_arc(draw_list, center, radius, thickness);
   draw_speedometer_needle(speed, draw_list, center, radius);
   draw_speedometer_labels(speed, center, radius);
}

static void stats_this_lesson(struct state *state)
{
   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted("THIS LESSON");
   float avail_w = ImGui::GetContentRegionAvail().x;

   // get lesson meta
   const struct lesson_meta m =
       db_lesson_exists(state->lesson.lesson_id)
           ? calc_get_lesson_meta(state->stats, state->lesson.lesson_id)
           : lesson_meta{};

   // streak
   ImVec2 streak_size =
       ImVec2(5 * 12 + 4 * 4, 12); // 5 boxes * 12px + 4px spacing
   float streak_offset = (avail_w - streak_size.x) * 0.5F;
   ImGui::SetCursorPosX(ImGui::GetCursorPosX() + streak_offset);
   draw_streak_boxes(m.streak);

   ImGui::Dummy(ImVec2(0.0F, 5.0F));

   // speedometer
   float gauge_width  = 100.0F; // match radius*2 from draw_speedometer
   float speed_offset = (avail_w - gauge_width) * 0.5F;
   ImGui::SetCursorPosX(ImGui::GetCursorPosX() + speed_offset);
   draw_speedometer(static_cast<float>(m.speed));

   // difficulty
   ImGui::TextUnformatted(("Ease: " + std::to_string(m.srs_ease)).c_str());
   ImGui::TextUnformatted(("Quality: " + std::to_string(m.quality)).c_str());
   ImGui::TextUnformatted(("Lives: " + std::to_string(m.lives_left)).c_str());
}

static void stats_today(struct state *state)
{
   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted("TODAY");

   const float bar_h = 18.0F;

   // Score progress bar
   ImGui::TextUnformatted("Score");
   double max_score      = state->settings.score_goal;
   std::string score_str = std::to_string(int(state->stats.score_today));
   ImGui::PushItemWidth(25.0F);
   ImGui::ProgressBar(
       (float)std::clamp(state->stats.score_today / max_score, 0.0, 1.0),
       ImVec2(-1, bar_h), score_str.c_str());
   ImGui::PopItemWidth();

   // Duration progress bar
   ImGui::TextUnformatted(
       ("Duration: " + time_format(state->stats.duration_today)).c_str());
}

static void stats_overall(struct state *state)
{
   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted("OVERALL");

   // Example placeholder for future streak stats
   ImGui::Text("Streak: %d", state->stats.practice_streak);
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

   ImGui::BeginChild("Stats", ImVec2(0, 300.0F), true);
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

   if (state->ui.settings_open)
      app_settings(state);
   else
      app_main_screen(state);

   ImGui::End(); // End MainWindow
}
