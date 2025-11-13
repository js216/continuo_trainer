/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include <algorithm>
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

void logic_interpret(struct state *state)
{
    static std::vector<unsigned char> chord_buffer; // notes for current chord
    static bool chord_active = false;               // true while keys are pressed

    if (!state->pressed_notes.empty()) {
        // accumulate notes
        for (auto note : state->pressed_notes) {
            if (std::find(chord_buffer.begin(), chord_buffer.end(), note) == chord_buffer.end())
                chord_buffer.push_back(note);
        }

        chord_active = true;

        // copy buffer to current column
        for (int i = 0; i < NOTES_PER_CHORD; ++i) {
            if (i < (int)chord_buffer.size())
                state->chords_ok[i][state->chord_index] =
                    static_cast<enum midi_note>(chord_buffer[i]);
            else
                state->chords_ok[i][state->chord_index] = NOTES_NONE;
        }

        return;
    }

    // all notes released -> prepare next chord
    if (chord_active) {
        chord_active = false;
        chord_buffer.clear();

        // advance column index
        state->chord_index = (state->chord_index + 1) % MAX_CHORDS;

        // clear the next column (only now)
        for (int i = 0; i < NOTES_PER_CHORD; ++i)
            state->chords_ok[i][state->chord_index] = NOTES_NONE;
    }
}

