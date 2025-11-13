/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "state.h"
#include <stddef.h>
#include <stdlib.h>

static enum midi_note get_note()
{
   int min = NOTES_E2;
   int max = NOTES_NUM - 1;

   enum midi_note n = (enum midi_note)(min + rand() % (max - min + 1));
   return n;
}

void logic_clear(struct state *state)
{
   for (size_t i = 0; i < MAX_CHORDS; i++) {
      state->bassline[i] = NOTES_NONE;
      for (size_t j = 0; j < NOTES_PER_CHORD; j++) {
         state->chords_ok[j][i]  = NOTES_NONE;
         state->chords_bad[j][i] = NOTES_NONE;
      }
   }
}

void logic_pop(struct state *state)
{
   for (size_t i = 0; i < MAX_CHORDS; i++)
      state->bassline[i] = get_note();
}

void logic_good(struct state *state)
{
   for (size_t j = 0; j < NOTES_PER_CHORD; j++) {
      for (size_t i = 0; i < MAX_CHORDS; i++) {
         if (state->chords_ok[j][i] == NOTES_NONE) {
            state->chords_ok[j][i] = get_note();
            return;
         }
      }
   }
}

void logic_bad(struct state *state)
{
   for (size_t i = 0; i < MAX_CHORDS; i++) {
      if (state->chords_bad[0][i] == NOTES_NONE) {
         state->chords_bad[0][i] = get_note();
         break;
      }
   }
}
