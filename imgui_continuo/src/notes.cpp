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
#include "theory.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#define CHORD_SEP 79.0F
#define N_CHORDS_LEFT 3

struct acc {
   const char* sym;
   float dx;
   float dy;
};

static struct acc acc_sym(enum accidental a)
{
   switch (a) {
      case ACC_SHARP:   return {"\uE262", -0.7F, -0.3F};
      case ACC_FLAT:    return {"\uE260", -0.7F, -0.3F};
      case ACC_NATURAL: return {"\uE261", -0.7F, -0.3F};
      case ACC_SLASH:   return {"/", 0.09F, 0.0F};
      default: return {"", 0.0F, 0.0F};
   }
}

static float calc_x(const int x_idx)
{
   ImVec2 p           = ImGui::GetCursorScreenPos();
   const float x_offs = 130;
   return p.x + x_offs + ((float)x_idx + 1) * CHORD_SEP;
}

static float calc_y(const enum midi_note n, enum key_sig key)
{
   ImVec2 p              = ImGui::GetCursorScreenPos();
   const float spacing   = 15.0F;
   const float top       = 3.0F * spacing;
   const float bottom    = 12.0F * spacing;
   const float staff_gap = spacing * 2.0F;

   // treble staff
   if (n >= NOTES_E4) {
      const int nb = note_to_treble(n, key);
      if (nb == NOTES_OUT_OF_RANGE)
         return NOTES_OUT_OF_RANGE;

      const float treble_bottom = top + spacing * 4.0F;
      return p.y + treble_bottom - spacing * ((float)nb / 2.0F) - staff_gap / 2;
   }

   // bass staff
   {
      const int nb = note_to_bass(n, key);
      if (nb == NOTES_OUT_OF_RANGE)
         return NOTES_OUT_OF_RANGE;

      return p.y + bottom - spacing * ((float)nb / 2.0F) + staff_gap / 2;
   }
}

static float staff_space(void)
{
   return fabsf(calc_y(NOTES_A3, KEY_SIG_0) - calc_y(NOTES_C4, KEY_SIG_0));
}

static void draw_clefs(ImVec2 origin)
{
   float x  = origin.x + 8.0F;
   float fs = 2.6F * staff_space();

   font_config cfg = {.fontsize     = fs,
                      .anch         = ANCHOR_TOP_LEFT,
                      .color        = STYLE_GRAY,
                      .border_size  = 0.0F,
                      .border_color = 0,
                      .anchor_size  = 0.0F,
                      .anchor_color = 0};

   // --- TREBLE CLEF ---
   float g_line   = calc_y(NOTES_G4, KEY_SIG_0);
   float y_treble = g_line - fs * 0.8F;
   style_text("\uE050", x, y_treble, &cfg);

   // --- BASS CLEF ---
   float f_line = calc_y(NOTES_F3, KEY_SIG_0);
   float y_bass = f_line - fs * 0.8F;
   style_text("\uE062", x, y_bass, &cfg);
}

static void draw_key_sig(struct state *state, ImVec2 origin, bool treble)
{
   float fs = 1.5F * staff_space();
   float x  = origin.x + fs * 2.4F;

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
         float y = calc_y(treble ? treble_sharps.at(i) : bass_sharps.at(i),
                          KEY_SIG_0) -
                   0.3F * fs;
         style_text(acc_sym(ACC_SHARP).sym, x + static_cast<float>(i) * fs * 0.3F,
                    y, &cfg);
      }
   } else if (acc_count < 0) { // flats
      for (int i = 0; i < -acc_count; ++i) {
         float y =
             calc_y(treble ? treble_flats.at(i) : bass_flats.at(i), KEY_SIG_0) -
             0.25F * fs;
         style_text(acc_sym(ACC_FLAT).sym, x + static_cast<float>(i) * fs * 0.3F, y,
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
      float y = calc_y(line, KEY_SIG_0);
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

static void draw_ledger_lines(float x, enum midi_note n, float note_radius,
                              enum key_sig key)
{
   const float ledger_width = 4.0F * note_radius;

   // Normalize special cases: D2 and D#2 use E2’s ledger line
   if (n == NOTES_D2 || n == NOTES_Ds2)
      n = NOTES_E2;

   // Convert note to staff position
   const float y = calc_y(n, key);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   // Out of staff range? Staff is 0..8
   int pos = note_to_bass(n, key);
   if (pos >= 0 && pos <= 8)
      return;

   // Ledger lines only occur on LINE positions (even pos)
   if (pos % 2 != 0) // odd = space → no line
      return;

   // Draw the ledger line through the note
   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   draw_list->AddLine(ImVec2(x - ledger_width / 2, y),
                      ImVec2(x + ledger_width / 2, y), STYLE_GRAY,
                      STYLE_LINE_THICKNESS);
}

static void draw_accidental(float x, enum midi_note n, float note_radius,
                            uint32_t color, enum accidental acc,
                            enum key_sig key)
{
   if (acc == ACC_NONE)
      return;

   float fs = 2.0F * staff_space();

   font_config cfg = {
       .fontsize = fs,
       .anch     = ANCHOR_CENTER_RIGHT,
       .color    = color,
   };

   const float offset_x = x - 0.6F * note_radius;
   const float y        = calc_y(n, key) - 0.76F * note_radius;
   if (y == NOTES_OUT_OF_RANGE)
      return;

   style_text(acc_sym(acc).sym, offset_x, y, &cfg);
}

static void notes_dot(enum key_sig key, enum midi_note n, int x_idx,
                      uint32_t color)
{
   const float note_radius = 0.44F * staff_space();

   const float x = calc_x(x_idx);

   draw_accidental(x, n, note_radius, color, key_sig_accidental(key, n), key);
   draw_ledger_lines(x, n, note_radius, key);

   const float y = calc_y(n, key);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);
}

static void draw_chord_figures(float font_size, float x, float y,
                               const std::vector<figure> &figs, uint32_t color)
{
   font_config cfg = {
       .fontsize = font_size, .anch = ANCHOR_TOP_LEFT, .color = color};

   for (size_t i = 0; i < figs.size(); ++i) {
      // figure
      float fx = x - 0.25F * font_size;
      float fy = y - (float)i * 0.9F * font_size - 1.5F * font_size;
      style_text(std::to_string(figs[i].num).c_str(), fx, fy, &cfg);

      // accidental
      if (figs[i].acc != ACC_NONE) {
         struct acc a = acc_sym(figs[i].acc);
         float nx = fx + a.dx * font_size;
         float ny = fy + a.dy * font_size;
         style_text(a.sym, nx, ny, &cfg);
      }
   }
}

static int chords_per_screen(float width)
{
    // floor(), but guard against divide-by-zero if width < CHORD_SEP
    if (CHORD_SEP <= 0.0f)
        return 1;

    int cps = static_cast<int>(width / CHORD_SEP);
    return (cps > 0) ? cps : 1;
}

static void compute_visible_range(int total,
                                  int active,
                                  int cps,
                                  int n_left,
                                  int &start,
                                  int &end)
{
    if (total <= 0) {
        start = end = 0;
        return;
    }

    // Try to center active with n_left before it
    start = active - n_left;

    // Clamp
    if (start < 0)
        start = 0;

    end = start + cps;
    if (end > total)
        end = total;

    // Fix start again if we clipped at the end
    if (end - start > cps)
        start = end - cps;
    if (start < 0)
        start = 0;
}

void notes_draw(const struct state *state)
{
    if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
        ImGui::EndChild();
        return;
    }

    const int total = static_cast<int>(state->chords.size());
    if (total == 0) {
        ImGui::EndChild();
        return;
    }

    ImVec2 size = ImGui::GetContentRegionAvail();
    const int cps = chords_per_screen(size.x);
    const int active = static_cast<int>(state->active_col);

    int start = 0, end = 0;
    compute_visible_range(total, active, cps, N_CHORDS_LEFT, start, end);

    for (int i = start; i < end; ++i) {
        const auto &col = state->chords[i];
        const int idx = i - start;  // screen-space index

        for (auto n : col.bass)
            notes_dot(state->key, n, idx, STYLE_WHITE);

        for (auto n : col.good)
            notes_dot(state->key, n, idx, STYLE_GREEN);

        for (auto n : col.bad)
            notes_dot(state->key, n, idx, STYLE_RED);

        if (state->edit_lesson)
            for (auto n : col.answer)
                notes_dot(state->key, n, idx, STYLE_YELLOW);

        if (!col.bass.empty()) {
            float x = calc_x(idx);
            float y = calc_y(*col.bass.begin(), state->key);
            if (y != NOTES_OUT_OF_RANGE) {
                const float fs = 1.7F * staff_space();
                draw_chord_figures(fs, x, y, col.figures, STYLE_WHITE);
            }
        }
    }

    ImGui::EndChild();
}

