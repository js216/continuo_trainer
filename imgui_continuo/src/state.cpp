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
#include "time_utils.h"
#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
   // if it's a new lesson, we have to add it to the cache
   if (!db_lesson_exists(state->lesson.lesson_id))
      create_lesson_meta(state->stats, state->lesson.lesson_id,
                         state->lesson.chords.size());

   db_clear_lesson_file(state->lesson.lesson_id);
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

   // 1. Reset Global Stats
   // We are rebuilding from scratch, so we must zero out today's accumulators.
   state->stats.score_today        = 0.0;
   state->stats.duration_today     = 0.0;
   state->stats.practice_streak    = 0;
   state->stats.last_practice_date = 0;
   state->stats.goal_met_today     = false;
   state->stats.has_last_record    = false;

   // 2. Initialize Lesson Cache
   state->stats.lesson_cache.clear();
   std::vector<int> lessons = db_get_lesson_ids();
   for (auto lesson_id : lessons) {
      std::vector<column> chords = db_load_lesson_chords(lesson_id);
      create_lesson_meta(state->stats, lesson_id, chords.size());
   }

   // 3. Read History
   std::vector<attempt_record> records = db_read_attempts();
   if (records.empty())
      return;

   // 4. Stream Process
   // Replay every record through the calculator to build the current state.
   for (const auto &r : records) {
      // Order is generally flexible, but calc_schedule handles the logic
      // for "finishing" the previous attempt, so it's good to call it
      // alongside the metric updaters.

      calc_duration(state->stats, r);
      calc_speed(state->stats, r);
      calc_score(state->stats, r);
      calc_practice_streak(state->stats, r, state->settings.score_goal);
      calc_schedule(state->stats, r);
   }
}

void state_stream_in(struct state *state, struct column &col)
{
   double t = time_now();
   db_store_attempt(state->lesson.lesson_id, state->ui.active_col, col, t);

   col.missed = th_get_missed(col.answer, col.good);

   struct attempt_record r = {
       .lesson_id    = state->lesson.lesson_id,
       .col_id       = state->ui.active_col,
       .time         = t,
       .good_count   = col.good.size(),
       .bad_count    = col.bad.size(),
       .missed_count = col.missed.size(),
   };

   calc_duration(state->stats, r);
   calc_speed(state->stats, r);
   calc_score(state->stats, r);
   calc_practice_streak(state->stats, r, state->settings.score_goal);
   calc_schedule(state->stats, r);
}

void state_choose_next(struct state *state)
{
   // In the new SRS system, the "next" lesson is determined by
   // the data now stored in state->stats.lesson_cache.

   // We assume state_reload_stats() has been called at startup
   // or after the last lesson to ensure state->stats is up to date.

   const std::vector<int> lesson_ids = db_get_lesson_ids();

   // Select the lesson with the lowest 'due' timestamp (or fallback logic)
   state->lesson.lesson_id = calc_next(lesson_ids, state->stats);
}
