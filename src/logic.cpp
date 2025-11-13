/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "notes.h"

void logic_clear(enum midi_note *const n_arr, size_t count)
{
   for (size_t i = 0; i < count; i++) {
      n_arr[i] = NOTES_NONE;
   }
}

void logic_pop(enum midi_note *const n_arr, size_t count)
{
   for (size_t i = 0; i < count; i++)
      n_arr[i] = (enum midi_note)(NOTES_E2 + i);
}
