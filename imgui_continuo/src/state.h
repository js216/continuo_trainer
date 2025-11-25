// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.h
 * @brief Application state definition.
 * @author Jakob Kastelic
 */

#ifndef STATE_H
#define STATE_H

#include "RtMidi.h"
#include "imgui.h"
#include "theory.h"
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#define MAX_STRING 512

struct state {
   // program general
   std::string status;
   ImFont *music_font;
   bool midi_menu_open;
   bool midi_forward;

   // lesson info
   bool edit_lesson;
   int lesson_id;
   std::string lesson_title;
   enum key_sig key;
   std::vector<struct column> chords;
   unsigned int active_col;
   char figs_entry[MAX_STRING];
   int lesson_streak;

   // stats
   double score;
   double duration_today;
   int practice_streak;

   // MIDI devices and data
   std::vector<std::string> midi_devices;
   std::string in_dev;
   std::string out_dev;
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
void state_load_lesson(struct state *state);
void state_store_lesson(struct state *state);

// stats
void state_reload_stats(struct state *state);

#endif /* STATE_H */
