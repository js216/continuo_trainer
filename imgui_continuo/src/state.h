// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.h
 * @brief Application state definition.
 * @author Jakob Kastelic
 */

#ifndef STATE_H
#define STATE_H

#include "RtMidi.h"
#include "theory.h"
#include <memory>
#include <string>
#include <vector>

struct ImFont;

struct app_ui {
   std::string status;
   bool settings_open;
   std::string figs_entry;
   std::string selected_device;
   unsigned int active_col;
   bool edit_lesson;
};

struct settings {
   bool midi_forward;
   std::string in_dev;
   std::string out_dev;
   int goal_minutes;
};

struct lesson {
   int lesson_id;
   std::string lesson_title;
   enum key_sig key;
   std::vector<struct column> chords;
};

struct stats {
   // lesson
   int lesson_streak;
   double avg_max_dt;

   // today
   double score;
   double duration_today;

   // overall
   int practice_streak;
};

struct midi_devices {
   std::vector<std::string> midi_devices;
   std::unique_ptr<RtMidiIn> midi_in;
   std::unique_ptr<RtMidiOut> midi_out;
   std::vector<unsigned char> pressed_notes;
};

struct state {
   struct app_ui ui;
   struct settings settings;
   struct lesson lesson;
   struct stats stats;
   struct midi_devices midi;
   struct ImFont *music_font;
};

// global state for debug only
extern float global_tune;

void state_save_settings(const struct settings &state);
void state_load_settings(struct settings &state);

void state_clear_lesson(struct state *state);
void state_pop_lesson(struct state *state);
void state_load_lesson(struct state *state);
void state_store_lesson(struct state *state);

// stats
void state_reload_stats(struct state *state);

#endif /* STATE_H */
