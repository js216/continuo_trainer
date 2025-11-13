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
   state->chord_buffer.clear();
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
      state->chords_ok.back().push_back(get_note());
      return;
   }

   auto it = std::min_element(
       state->chords_ok.begin(), state->chords_ok.end(),
       [](const auto &a, const auto &b) { return a.size() < b.size(); });

   if (it != state->chords_ok.end()) {
      it->push_back(get_note());
   }
}

void logic_bad(struct state *state)
{
   if (state->chords_bad.size() < MAX_CHORDS) {
      state->chords_bad.emplace_back();
      state->chords_bad.back().push_back(get_note());
   }
}

void logic_interpret(struct state *state)
{
   if (!state->pressed_notes.empty()) {
      for (auto note : state->pressed_notes)
         state->chord_buffer.insert(static_cast<midi_note>(note));

      if (state->chords_ok.empty())
         state->chords_ok.emplace_back();

      state->chords_ok.back().assign(state->chord_buffer.begin(),
                                     state->chord_buffer.end());
   }

   else if (!state->chord_buffer.empty()) {
      state->chord_buffer.clear();

      if (state->chords_ok.size() >= MAX_CHORDS)
         state->chords_ok.clear();
   }
}
