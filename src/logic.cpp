// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "state.h"
#include "util.h"
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
   state->pressed_notes.clear();
   state->bassline.clear();
   state->chords_ok.clear();
   state->chords_bad.clear();
}

void logic_pop(state *state)
{
   state->bassline.clear();
   std::generate_n(std::back_inserter(state->bassline), MAX_CHORDS, get_note);
}

void logic_good(struct state *state)
{
   if (state->chords_ok.size() < MAX_CHORDS) {
      state->chords_ok.emplace_back();
      state->chords_ok.back().insert(get_note());
      return;
   }

   auto it = std::min_element(
       state->chords_ok.begin(), state->chords_ok.end(),
       [](const auto &a, const auto &b) { return a.size() < b.size(); });

   if (it != state->chords_ok.end()) {
      it->insert(get_note());
   }
}

void logic_bad(struct state *state)
{
   if (state->chords_bad.size() < MAX_CHORDS) {
      state->chords_bad.emplace_back();
      state->chords_bad.back().insert(get_note());
   }
}

bool logic_adjudicate(enum midi_note bass_note, enum midi_note realization)
{
   (void)bass_note;
   (void)realization;

   static std::random_device rd;
   static std::mt19937 gen(rd());
   static std::uniform_int_distribution<int> dist(0, 1);

   return dist(gen) != 0;
}

static void process_note(struct state *state, midi_note realization)
{
   if (state->chords_ok.size() != state->chords_bad.size())
      ERROR("set size mismatch");

   size_t back_idx = state->chords_ok.size() - 1;

   if (back_idx < state->bassline.size()) {
      midi_note bass_note = state->bassline[back_idx];
      if (logic_adjudicate(bass_note, realization))
         state->chords_ok.back().insert(realization);
      else
         state->chords_bad.back().insert(realization);
   } else {
      state->chords_bad.back().insert(realization);
   }
}

void logic_receive(struct state *state)
{
   if (!state->pressed_notes.empty()) {
      // accumulate all pressed notes into the current back column
      for (auto note_val : state->pressed_notes)
         process_note(state, static_cast<midi_note>(note_val));
   }

   else {
      // all notes released → ensure there's an empty column for the next chord
      if (!state->chords_ok.empty() && (!state->chords_ok.back().empty() ||
                                        !state->chords_bad.back().empty())) {
         state->chords_ok.emplace_back();
         state->chords_bad.emplace_back();
      }

      // clear everything if MAX_CHORDS reached
      if (state->chords_ok.size() >= MAX_CHORDS ||
          state->chords_bad.size() >= MAX_CHORDS) {
         state->chords_ok.clear();
         state->chords_bad.clear();
         state->bassline.clear();
      }
   }
}
