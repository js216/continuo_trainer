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

void state_save_settings(const struct state *state)
{
   db_store_key_val("in_dev", state->settings.in_dev);
   db_store_key_val("out_dev", state->settings.out_dev);
   db_store_bool("midi_forward", state->settings.midi_forward);
}

void state_load_settings(struct state *state)
{
   state->settings.in_dev       = db_load_key_val("in_dev");
   state->settings.out_dev      = db_load_key_val("out_dev");
   state->settings.midi_forward = db_load_bool("midi_forward");
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
   if (state->lesson.chords.empty())
      return;

   state->lesson.chords.pop_back();
   state->ui.active_col--;
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
   state->stats.score          = calc_score_today(records);
   state->stats.lesson_streak  = calc_lesson_streak(
       records, state->lesson.lesson_id, state->lesson.chords.size());
   state->stats.practice_streak = calc_practice_streak(records);
}
