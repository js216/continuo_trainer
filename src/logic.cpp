// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "state.h"
#include "util.h"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <random>
#include <stddef.h>
#include <stdlib.h>

static midi_note get_note(void)
{
   constexpr int min = NOTES_E2;
   constexpr int max = NOTES_NUM - 1;

   static std::random_device rd;
   static std::mt19937 engine(rd());
   static std::uniform_int_distribution<int> dist(min, max);

   return static_cast<midi_note>(dist(engine));
}

void logic_clear(struct state *state)
{
   state->bassline.clear();

   state->chords_ok.clear();
   state->chords_ok.resize(MAX_CHORDS);

   state->chords_bad.clear();
   state->chords_bad.resize(MAX_CHORDS);
}

void logic_pop(state *state)
{
   state->bassline.clear();
   std::generate_n(std::back_inserter(state->bassline), MAX_CHORDS, get_note);
}

void logic_good(state *state)
{
   for (auto &row : state->chords_ok) {
      if (row.size() < MAX_CHORDS) {
         row.push_back(get_note());
         return;
      }
   }
}

void logic_bad(state *state)
{
   if (state->chords_bad.empty())
      ERROR("chords_bad too small");

   auto &row = state->chords_bad[0]; // first row only
   if (row.size() < MAX_CHORDS)
      row.push_back(get_note());
}

void logic_interpret(struct state *state)
{
   static std::vector<unsigned char> chord_buffer; // notes for current chord
   static bool chord_active = false; // true while keys are pressed

   if (!state->pressed_notes.empty()) {
      // accumulate notes
      for (auto note : state->pressed_notes) {
         if (std::find(chord_buffer.begin(), chord_buffer.end(), note) ==
             chord_buffer.end())
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
      for (auto &col : state->chords_ok)
         col[state->chord_index] = NOTES_NONE;
   }
}
