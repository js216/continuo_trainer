// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file notes.cpp
 * @brief Note manupulation routines.
 * @author Jakob Kastelic
 */

#include "notes.h"
#include "imgui.h"
#include "state.h"
#include "style.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

#define NOTES_OUT_OF_RANGE (-100)

static const char *acc_sym(enum accidental a)
{
   switch (a) {
      case ACC_SHARP: return "\uE262";   // SMuFL sharp
      case ACC_FLAT: return "\uE260";    // SMuFL flat
      case ACC_NATURAL: return "\uE261"; // SMuFL natural
      default: return "";
   }
}

static enum accidental key_sig_accidental(enum key_sig key, midi_note n)
{
   static const std::array<std::array<int, 12>, KEY_NUM> table = {
      {
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
         {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
         {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
         {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
         {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0},
         {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0},
         {0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0},
         {1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1},
         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
         {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0},
         {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0},
         {0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0},
         {0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0},
         {1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0},
         {1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1},
      }
   };
   return table[key][n % 12] ? ACC_NATURAL : ACC_NONE;
}

static int note_to_bass(const enum midi_note n)
{
   switch (n) {
      case NOTES_D2:
      case NOTES_Ds2: return -3;
      case NOTES_E2: return -2;
      case NOTES_F2:
      case NOTES_Fs2: return -1;
      case NOTES_G2:
      case NOTES_Gs2: return 0;
      case NOTES_A2:
      case NOTES_As2: return 1;
      case NOTES_B2: return 2;
      case NOTES_C3:
      case NOTES_Cs3: return 3;
      case NOTES_D3:
      case NOTES_Ds3: return 4;
      case NOTES_E3: return 5;
      case NOTES_F3:
      case NOTES_Fs3: return 6;
      case NOTES_G3:
      case NOTES_Gs3: return 7;
      case NOTES_A3:
      case NOTES_As3: return 8;
      case NOTES_B3: return 9;
      case NOTES_C4:
      case NOTES_Cs4: return 10;
      case NOTES_D4:
      case NOTES_Ds4: return 11;
      default: return NOTES_OUT_OF_RANGE;
   }
}

static float note_to_treble(const enum midi_note n)
{
   switch (n) {
      case NOTES_E4: return 0;
      case NOTES_F4:
      case NOTES_Fs4: return 1;
      case NOTES_G4:
      case NOTES_Gs4: return 2;
      case NOTES_A4:
      case NOTES_As4: return 3;
      case NOTES_B4: return 4;
      case NOTES_C5:
      case NOTES_Cs5: return 5;
      case NOTES_D5:
      case NOTES_Ds5: return 6;
      case NOTES_E5: return 7;
      case NOTES_F5:
      case NOTES_Fs5: return 8;
      case NOTES_G5:
      case NOTES_Gs5: return 9;
      case NOTES_A5:
      case NOTES_As5: return 10;
      default: return NOTES_OUT_OF_RANGE;
   }
}

static int key_sig_acc_count(enum key_sig key)
{
   switch (key) {
      case KEY_SIG_1_SHARP: return 1;
      case KEY_SIG_2_SHARP: return 2;
      case KEY_SIG_3_SHARP: return 3;
      case KEY_SIG_4_SHARP: return 4;
      case KEY_SIG_5_SHARP: return 5;
      case KEY_SIG_6_SHARP: return 6;
      case KEY_SIG_7_SHARP: return 7;
      case KEY_SIG_1_FLAT: return -1;
      case KEY_SIG_2_FLAT: return -2;
      case KEY_SIG_3_FLAT: return -3;
      case KEY_SIG_4_FLAT: return -4;
      case KEY_SIG_5_FLAT: return -5;
      case KEY_SIG_6_FLAT: return -6;
      case KEY_SIG_7_FLAT: return -7;
      default: return 0;
   }
}

static float calc_y(const enum midi_note n)
{
   ImVec2 p    = ImGui::GetCursorScreenPos();
   ImVec2 size = ImGui::GetContentRegionAvail();

   const float top       = p.y + size.y * 0.35F;
   const float bottom    = p.y + size.y * 0.8F;
   const float spacing   = (bottom - top) / 8.0F;
   const float staff_gap = spacing * 2.0F;

   float nb = 0;
   float y  = 0;

   if (n >= NOTES_E4) {
      nb = note_to_treble(n);
      if (nb == NOTES_OUT_OF_RANGE)
         return NOTES_OUT_OF_RANGE;

      const float treble_bottom = top + spacing * 4.0F;
      y = treble_bottom - spacing * (nb / 2.0F) - staff_gap / 2;

   } else {
      nb = note_to_bass(n);
      if (nb == NOTES_OUT_OF_RANGE)
         return NOTES_OUT_OF_RANGE;

      y = bottom - spacing * (nb / 2.0F) + staff_gap / 2;
   }

   return y;
}

static float calc_x(const int x_idx, const int x_sz)
{
   const float x_offs = 130;
   return x_offs + ((float)x_idx + 1) * x_sz / (10.0F);
}

static void draw_clefs(ImVec2 origin)
{
   float staff_space = fabsf(calc_y(NOTES_A3) - calc_y(NOTES_C4));
   float x           = origin.x + 8.0F;
   float fs          = 2.6F * staff_space;

   font_config cfg = {.fontsize     = fs,
      .anch         = ANCHOR_TOP_LEFT,
      .color        = STYLE_GRAY,
      .border_size  = 0.0F,
      .border_color = 0,
      .anchor_size  = 0.0F,
      .anchor_color = 0};

   // --- TREBLE CLEF ---
   float g_line   = calc_y(NOTES_G4);
   float y_treble = g_line - fs * 0.8F;
   style_text("\uE050", x, y_treble, &cfg);

   // --- BASS CLEF ---
   float f_line = calc_y(NOTES_F3);
   float y_bass = f_line - fs * 0.8F;
   style_text("\uE062", x, y_bass, &cfg);
}

static void draw_key_sig(struct state *state, ImVec2 origin, bool treble)
{
   float staff_space = std::fabs(calc_y(NOTES_A3) - calc_y(NOTES_C4));
   float fs          = 1.5F * staff_space;
   float x           = origin.x + fs * 2.4F;

   static const std::array<midi_note, 7> treble_sharps = {
      NOTES_F5, NOTES_C5, NOTES_G5, NOTES_D5, NOTES_A4, NOTES_E5, NOTES_B4};
   static const std::array<midi_note, 7> bass_sharps = {
      NOTES_F3, NOTES_C3, NOTES_G3, NOTES_D3, NOTES_A2, NOTES_E3, NOTES_B2};
   static const std::array<midi_note, 7> treble_flats = {
      NOTES_B4, NOTES_E5, NOTES_A4, NOTES_D5, NOTES_G4, NOTES_C5, NOTES_F4};
   static const std::array<midi_note, 7> bass_flats = {
      NOTES_B2, NOTES_E3, NOTES_A2, NOTES_D3, NOTES_G2, NOTES_C3, NOTES_F2};

   int acc_count = key_sig_acc_count(state->key);

   font_config cfg = {
      .fontsize = fs,
      .anch     = ANCHOR_CENTER,
      .color    = STYLE_GRAY,
   };

   if (acc_count > 0) { // sharps
      for (int i = 0; i < acc_count; ++i) {
         float y = calc_y(treble ? treble_sharps.at(i) : bass_sharps.at(i)) - 0.3F * fs;
         style_text(acc_sym(ACC_SHARP), x + static_cast<float>(i) * fs * 0.3F,
               y, &cfg);
      }
   } else if (acc_count < 0) { // flats
      for (int i = 0; i < -acc_count; ++i) {
         float y = calc_y(treble ? treble_flats.at(i) : bass_flats.at(i)) - 0.25F * fs;
         style_text(acc_sym(ACC_FLAT), x + static_cast<float>(i) * fs * 0.3F, y,
               &cfg);
      }
   }
}

void notes_staff(struct state *state)
{
   ImVec2 avail = ImGui::GetContentRegionAvail();
   ImGui::BeginChild("Staff", avail, true);

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();
   ImVec2 size           = ImGui::GetContentRegionAvail();

   static const std::array<midi_note, 10> staff_lines = {
      NOTES_G2, NOTES_B2, NOTES_D3, NOTES_F3, NOTES_A3,
      NOTES_E4, NOTES_G4, NOTES_B4, NOTES_D5, NOTES_F5};

   for (const auto &line : staff_lines) {
      float y = calc_y(line);
      if (y == NOTES_OUT_OF_RANGE)
         continue;

      draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + size.x, y), STYLE_GRAY,
            STYLE_LINE_THICKNESS);
   }

   draw_clefs(p);
   draw_key_sig(state, p, false);
   draw_key_sig(state, p, true);

   ImGui::EndChild();
}

static void draw_ledger_lines(float x, enum midi_note n, float note_radius)
{
   const float ledger_width = 4.0F * note_radius;

   // Convert note to staff position
   int pos = note_to_bass(n);
   const float y = calc_y(n);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   // Out of staff range? Staff is 0..8
   if (pos >= 0.0F && pos <= 8.0F)
      return;

   // Special case: just below ledger line
   if ((n == NOTES_D2) || (n == NOTES_Ds2)) {
      draw_ledger_lines(x, NOTES_E2, note_radius);
      return;
   }

   // Ledger lines only occur on LINE positions (even pos)
   if (pos % 2 != 0) // odd = space → no line
      return;

   // Draw the ledger line through the note
   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   draw_list->AddLine(ImVec2(x - ledger_width / 2, y),
         ImVec2(x + ledger_width / 2, y), STYLE_GRAY,
         STYLE_LINE_THICKNESS);
}

static void draw_accidental(float x, enum midi_note n, float note_radius, uint32_t color,
      enum accidental acc)
{
   if (acc == ACC_NONE)
      return;

   const float y = calc_y(n);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   float staff_space = std::fabs(calc_y(NOTES_A3) - calc_y(NOTES_C4));
   float fs          = 2.0F * staff_space;

   font_config cfg = {.fontsize     = fs,
      .anch         = ANCHOR_CENTER_RIGHT,
      .color        = color,
      .border_size  = 1.0F, // optional debug
      .border_color = STYLE_RED,
      .anchor_size  = 5.0F,
      .anchor_color = STYLE_RED};

   float offset_x = x - 1.5F * note_radius;
   style_text(acc_sym(acc), offset_x, y, &cfg);
}

static void notes_dot(enum key_sig key, enum midi_note n, int x_idx, uint32_t color)
{
   ImVec2 size             = ImGui::GetContentRegionAvail();
   const float note_radius = size.y / 45.0F;

   const float x = calc_x(x_idx, size.x);

   draw_accidental(x, n, note_radius, color, key_sig_accidental(key, n));
   draw_ledger_lines(x, n, note_radius);

   const float y = calc_y(n);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);
}

static void draw_chord_figures(float font_size, float x, float y,
      const std::vector<figure> &figs, uint32_t color)
{
   ImVec2 size               = ImGui::GetContentRegionAvail();
   const float figure_offset = size.y / 8.0F;
   float offset_step         = figure_offset * 0.5F;

   font_config cfg = {
      .fontsize = font_size, .anch = ANCHOR_TOP_LEFT, .color = color};

   for (size_t i = 0; i < figs.size(); ++i) {
      // figure
      float fx = x - 0.25F * font_size;
      float fy = y - (float)i * offset_step - figure_offset;
      style_text(std::to_string(figs[i].num).c_str(), fx, fy, &cfg);

      // accidental
      if (figs[i].acc != ACC_NONE) {
         float nx = fx - 0.7F * font_size;
         float ny = fy - 0.30F * font_size;
         style_text(acc_sym(figs[i].acc), nx, ny, &cfg);
      }
   }
}

void notes_draw(const struct state *state)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   ImVec2 size     = ImGui::GetContentRegionAvail();
   float font_size = size.y / 16.0F;

   for (size_t i = 0; i < state->chords.size(); ++i) {
      const auto &col = state->chords[i];

      // --- Bass notes ---
      for (auto n : col.bass) {
         notes_dot(state->key, n, static_cast<int>(i), STYLE_WHITE);
      }

      // --- Good chords ---
      for (auto n : col.good) {
         notes_dot(state->key, n, static_cast<int>(i), STYLE_GREEN);
      }

      // --- Bad chords ---
      for (auto n : col.bad) {
         notes_dot(state->key, n, static_cast<int>(i), STYLE_RED);
      }

      // --- Figures (typically for bass) ---
      if (!col.bass.empty()) {
         const float x = calc_x(i, size.x);
         const float y = calc_y(*col.bass.begin());
         if (y != NOTES_OUT_OF_RANGE) {
            draw_chord_figures(font_size, x, y, col.figures, STYLE_WHITE);
         }
      }
   }

   ImGui::EndChild();
}

