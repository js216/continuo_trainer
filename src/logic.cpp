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
   state->key = KEY_SIG_2_SHARP;
   state->pressed_notes.clear();
   state->bassline.clear();
   state->chords_ok.clear();
   state->chords_bad.clear();
   state->melody.clear();
}

static enum midi_note get_note(void)
{
   static std::random_device rd;
   static std::mt19937 engine(rd());
   static std::uniform_int_distribution<int> dist(NOTES_E2, NOTES_NUM - 1);

   return static_cast<midi_note>(dist(engine));
}

static std::vector<figure> get_figures(void)
{
   static std::random_device rd;
   static std::mt19937 engine(rd());
   static std::uniform_int_distribution<int> dist_num(1, 8);
   static std::uniform_int_distribution<int> dist_acc(ACC_NONE, ACC_NUM - 1);
   static std::uniform_int_distribution<int> dist_count(1, 3);

   int n_figures = dist_count(engine);
   std::vector<figure> figs;
   figs.reserve(n_figures);

   for (int i = 0; i < n_figures; ++i) {
      figs.push_back(
          figure{dist_num(engine), (enum accidental)dist_acc(engine)});
   }

   return figs;
}

void logic_populate(state *state)
{
   // notes
   state->bassline.clear();
   std::generate_n(std::back_inserter(state->bassline), MAX_CHORDS, get_note);

   // figures
   state->figures.clear();
   std::generate_n(std::back_inserter(state->figures), MAX_CHORDS, get_figures);
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

   if (state->melody.size() < MAX_CHORDS) {
      state->melody.emplace_back();
      state->melody.back().insert(get_note());
   }
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
   if (state->chords_ok.size() != state->chords_bad.size())
      ERROR("set size mismatch");

   if (state->chords_ok.empty()) {
      state->chords_ok.emplace_back();
      state->chords_bad.emplace_back();
   }

   size_t back_idx = state->chords_ok.size() - 1;

   if (state->chords_ok[back_idx].contains(realization) ||
       state->chords_bad[back_idx].contains(realization))
      return;

   if (back_idx < state->bassline.size()) {
      enum midi_note bass_note        = state->bassline[back_idx];
      const std::vector<figure> &figs = state->figures[back_idx];
      if (logic_adjudicate(bass_note, figs, realization))
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
