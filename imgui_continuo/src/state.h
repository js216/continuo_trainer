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

struct column {
   std::unordered_set<midi_note> bass;
   std::vector<struct figure> figures;
   std::unordered_set<midi_note> answer;
   std::unordered_set<midi_note> good;
   std::unordered_set<midi_note> bad;
   double time;
};

struct state {
   // program general
   std::string status;
   ImFont *music_font;
   bool midi_menu_open;
   bool midi_forward;

   // lesson info
   bool edit_lesson;
   int lesson_id;
   char lesson_title[MAX_STRING];
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
   int in_dev = -1;
   int out_dev = -1;
   int selected_device_index = -1;
   std::unique_ptr<RtMidiIn> midi_in;
   std::unique_ptr<RtMidiOut> midi_out;
   std::vector<unsigned char> pressed_notes;
};

// global state for debug only
extern float global_tune;

void state_load(struct state *state);
void state_save(const struct state *state);
void state_clear_lesson(struct state *state);
std::string state_lesson_fname(const int id);
void state_read_lesson(struct state *state);
void state_write_lesson(struct state *state);
int state_load_last_lesson_id(const char *fname);
void parse_figures_token(const std::string &token,
                                std::vector<figure> &figures);

#endif /* STATE_H */
