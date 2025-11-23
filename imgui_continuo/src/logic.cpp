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
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <random>

void logic_clear(struct state *state)
{
   state->key = KEY_SIG_1_FLAT;
   state->pressed_notes.clear();
   state->chords.clear();

   read_bassline_from_file("lessons/l1.txt", state->chords);
}

void logic_inc(struct state *state)
{
   state->key = static_cast<enum key_sig>(static_cast<int>(state->key) + 1);
}

void logic_dec(struct state *state)
{
   state->key = static_cast<enum key_sig>(static_cast<int>(state->key) - 1);
}

static bool logic_adjudicate(enum midi_note bass_note,
                             const std::vector<figure> &figures,
                             enum midi_note realization)
{
   int realized_pc = (int)realization % 12;

   for (const auto &fig : figures) {
      int interval = 0;
      switch (fig.num) {
         case 3: interval = 4; break;
         case 5: interval = 7; break;
         case 6: interval = 9; break;
         case 7: interval = 10; break;
         case 8: interval = 12; break;
         default: continue;
      }

      if (fig.acc == ACC_SHARP) {
         interval += 1;
      } else if (fig.acc == ACC_FLAT) {
         interval -= 1;
      }

      int expected_pc = ((int)bass_note + interval + 12) % 12;
      if (realized_pc == expected_pc)
         return true; // correct realization
   }

   return false; // no match found
}

static void process_note(struct state *state, midi_note realization)
{
    if (state->chords.empty())
        state->chords.emplace_back(); // create first column if empty

    column &col = state->chords.back();

    // skip if note already recorded
    if (col.good.contains(realization) || col.bad.contains(realization))
        return;

    if (!col.bass.empty()) {
        // assume one bass note per column
        midi_note bass_note = *col.bass.begin();
        const std::vector<figure> &figs = col.figures;

        if (logic_adjudicate(bass_note, figs, realization))
            col.good.insert(realization);
        else
            col.bad.insert(realization);
    } else {
        // no bass yet, consider as bad
        col.bad.insert(realization);
    }
}

void logic_receive(struct state *state)
{
    if (!state->pressed_notes.empty()) {
        // accumulate pressed notes into the current back column
        if (state->chords.empty())
            state->chords.emplace_back();

        for (auto note_val : state->pressed_notes)
            process_note(state, static_cast<midi_note>(note_val));

    } else {
        // all notes released → start a new column if the last has data
        if (!state->chords.empty()) {
            column &col = state->chords.back();
            if (!col.good.empty() || !col.bad.empty())
                state->chords.emplace_back();
        }

        // clear everything if MAX_CHORDS reached
        if (state->chords.size() >= MAX_CHORDS)
            state->chords.clear();
    }
}

