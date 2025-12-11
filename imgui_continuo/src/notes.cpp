// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file notes.cpp
 * @brief Note manipulation routines.
 * @author Jakob Kastelic
 */

#include "notes.h"
#include "imgui.h"
#include "state.h"
#include "style.h"
#include "theory.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

constexpr float chord_sep = 48.0F;

struct acc {
   const char *sym;
   float dx; // horiz from number
   float dy; // vert from number
   float xx; // horiz w/o numnber
   float yy; // vert w/o numer
};

static struct acc acc_sym(enum accidental a)
{
   switch (a) {
      case ACC_SHARP:
         return {.sym = "\uE262",
                 .dx  = -0.7F,
                 .dy  = -0.3F,
                 .xx  = -0.25F,
                 .yy  = -0.3F};
      case ACC_FLAT:
         return {.sym = "\uE260",
                 .dx  = -0.7F,
                 .dy  = -0.3F,
                 .xx  = -0.25F,
                 .yy  = -0.3F};
      case ACC_NATURAL:
         return {.sym = "\uE261",
                 .dx  = -0.7F,
                 .dy  = -0.3F,
                 .xx  = -0.25F,
                 .yy  = -0.3F};
      case ACC_SLASH:
         return {.sym = "/", .dx = 0.09F, .dy = 0.0F, .xx = 0.0F, .yy = 0.0F};
      default:
         return {.sym = "", .dx = 0.0F, .dy = 0.0F, .xx = 0.0F, .yy = 0.0F};
   }
}

static float calc_x(const int x_idx, const enum key_sig key)
{
   const ImVec2 p = ImGui::GetCursorScreenPos();
   const float x_offs =
       40 + (7.50F * static_cast<float>(std::abs(th_key_sig_acc_count(key))));
   return p.x + x_offs + (((float)x_idx + 1) * chord_sep);
}

static float calc_y(const enum note_name nn, enum key_sig key)
{
   const ImVec2 p        = ImGui::GetCursorScreenPos();
   const float spacing   = 15.0F;
   const float top       = 3.0F * spacing;
   const float bottom    = 12.0F * spacing;
   const float staff_gap = spacing * 2.0F;

   // treble staff
   if (nn >= NN_Db4) {
      const int nb = th_note_to_treble(nn, key);
      if (nb == NOTES_OUT_OF_RANGE)
         return NOTES_OUT_OF_RANGE;

      const float treble_bottom = top + (spacing * 4.0F);
      return p.y + treble_bottom - (spacing * ((float)nb / 2.0F)) -
             (staff_gap / 2);
   }

   // bass staff
   {
      const int nb = th_note_to_bass(nn, key);
      if (nb == NOTES_OUT_OF_RANGE)
         return NOTES_OUT_OF_RANGE;

      return p.y + bottom - (spacing * ((float)nb / 2.0F)) + (staff_gap / 2);
   }
}

static float staff_space(void)
{
   return fabsf(calc_y(NN_A3, KEY_SIG_0) - calc_y(NN_C4, KEY_SIG_0));
}

static void draw_clefs(ImVec2 origin)
{
   const float x  = origin.x + 8.0F;
   const float fs = 2.6F * staff_space();

   const font_config cfg = {.fontsize     = fs,
                            .anch         = ANCHOR_TOP_LEFT,
                            .color        = STYLE_GRAY,
                            .border_size  = 0.0F,
                            .border_color = 0,
                            .anchor_size  = 0.0F,
                            .anchor_color = 0};

   // --- TREBLE CLEF ---
   const float g_line   = calc_y(NN_G4, KEY_SIG_0);
   const float y_treble = g_line - (fs * 0.8F);
   style_text("\uE050", x, y_treble, &cfg);

   // --- BASS CLEF ---
   const float f_line = calc_y(NN_F3, KEY_SIG_0);
   const float y_bass = f_line - (fs * 0.8F);
   style_text("\uE062", x, y_bass, &cfg);
}

static void draw_key_sig(struct state *state, ImVec2 origin, bool treble)
{
   const float fs = 1.5F * staff_space();
   const float x  = origin.x + (fs * 2.4F);

   static const std::array<note_name, 7> treble_sharps = {
       NN_F5, NN_C5, NN_G5, NN_D5, NN_A4, NN_E5, NN_B4};
   static const std::array<note_name, 7> bass_sharps = {
       NN_F3, NN_C3, NN_G3, NN_D3, NN_A2, NN_E3, NN_B2};
   static const std::array<note_name, 7> treble_flats = {
       NN_B4, NN_E5, NN_A4, NN_D5, NN_G4, NN_C5, NN_F4};
   static const std::array<note_name, 7> bass_flats = {
       NN_B2, NN_E3, NN_A2, NN_D3, NN_G2, NN_C3, NN_F2};

   const int acc_count = th_key_sig_acc_count(state->lesson.key);

   const font_config cfg = {
       .fontsize = fs,
       .anch     = ANCHOR_CENTER,
       .color    = STYLE_GRAY,
   };

   if (acc_count > 0) { // sharps
      for (int i = 0; i < acc_count; ++i) {
         const float y =
             calc_y(treble ? treble_sharps.at(i) : bass_sharps.at(i),
                    KEY_SIG_0) -
             (0.3F * fs);
         style_text(acc_sym(ACC_SHARP).sym,
                    x + (static_cast<float>(i) * fs * 0.3F), y, &cfg);
      }
   } else if (acc_count < 0) { // flats
      for (int i = 0; i < -acc_count; ++i) {
         const float y =
             calc_y(treble ? treble_flats.at(i) : bass_flats.at(i), KEY_SIG_0) -
             (0.25F * fs);
         style_text(acc_sym(ACC_FLAT).sym,
                    x + (static_cast<float>(i) * fs * 0.3F), y, &cfg);
      }
   }
}

void notes_staff(struct state *state)
{
   const ImVec2 avail = ImGui::GetContentRegionAvail();
   ImGui::BeginChild("Staff", avail, true);

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   const ImVec2 p        = ImGui::GetCursorScreenPos();
   const ImVec2 size     = ImGui::GetContentRegionAvail();

   static const std::array<note_name, 10> staff_lines = {
       NN_G2, NN_B2, NN_D3, NN_F3, NN_A3, NN_E4, NN_G4, NN_B4, NN_D5, NN_F5};

   for (const auto &line : staff_lines) {
      const float y = calc_y(line, KEY_SIG_0);
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

static bool need_ledger(const enum note_name nn, enum key_sig key)
{
   int pos = th_note_to_bass(nn, key);

   // bass
   if (pos != NOTES_OUT_OF_RANGE) {
      if (pos <= -2 || pos > 8)
         if (pos % 2 == 0)
            return true;
   }

   // treble
   else {
      pos = th_note_to_treble(nn, key);
      if (pos != NOTES_OUT_OF_RANGE)
         if (pos < 0 || pos >= 10)
            if (pos % 2 == 0)
               return true;
   }

   return false;
}

static void draw_ledger_lines(float x, enum note_name nn, float note_radius,
                              enum key_sig key)
{
   const float ledger_width = 4.0F * note_radius;

   // Normalize special cases: D2, Db2, and D#2 use E2's ledger line
   if (nn == NN_D2 || nn == NN_Ds2 || nn == NN_Db2)
      nn = NN_E2;

   // Convert note to staff position
   const float y = calc_y(nn, key);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   // Do we need ledger lines at all?
   if (!need_ledger(nn, key))
      return;

   // Draw the ledger line through the note
   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   draw_list->AddLine(ImVec2(x - (ledger_width / 2), y),
                      ImVec2(x + (ledger_width / 2), y), STYLE_GRAY,
                      STYLE_LINE_THICKNESS);
}

static void draw_accidental(float x, enum note_name nn, float note_radius,
                            uint32_t color, enum accidental acc,
                            enum key_sig key)
{
   if (acc == ACC_NONE)
      return;

   const float fs = 2.0F * staff_space();

   const font_config cfg = {
       .fontsize = fs,
       .anch     = ANCHOR_CENTER_RIGHT,
       .color    = color,
   };

   const float offset_x = x - (0.6F * note_radius);
   const float y        = calc_y(nn, key) - (0.76F * note_radius);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   style_text(acc_sym(acc).sym, offset_x, y, &cfg);
}

static void notes_dot(enum key_sig key, enum midi_note n, int x_idx,
                      uint32_t color)
{
   const float note_radius = 0.44F * staff_space();
   const float x           = calc_x(x_idx, key);

   const enum note_name nn = th_preferred_spelling(n, key);
   const float y           = calc_y(nn, key);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   draw_accidental(x, nn, note_radius, color, th_key_sig_accidental(key, nn),
                   key);
   draw_ledger_lines(x, nn, note_radius, key);

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);
}

static void draw_chord_figures(float fs, float x, float y,
                               const std::vector<figure> &figs, uint32_t color)
{
   const font_config cfg = {
       .fontsize = fs, .anch = ANCHOR_TOP_LEFT, .color = color};

   for (size_t i = 0; i < figs.size(); ++i) {
      // figure
      const float fx = x - (0.25F * fs);
      const float fy = y - ((float)i * 0.9F * fs) - (1.5F * fs);
      if (figs[i].num != 0)
         style_text(std::to_string(figs[i].num).c_str(), fx, fy, &cfg);

      // accidental
      if (figs[i].acc != ACC_NONE) {
         const struct acc a = acc_sym(figs[i].acc);
         if (figs[i].num == 0)
            style_text(a.sym, fx + (a.xx * fs), fy + (a.yy * fs), &cfg);
         else
            style_text(a.sym, fx + (a.dx * fs), fy + (a.dy * fs), &cfg);
      }
   }
}

static int chords_per_screen(float width)
{
   // floor(), but guard against divide-by-zero if width < chord_sep
   if (chord_sep <= 0.0F)
      return 1;

   const int cps = static_cast<int>(width / chord_sep);
   return (cps > 0) ? cps : 1;
}

static void compute_visible_range(int total, int active, int cps, int n_left,
                                  int &start, int &end)
{
   if (total <= 0) {
      start = end = 0;
      return;
   }

   // If everything fits including barlines:
   if (total <= cps - 2) {
      start = 0;
      end   = total;
      return;
   }

   // Default: final note not visible → reserve 1 slot for right edge
   const int usable = cps - 1;

   // Try centering active with n_left before it
   start = active - n_left;
   start = std::max(start, 0);

   end = start + usable;

   if (end >= total) {
      // Final region reached → reserve 2 slots for final double barline
      end   = total;
      start = total - (cps - 1);
      start = std::max(start, 0);
   }
}

static void draw_active_col_cursor(int x_idx, const enum key_sig key)
{
   const uint32_t color = IM_COL32(255, 255, 255, 25);
   const float margin   = 20.0F;

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   const ImVec2 avail    = ImGui::GetContentRegionAvail();
   const ImVec2 origin   = ImGui::GetCursorScreenPos();

   // Compute X position for the left and right edges of the rectangle
   const float x_left  = calc_x(x_idx, key) - margin;
   const float x_right = calc_x(x_idx, key) + margin;

   // Y spans entire available content region (grand staff)
   const float y_top    = origin.y;
   const float y_bottom = origin.y + avail.y;

   draw_list->AddRectFilled(ImVec2(x_left, y_top), ImVec2(x_right, y_bottom),
                            color);
}

static void handle_chord_click(int screen_idx, int chord_idx,
                               struct state *state)
{
   const ImVec2 origin = ImGui::GetCursorScreenPos();
   const ImVec2 avail  = ImGui::GetContentRegionAvail();
   const float margin  = 20.0F;

   const float x_left   = calc_x(screen_idx, state->lesson.key) - margin;
   const float x_right  = calc_x(screen_idx, state->lesson.key) + margin;
   const float y_top    = origin.y;
   const float y_bottom = origin.y + avail.y;

   const ImGuiIO &io   = ImGui::GetIO();
   const bool hovering = ImGui::IsMouseHoveringRect(ImVec2(x_left, y_top),
                                                    ImVec2(x_right, y_bottom));

   ImDrawList *draw = ImGui::GetWindowDrawList();

   // Draw hover highlight
   if (hovering) {
      const ImU32 color = IM_COL32(200, 200, 200, 50); // light gray
      draw->AddRectFilled(ImVec2(x_left, y_top), ImVec2(x_right, y_bottom),
                          color);
   }

   // Draw pressed highlight if mouse is down over this column
   if (hovering && io.MouseDown[0]) {
      const ImU32 color = IM_COL32(255, 200, 0, 50); // light orange
      draw->AddRectFilled(ImVec2(x_left, y_top), ImVec2(x_right, y_bottom),
                          color);
   }

   // Execute action on mouse release
   if (hovering && io.MouseReleased[0]) {
      state->ui.active_col = chord_idx;
      if (state->ui.active_col < state->lesson.chords.size()) {
         const struct column &col = state->lesson.chords[state->ui.active_col];
         state->ui.figs_entry     = th_fig_to_string(col.figures);
      }
   }
}

static void draw_final_barline(int screen_idx, const enum key_sig key)
{
   ImDrawList *dl = ImGui::GetWindowDrawList();
   const float x  = calc_x(screen_idx + 1, key);

   const float y_top    = calc_y(NN_F5, KEY_SIG_0);
   const float y_bottom = calc_y(NN_G2, KEY_SIG_0);

   const float thin  = 1.0F;
   const float thick = 3.50F;
   const float sep   = staff_space() * 0.30F;

   // Thin line on left
   dl->AddLine(ImVec2(x - sep, y_top), ImVec2(x - sep, y_bottom), STYLE_GRAY,
               thin);

   // Thick line on right
   dl->AddLine(ImVec2(x + sep, y_top), ImVec2(x + sep, y_bottom), STYLE_GRAY,
               thick);
}

void notes_draw(struct state *state)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   const int total = static_cast<int>(state->lesson.chords.size());
   if (total == 0) {
      ImGui::EndChild();
      return;
   }

   const ImVec2 size = ImGui::GetContentRegionAvail();
   const int cps     = chords_per_screen(size.x);
   const int active  = static_cast<int>(state->ui.active_col);

   int start = 0;
   int end   = 0;
   compute_visible_range(total, active, cps, cps / 3, start, end);

   draw_active_col_cursor(active - start, state->lesson.key);

   for (int i = start; i < end; ++i) {
      const auto &col = state->lesson.chords[i];
      const int idx   = i - start; // screen-space index

      handle_chord_click(idx, i, state);

      for (auto n : col.bass)
         notes_dot(state->lesson.key, n, idx, STYLE_WHITE);

      for (auto n : col.good)
         notes_dot(state->lesson.key, n, idx, STYLE_GREEN);

      for (auto n : col.bad)
         notes_dot(state->lesson.key, n, idx, STYLE_RED);

      for (auto n : col.missed)
         notes_dot(state->lesson.key, n, idx, STYLE_YELLOW);

      if (state->ui.edit_lesson)
         for (auto n : col.answer)
            notes_dot(state->lesson.key, n, idx, STYLE_BLUE);

      if (!col.bass.empty()) {
         const float x = calc_x(idx, state->lesson.key);
         const float y =
             calc_y(th_preferred_spelling(*col.bass.begin(), state->lesson.key),
                    state->lesson.key);
         if (y != NOTES_OUT_OF_RANGE) {
            const float fs = 1.7F * staff_space();
            draw_chord_figures(fs, x, y, col.figures, STYLE_WHITE);
         }
      }

      // Final barline: only if this is the last chord overall
      if (i == total - 1)
         draw_final_barline(idx, state->lesson.key);
   }

   ImGui::EndChild();
}
