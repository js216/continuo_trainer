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
#include <vector>

void logic_clear(struct state *state)
{
   state->key = KEY_SIG_1_FLAT;
   state->pressed_notes.clear();
   state->chords.clear();
   state->active_col = 0;

   state_read_lesson("lessons/l1.txt", state);
}

static bool logic_adjudicate(const struct column &col,
      enum midi_note realization)
{
   // If no bass exists yet, automatically bad
   if (col.bass.empty())
      return false;

   int realized_pc = static_cast<int>(realization) % 12;

   for (auto ans_note : col.answer)
   {
      int expected_pc = static_cast<int>(ans_note) % 12;
      if (realized_pc == expected_pc)
         return true;
   }

   return false;
}

static void process_note(struct state *state, midi_note realization)
{
   if (state->chords.empty())
      state->chords.emplace_back();

   if (state->active_col >= state->chords.size()) {
      ERROR("active_col out of range!");
      return;
   }

   column &col = state->chords[state->active_col];

   if (col.good.contains(realization) || col.bad.contains(realization))
      return;

   if (logic_adjudicate(col, realization))
      col.good.insert(realization);
   else
      col.bad.insert(realization);
}


static std::string midi_to_name(int midi)
{
   static const char *names[] = { "C", "C#", "D", "D#", "E",
      "F", "F#", "G", "G#", "A", "A#", "B" };
   int octave = (midi / 12) - 1;      // MIDI 0 = C-1
   int note   = midi % 12;
   return std::string(names[note]) + std::to_string(octave);
}

static void print_chord(const std::unordered_set<midi_note> &good,
      const std::unordered_set<midi_note> &bad)
{
   std::vector<midi_note> all_notes;
   all_notes.insert(all_notes.end(), good.begin(), good.end());
   all_notes.insert(all_notes.end(), bad.begin(), bad.end());

   if (all_notes.empty())
      return;

   std::sort(all_notes.begin(), all_notes.end());
   midi_note lowest = all_notes.front();

   // print lowest note first
   std::cout << midi_to_name(lowest) << " ";

   // print remaining notes comma-separated
   bool first = true;
   for (auto n : all_notes) {
      if (n == lowest) continue;
      if (!first) std::cout << ",";
      std::cout << midi_to_name(n);
      first = false;
   }
   std::cout << "\n";
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
      // all notes released → go to next column
      if (!state->chords.empty()) {
         column &col = state->chords[state->active_col];
         if (!col.good.empty() || !col.bad.empty()) {
            print_chord(col.good, col.bad);
            state->active_col++;
         }
      }

      // clear everything if MAX_CHORDS reached
      if (state->chords.size() >= MAX_CHORDS)
         state->chords.clear();
   }
}

