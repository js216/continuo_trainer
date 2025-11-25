// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "db.h"
#include "state.h"
#include "time_utils.h"
#include "util.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

void logic_clear(struct state *state)
{
   if (state->lesson_id <= 0)
      state->lesson_id = 1;

   if (db_lesson_exists(state->lesson_id)) {
      // load lesson
      state->edit_lesson = false;
      state_load_lesson(state);
      state->status = "Loaded lesson " + std::to_string(state->lesson_id);
   } else {
      // enter edit mode
      state_clear_lesson(state);
      state->edit_lesson = true;
   }

   state_reload_stats(state);
}

static bool logic_adjudicate(const struct column &col,
                             enum midi_note realization)
{
   if (col.bass.empty())
      return false;

   int realized_pc = static_cast<int>(realization) % 12;

   auto matches_pc = [realized_pc](auto n) {
      return realized_pc == (int(n) % 12);
   };

   return std::ranges::any_of(col.answer, matches_pc);
}

static void process_note(struct state *state, midi_note realization)
{
   if (state->chords.empty())
      state->chords.emplace_back();

   if (state->active_col >= state->chords.size()) {
      ERROR("active_col out of range!");
      return;
   }

   struct column &col = state->chords[state->active_col];

   if (col.good.contains(realization) || col.bad.contains(realization))
      return;

   if (logic_adjudicate(col, realization))
      col.good.insert(realization);
   else
      col.bad.insert(realization);
}

static void logic_play(struct state *state)
{
   // cannot play without chords
   if (state->chords.empty())
      return;

   // lesson finished
   if (state->active_col >= state->chords.size()) {
      logic_clear(state);
      state->status = "Done!";
      return;
   }

   // all notes released : go to next column
   if (state->pressed_notes.empty()) {
      struct column &col = state->chords[state->active_col];

      if (!col.good.empty() || !col.bad.empty()) {
         col.time = time_now();
         db_store_attempt(state->lesson_id, col);
         state->active_col++;
      }
   }

   else {
      // accumulate pressed notes into the current back column
      if (state->chords.empty())
         state->chords.emplace_back();

      for (auto note_val : state->pressed_notes)
         process_note(state, static_cast<midi_note>(note_val));
   }
}

static void logic_record(struct state *state)
{
   // If nothing pressed → finalize column if anything was recorded
   if (state->pressed_notes.empty()) {
      if (state->active_col < state->chords.size()) {
         struct column &col = state->chords[state->active_col];
         if (!col.bass.empty() || !col.answer.empty()) {
            // Finalize this column, move to the next
            state->active_col++;
         }
      }
      return;
   }

   // Notes currently pressed → ensure there is a column to record into
   if (state->chords.empty() || state->active_col >= state->chords.size()) {
      state->chords.emplace_back();
   }

   struct column &col = state->chords[state->active_col];

   // Determine lowest pressed note
   unsigned char lowest = *std::min_element(state->pressed_notes.begin(),
                                            state->pressed_notes.end());

   // Insert into bass + answer sets
   col.bass.insert(static_cast<midi_note>(lowest));
   for (auto n : state->pressed_notes) {
      if (n != lowest)
         col.answer.insert(static_cast<midi_note>(n));
   }
}

void logic_receive(struct state *state)
{
   if (state->edit_lesson)
      logic_record(state);
   else
      logic_play(state);
}
