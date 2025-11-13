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
#include <span>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static int note_to_bass(const enum midi_note n)
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
      default: return -3;
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

   const float top     = p.y + size.y * 0.2F;
   const float bottom  = p.y + size.y * 0.8F;
   const float spacing = (bottom - top) / 4.0F;

   return bottom - spacing * (float)note_to_bass(n) / 2.0F;
}

void notes_staff(void)
{
   ImVec2 avail = ImGui::GetContentRegionAvail();

   ImGui::BeginChild("Staff", avail, true);

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();
   ImVec2 size           = ImGui::GetContentRegionAvail();

   static const enum midi_note staff_lines[] = {NOTES_G2, NOTES_B2, NOTES_D3,
                                                NOTES_F3, NOTES_A3};

   for (const auto &line : staff_lines) {
      float y = calc_y(line);
      draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + size.x, y), STYLE_GRAY,
                         STYLE_LINE_THICKNESS);
   }

   ImGui::EndChild();
}

static void draw_ledger_lines(ImDrawList *draw_list, float x, float y,
                              int note_val, float note_radius)
{
   const float ledger_width = 4.0F * note_radius;

   if (note_val < NOTES_G2 || note_val > NOTES_B3) {
      // Draw a single line through the dot
      draw_list->AddLine(ImVec2(x - ledger_width / 2, y),
                         ImVec2(x + ledger_width / 2, y), STYLE_GRAY,
                         STYLE_LINE_THICKNESS);
   }
}

static void draw_sharp(ImDrawList *draw_list, ImFont *font, float font_size,
                       float x, float y, float note_radius, uint32_t color)
{
   const char *sharp = "#";
   ImVec2 text_size  = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, sharp);

   draw_list->AddText(
       font, font_size,
       ImVec2(x - note_radius - text_size.x, y - text_size.y / 2), color,
       sharp);
}

static void notes_dot(midi_note n, int x_idx, uint32_t color)
{
   ImVec2 size             = ImGui::GetContentRegionAvail();
   const float note_radius = size.y / 30.0F;
   float font_size         = 3.0F * note_radius;

   float x = ((float)x_idx + 1) * size.x / (MAX_CHORDS + 1.0F);
   float y = static_cast<float>(calc_y(n));

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImFont *font          = ImGui::GetFont();

   if (note_has_sharp(n))
      draw_sharp(draw_list, font, font_size, x, y, note_radius, color);

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

void notes_chords(const std::vector<std::vector<midi_note>> &chords,
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
