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
   db_store_key_val("in_dev", state->in_dev);
   db_store_key_val("out_dev", state->out_dev);
   db_store_bool("midi_forward", state->midi_forward);
}

void state_load_settings(struct state *state)
{
   state->in_dev       = db_load_key_val("in_dev");
   state->out_dev      = db_load_key_val("out_dev");
   state->midi_forward = db_load_bool("midi_forward");
}

void state_clear_lesson(struct state *state)
{
   std::fill(std::begin(state->lesson_title), std::end(state->lesson_title),
             '\0');
   state->key = KEY_SIG_0;
   state->chords.clear();
   state->pressed_notes.clear();
   state->active_col = 0;
}

void state_load_lesson(struct state *state)
{
   state_clear_lesson(state);
   state->lesson_title = db_load_lesson_key_val(state->lesson_id, "title");
   state->key    = parse_key(db_load_lesson_key_val(state->lesson_id, "key"));
   state->chords = db_load_lesson_chords(state->lesson_id);

   state->status = "Loaded lesson " + std::to_string(state->lesson_id);
}

void state_store_lesson(struct state *state)
{
   db_clear_lesson(state->lesson_id);
   db_store_lesson_key_val(state->lesson_id, "title", state->lesson_title);
   db_store_lesson_key_val(state->lesson_id, "key",
                           key_sig_to_string(state->key));
   db_store_lesson_chords(state->lesson_id, state->chords);

   state->status = "Lesson saved to " + std::to_string(state->lesson_id);
}

void state_reload_stats(struct state *state)
{
   if (!state)
      return;

   // Read all attempts from file
   std::vector<attempt_record> records = db_read_attempts();
   if (records.empty()) {
      state->duration_today  = 0.0;
      state->score           = 0.0;
      state->lesson_streak   = 0;
      state->practice_streak = 0;
      return;
   }

   // Compute score and streaks
   state->duration_today = calc_duration_today(records);
   state->score          = calc_score_today(records);
   state->lesson_streak =
       calc_lesson_streak(records, state->lesson_id, state->chords.size());
   state->practice_streak = calc_practice_streak(records);
}
