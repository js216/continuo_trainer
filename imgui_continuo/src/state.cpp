// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Application state manipulation.
 * @author Jakob Kastelic
 */

#include "state.h"
#include "calc.h"
#include "db.h"
#include "theory.h"
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

// global state for debug only
float global_tune;

void state_save_settings(const struct settings &set)
{
   db_store_key_val("in_dev", set.in_dev);
   db_store_key_val("out_dev", set.out_dev);
   db_store_bool("midi_forward", set.midi_forward);
   db_store_int("score_goal", set.score_goal);
}

void state_load_settings(struct settings &set)
{
   set.in_dev       = db_load_key_val("in_dev");
   set.out_dev      = db_load_key_val("out_dev");
   set.midi_forward = db_load_bool("midi_forward");
   set.score_goal   = db_load_int("score_goal");
}

void state_clear_lesson(struct state *state)
{
   std::fill(std::begin(state->lesson.lesson_title),
             std::end(state->lesson.lesson_title), '\0');
   state->lesson.key = KEY_SIG_0;
   state->lesson.chords.clear();
   state->midi.pressed_notes.clear();
   state->ui.active_col = 0;
}

void state_pop_lesson(struct state *state)
{
   if (!state->ui.edit_lesson)
      return;

   if (state->lesson.chords.empty() ||
       state->ui.active_col >= state->lesson.chords.size())
      return;

   // Remove the currently active column
   state->lesson.chords.erase(state->lesson.chords.begin() +
                              state->ui.active_col);

   // Adjust active_col so it stays in bounds
   if (state->ui.active_col >= state->lesson.chords.size()) {
      state->ui.active_col = state->lesson.chords.size() - 1;
   }
}

void state_load_lesson(struct state *state)
{
   state_clear_lesson(state);
   state->lesson.lesson_title =
       db_load_lesson_key_val(state->lesson.lesson_id, "title");
   state->lesson.key =
       th_parse_key(db_load_lesson_key_val(state->lesson.lesson_id, "key"));
   state->lesson.chords = db_load_lesson_chords(state->lesson.lesson_id);

   state->ui.status =
       "Loaded lesson " + std::to_string(state->lesson.lesson_id);
}

void state_store_lesson(struct state *state)
{
   db_clear_lesson(state->lesson.lesson_id);
   db_store_lesson_key_val(state->lesson.lesson_id, "title",
                           state->lesson.lesson_title);
   db_store_lesson_key_val(state->lesson.lesson_id, "key",
                           th_key_sig_to_string(state->lesson.key));
   db_store_lesson_chords(state->lesson.lesson_id, state->lesson.chords);

   state->ui.status =
       "Lesson saved to " + std::to_string(state->lesson.lesson_id);
}

void state_reload_stats(struct state *state)
{
   if (!state)
      return;

   // Clear lesson metadata cache to avoid stale data
   state->stats.lesson_cache.clear();

   // Read all attempts from file
   std::vector<attempt_record> records = db_read_attempts();
   if (records.empty()) {
      state->stats.duration_today  = 0.0;
      state->stats.score           = 0.0;
      state->stats.lesson_streak   = 0;
      state->stats.practice_streak = 0;
      return;
   }

   // Compute score and streaks
   state->stats.duration_today = calc_duration_today(records);
   state->stats.lesson_streak  = calc_lesson_streak(
       records, state->lesson.lesson_id, state->lesson.chords.size());
   state->stats.lesson_speed = calc_speed(records, state->lesson.lesson_id);
   state->stats.score = calc_score_today(records, state->stats.lesson_cache);
   state->stats.practice_streak = calc_practice_streak(
       records, state->settings.score_goal, state->stats.lesson_cache);
   state->stats.difficulty = calc_difficulty(state->lesson.lesson_id, records,
                                             state->stats.lesson_cache);
}

void state_choose_next(struct state *state)
{
   std::vector<attempt_record> records = db_read_attempts();
   if (records.empty())
      return;

   const std::vector<int> lesson_ids = db_get_lesson_ids();

   state->lesson.lesson_id = calc_next(state->lesson.lesson_id, lesson_ids,
                                       records, state->stats.lesson_cache);
}
