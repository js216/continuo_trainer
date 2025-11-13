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

/**
 * @brief Draw ledger lines through a note outside the staff.
 *
 * @param draw_list ImGui draw list
 * @param x X position of the note
 * @param y Y position of the note
 * @param note_val MIDI note value
 * @param color Color to draw
 * @param note_radius Radius of the note circle
 */
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

/**
 * @brief Draw a sharp symbol next to a note.
 *
 * @param draw_list ImGui draw list
 * @param font ImGui font pointer
 * @param font_size Font size in pixels
 * @param x X position of the note
 * @param y Y position of the note
 * @param note_radius Radius of the note circle
 * @param color Color of the sharp
 */
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

void notes_dots(const std::vector<midi_note> &notes, uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();
   ImVec2 size           = ImGui::GetContentRegionAvail();

   const float note_radius = size.y / 30.0F;
   const float x_space =
       size.x / (MAX_CHORDS + 1.0F); // reserve slots for max possible notes

   ImFont *font    = ImGui::GetFont();
   float font_size = 3.0F * note_radius;

   for (size_t i = 0; i < notes.size(); ++i) {
      float x = p.x + ((float)i + 1) * x_space;
      float y = static_cast<float>(calc_y(notes[i]));

      if (note_has_sharp(notes[i]))
         draw_sharp(draw_list, font, font_size, x, y, note_radius, color);

      draw_ledger_lines(draw_list, x, y, static_cast<int>(notes[i]),
                        note_radius);
      draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);
   }

   ImGui::EndChild();
}

void notes_chords(const std::vector<std::vector<midi_note>> &ch_arr,
                  uint32_t color)
{
   for (const auto &line : ch_arr) {
      notes_dots(line, color);
   }
}
