/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include <stddef.h>
#include <stdlib.h>

void logic_clear(enum midi_note *const n_arr, const size_t count)
{
   for (size_t i = 0; i < count; i++) {
      n_arr[i] = NOTES_NONE;
   }
}

void logic_pop(enum midi_note *const n_arr, const size_t count)
{
   for (size_t i = 0; i < count; i++)
      n_arr[i] = (enum midi_note)(NOTES_E2 + i);
}

void logic_one(enum midi_note ch_arr[NOTES_PER_CHORD][MAX_CHORDS])
{
   for (size_t i = 0; i < MAX_CHORDS; i++) {
      if (ch_arr[0][i] == NOTES_NONE) {
         ch_arr[0][i] = (enum midi_note)(rand() % NOTES_NUM);
         break;
      }
   }
}
