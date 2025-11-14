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

void notes_staff(void)
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

static void draw_sharp(ImDrawList *draw_list, float font_size, float x, float y,
                       float note_radius, uint32_t color)
{
   const char *sharp = "#";
   ImFont *font      = ImGui::GetFont();
   ImVec2 text_size  = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, sharp);

   draw_list->AddText(
       font, font_size,
       ImVec2(x - note_radius - text_size.x, y - text_size.y / 2), color,
       sharp);
}

static void notes_dot(midi_note n, int x_idx, uint32_t color)
{
   ImVec2 size             = ImGui::GetContentRegionAvail();
   const float note_radius = size.y / 45.0F;
   float font_size         = 3.0F * note_radius;

   float x = ((float)x_idx + 1) * size.x / (MAX_CHORDS + 1.0F);
   float y = calc_y(n);
   if (y == NOTES_OUT_OF_RANGE)
      return;

   ImDrawList *draw_list = ImGui::GetWindowDrawList();

   if (note_has_sharp(n))
      draw_sharp(draw_list, font_size, x, y, note_radius, color);

   draw_ledger_lines(draw_list, x, y, static_cast<int>(n), note_radius);
   draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);
}

void notes_single(const std::vector<midi_note> &notes, uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   for (size_t i = 0; i < notes.size(); ++i)
      notes_dot(notes[i], static_cast<int>(i), color);

   ImGui::EndChild();
}

void notes_chords(const std::vector<std::unordered_set<midi_note>> &chords,
                  uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   int x_idx = 0;
   for (const auto &chord : chords) {
      for (auto n : chord) {
         notes_dot(n, x_idx, color);
      }
      ++x_idx;
   }

   ImGui::EndChild();
}

static const char *accidental_symbol(enum accidental a)
{
   switch (a) {
      case ACC_SHARP: return "#";
      case ACC_FLAT: return "b";
      case ACC_NATURAL: return "n";
      default: return "";
   }
}

static void draw_figure(ImDrawList *draw, ImFont *font, float font_size,
                        float x, float y, const figure &fig, uint32_t color,
                        float figure_offset)
{
   // number
   const std::string num_str = std::to_string(fig.num);
   ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, num_str.c_str());

   float fx = x - ts.x * 0.5F;
   float fy = y - figure_offset;

   draw->AddText(font, font_size, ImVec2(fx, fy), color, num_str.c_str());

   // accidental
   if (fig.acc != ACC_NONE) {
      const char *acc = accidental_symbol(fig.acc);
      ImVec2 as       = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, acc);
      draw->AddText(font, font_size, ImVec2(fx - as.x - 2.0F, fy), color, acc);
   }
}

static void draw_chord_figures(ImDrawList *draw, ImFont *font, float font_size,
                               float x, float y,
                               const std::vector<figure> &figs, uint32_t color,
                               float figure_offset)
{
   // space between stacked figures
   float offset_step = figure_offset * 0.5F;

   for (size_t i = 0; i < figs.size(); ++i) {
      draw_figure(draw, font, font_size, x, y - (float)i * offset_step, figs[i],
                  color, figure_offset);
   }
}

void notes_figures(const std::vector<midi_note> &notes,
                   const std::vector<std::vector<figure>> &figures,
                   uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   ImDrawList *draw          = ImGui::GetWindowDrawList();
   ImVec2 size               = ImGui::GetContentRegionAvail();
   const float figure_offset = size.y / 8.0F;
   float font_size           = size.y / 16.0F;
   ImFont *font              = ImGui::GetFont();

   const size_t n = std::min(notes.size(), figures.size());

   for (size_t i = 0; i < n; i++) {
      const midi_note note = notes[i];
      const auto &figs     = figures[i];

      float x = ((float)i + 1) * size.x / (MAX_CHORDS + 1.0F);
      float y = calc_y(note);
      if (y == NOTES_OUT_OF_RANGE)
         continue;

      draw_chord_figures(draw, font, font_size, x, y, figs, color,
                         figure_offset);
   }

   ImGui::EndChild();
}
