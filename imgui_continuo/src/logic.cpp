// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "db.h"
#include "state.h"
#include "theory.h"
#include "time_utils.h"
#include "util.h"
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

void logic_clear(struct state *state)
{
   if (state->lesson.lesson_id <= 0)
      state->lesson.lesson_id = 1;

   if (db_lesson_exists(state->lesson.lesson_id)) {
      // load lesson
      state->ui.edit_lesson = false;
      state_load_lesson(state);
      state->ui.status =
          "Loaded lesson " + std::to_string(state->lesson.lesson_id);
   } else {
      // enter edit mode
      state_clear_lesson(state);
      state->ui.edit_lesson = true;
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

   // replace std::ranges::any_of with manual loop
   for (auto n : col.answer) {
      if (matches_pc(n))
         return true;
   }
   return false;
}

static void process_note(struct state *state, midi_note realization)
{
   if (state->lesson.chords.empty())
      state->lesson.chords.emplace_back();

   if (state->ui.active_col >= state->lesson.chords.size()) {
      error("active_col out of range!");
      return;
   }

   struct column &col = state->lesson.chords[state->ui.active_col];

   // replace unordered_set::contains with find
   if (col.good.find(realization) != col.good.end() ||
       col.bad.find(realization) != col.bad.end())
      return;

   if (logic_adjudicate(col, realization))
      col.good.insert(realization);
   else
      col.bad.insert(realization);
}

static void logic_play(struct state *state)
{
   // cannot play without chords
   if (state->lesson.chords.empty())
      return;

   // lesson finished
   if (state->ui.active_col >= state->lesson.chords.size()) {
      logic_clear(state);
      state->ui.status = "Done!";
      return;
   }

   // all notes released : go to next column
   if (state->midi.pressed_notes.empty()) {
      struct column &col = state->lesson.chords[state->ui.active_col];

      if (!col.good.empty() || !col.bad.empty()) {
         col.time = time_now();
         db_store_attempt(state->lesson.lesson_id, col);
         state->ui.active_col++;
      }
   }

   else {
      // accumulate pressed notes into the current back column
      if (state->lesson.chords.empty())
         state->lesson.chords.emplace_back();

      for (auto note_val : state->midi.pressed_notes)
         process_note(state, static_cast<midi_note>(note_val));
   }
}

static void logic_record(struct state *state)
{
   static bool was_pressed = false;
   bool is_pressed = !state->midi.pressed_notes.empty();

   // --- RELEASE LOGIC (Falling Edge) ---
   if (!is_pressed) {
      if (was_pressed)
         state->ui.figs_entry = "";
      was_pressed = false;
      return;
   }

   // --- PRESS LOGIC (Rising Edge) ---
   if (!was_pressed) {
      if (state->lesson.chords.empty()) {
         // Case 1: Start of lesson. Insert at index 0.
         state->lesson.chords.emplace_back();
         state->ui.active_col = 0;
      } else {
         // Case 2: Insert into the middle or at the end.
         // We insert at active_col + 1 (AFTER the currently selected column).
         size_t insert_idx = state->ui.active_col + 1;

         // Insert the new empty column.
         state->lesson.chords.insert(
            state->lesson.chords.begin() + insert_idx,
            column());

         // Move the active cursor to the newly inserted column.
         state->ui.active_col = insert_idx;
      }
   }

   // Ensure the vector is large enough (safety check).
   if (state->ui.active_col >= state->lesson.chords.size()) {
      state->lesson.chords.emplace_back();
   }

   // --- RECORDING ---
   struct column &col = state->lesson.chords[state->ui.active_col];

   // Determine lowest pressed note
   unsigned char lowest = *std::min_element(state->midi.pressed_notes.begin(),
                                            state->midi.pressed_notes.end());

   // Insert into bass + answer sets
   col.bass.insert(static_cast<midi_note>(lowest));
   for (auto n : state->midi.pressed_notes) {
      if (n != lowest)
         col.answer.insert(static_cast<midi_note>(n));
   }

   was_pressed = true;
}

void logic_receive(struct state *state)
{
   if (state->ui.edit_lesson)
      logic_record(state);
   else
      logic_play(state);
}
