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
      case NOTES_D4:
      case NOTES_Ds4: return 11;
      case NOTES_E4: return 12;
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

   for (size_t i = 0; i < sizeof(staff_lines) / sizeof(staff_lines[0]); i++) {
      float y = calc_y(staff_lines[i]);
      draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + size.x, y), STYLE_WHITE,
                         1.0F);
   }

   ImGui::EndChild();
}

void notes_dots(const enum midi_note *const n_arr, const size_t count,
                const uint32_t color)
{
   if (!ImGui::BeginChild("Staff", ImVec2(0, 0), false)) {
      ImGui::EndChild();
      return;
   }

   ImDrawList *draw_list = ImGui::GetWindowDrawList();
   ImVec2 p              = ImGui::GetCursorScreenPos();
   ImVec2 size           = ImGui::GetContentRegionAvail();

   const float note_radius = size.y / 30.0F;
   const float x_space     = size.x / ((float)count + 1);

   ImFont *font    = ImGui::GetFont();
   float font_size = 3 * note_radius;

   for (size_t i = 0; i < count; i++) {
      if (note_to_bass(n_arr[i]) < -2)
         continue;

      float x = p.x + ((float)i + 1) * x_space;
      float y = (float)calc_y(n_arr[i]);

      draw_list->AddCircleFilled(ImVec2(x, y), note_radius, color);

      if (note_has_sharp(n_arr[i])) {
         const char *sharp = "#";
         ImVec2 text_size =
             font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, sharp);
         draw_list->AddText(
             font, font_size,
             ImVec2(x - note_radius - text_size.x, y - text_size.y / 2), color,
             sharp);
      }
   }

   ImGui::EndChild();
}

void notes_chords(const enum midi_note ch_arr[NOTES_PER_CHORD][MAX_CHORDS],
                  const uint32_t color)
{
   for (size_t i = 0; i < NOTES_PER_CHORD; i++)
      notes_dots(ch_arr[i], MAX_CHORDS, color);
}
