// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file notes.cpp
 * @brief Note manupulation routines.
 * @author Jakob Kastelic
 */

#include "notes.h"
#include "imgui.h"
#include "logic.h"
#include "state.h"
#include "style.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

#define NOTES_OUT_OF_RANGE (-100)

static float note_to_bass(const enum midi_note n)
{
   switch (n) {
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

static bool note_has_sharp(const enum midi_note n)
{
   switch (n % 12) {
      case 1:  // C#
      case 3:  // D#
      case 6:  // F#
      case 8:  // G#
      case 10: // A#
         return true;
      default: return false;
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

static void draw_clefs(struct state *state, ImDrawList *draw_list,
                       ImVec2 origin)
{
   float staff_space = fabsf(calc_y(NOTES_A3) - calc_y(NOTES_C4));
   float x           = origin.x + 8.0F;
   float ch          = staff_space * STYLE_FONT_RATIO;

   // --- TREBLE CLEF ---
   float g_line   = calc_y(NOTES_G4);
   float y_treble = g_line - ch * 0.5F;
   draw_list->AddText(state->music_font, ch, ImVec2(x, y_treble), STYLE_GRAY,
                      "\uE050");

   // --- BASS CLEF ---
   float f_line = calc_y(NOTES_F3);
   float y_bass = f_line - ch * 0.5F;
   draw_list->AddText(state->music_font, ch, ImVec2(x, y_bass), STYLE_GRAY,
                      "\uE062");
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

   draw_clefs(state, draw_list, p);

   ImGui::EndChild();
}

static void draw_ledger_lines(ImDrawList *draw_list, float x, float y,
                              int note_val, float note_radius)
{
   const float ledger_width = 4.0F * note_radius;

   // Convert note to staff position
   float pos = note_to_bass((midi_note)note_val);

   // Out of staff range? Staff is 0..8
   if (pos >= 0.0F && pos <= 8.0F)
      return;

   // Ledger lines only occur on LINE positions (even pos)
   if (((unsigned int)pos & 1U) != 0) // odd = space → no line
      return;

   // Draw the ledger line through the note
   draw_list->AddLine(ImVec2(x - ledger_width / 2, y),
                      ImVec2(x + ledger_width / 2, y), STYLE_GRAY,
                      STYLE_LINE_THICKNESS);
}

static void draw_sharp(struct state *state, ImDrawList *draw_list, float x,
                       float y, float note_radius, uint32_t color)
{
   float staff_space = fabsf(calc_y(NOTES_A3) - calc_y(NOTES_C4));
   float font_size   = staff_space * 7.0F;
   const char *sharp = "\uE262";

   ImVec2 text_size =
       state->music_font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, sharp);
   ImVec2 pos(x - 1.5F * note_radius - text_size.x, y - text_size.y / 2);

   draw_list->AddText(state->music_font, font_size, pos, color, sharp);
}

static void notes_dot(struct state *state, midi_note n, int x_idx,
                      uint32_t color)
{
   ImVec2 size             = ImGui::GetContentRegionAvail();
   const float note_radius = size.y / 45.0F;

   float x = ((float)x_idx + 1) * size.x / (MAX_CHORDS + 1.0F);
   float y = calc_y(n);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   ImDrawList *draw_list = ImGui::GetWindowDrawList();

   if (note_has_sharp(n))
      draw_sharp(state, draw_list, x, y, note_radius, color);

   draw_ledger_lines(draw_list, x, y, static_cast<int>(n), note_radius);
   draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);
}

void notes_single(struct state *state, const std::vector<midi_note> &notes,
                  uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   for (size_t i = 0; i < notes.size(); ++i)
      notes_dot(state, notes[i], static_cast<int>(i), color);

   ImGui::EndChild();
}

void notes_chords(struct state *state,
                  const std::vector<std::unordered_set<midi_note>> &chords,
                  uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   int x_idx = 0;
   for (const auto &chord : chords) {
      for (auto n : chord) {
         notes_dot(state, n, x_idx, color);
      }
      ++x_idx;
   }

   ImGui::EndChild();
}

static const char *accidental_symbol(enum accidental a)
{
   switch (a) {
      case ACC_SHARP: return "\uE262";   // SMuFL sharp
      case ACC_FLAT: return "\uE260";    // SMuFL flat
      case ACC_NATURAL: return "\uE261"; // SMuFL natural
      default: return "";
   }
}

static void draw_chord_figures(struct state *state, ImDrawList *draw,
                               float font_size, float x, float y,
                               const std::vector<figure> &figs, uint32_t color)
{
   ImVec2 size               = ImGui::GetContentRegionAvail();
   const float figure_offset = size.y / 8.0F;
   float offset_step         = figure_offset * 0.5F;

   for (size_t i = 0; i < figs.size(); ++i) {
      // figure
      float fx = x - 0.25F * font_size;
      float fy = y - (float)i * offset_step - figure_offset;
      draw->AddText(ImGui::GetFont(), font_size, ImVec2(fx, fy), color,
                    std::to_string(figs[i].num).c_str());

      // accidental
      float nx = fx - 0.5F * font_size;
      float ny = fy - 0.41F * STYLE_FONT_RATIO * font_size;
      if (figs[i].acc != ACC_NONE)
         draw->AddText(state->music_font, STYLE_FONT_RATIO * font_size,
                       ImVec2(nx, ny), color, accidental_symbol(figs[i].acc));
   }
}

void notes_figures(struct state *state, const std::vector<midi_note> &notes,
                   const std::vector<std::vector<figure>> &figures,
                   uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   ImDrawList *draw = ImGui::GetWindowDrawList();
   ImVec2 size      = ImGui::GetContentRegionAvail();
   float font_size  = size.y / 16.0F;

   const size_t n = std::min(notes.size(), figures.size());

   for (size_t i = 0; i < n; i++) {
      const midi_note note = notes[i];
      const auto &figs     = figures[i];

      float x = ((float)i + 1) * size.x / (MAX_CHORDS + 1.0F);
      float y = calc_y(note);
      if (y == NOTES_OUT_OF_RANGE)
         continue;

      draw_chord_figures(state, draw, font_size, x, y, figs, color);
   }

   ImGui::EndChild();
}
