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

struct state {
   // program general
   std::string status;
   ImFont *music_font;
   bool settings_open;
   std::string figs_entry;

   // settings
   bool midi_forward;
   std::string in_dev;
   std::string out_dev;

   // lesson info
   bool edit_lesson;
   int lesson_id;
   std::string lesson_title;
   enum key_sig key;
   std::vector<struct column> chords;
   unsigned int active_col;

   // stats
   double score;
   double duration_today;
   int practice_streak;
   int lesson_streak;

   // MIDI devices and data
   std::vector<std::string> midi_devices;
   std::string selected_device;
   std::unique_ptr<RtMidiIn> midi_in;
   std::unique_ptr<RtMidiOut> midi_out;
   std::vector<unsigned char> pressed_notes;
};

// global state for debug only
extern float global_tune;

void state_save_settings(const struct state *state);
void state_load_settings(struct state *state);

void state_clear_lesson(struct state *state);
void state_pop_lesson(struct state *state);
void state_load_lesson(struct state *state);
void state_store_lesson(struct state *state);

// stats
void state_reload_stats(struct state *state);

#endif /* STATE_H */
