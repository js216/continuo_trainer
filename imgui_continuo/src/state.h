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
   std::string config_file;
   std::string status;
   ImFont *music_font;

   // lesson info
   bool edit_lesson;
   int lesson_id;
   char lesson_title[MAX_STRING];
   enum key_sig key;
   std::vector<struct column> chords;
   unsigned int active_col;

   // stats
   double score;
   double duration_today;

   // MIDI devices and data
   std::vector<std::string> midi_devices;
   int selected_device = -1;
   std::unique_ptr<RtMidiIn> midi_in;
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

#endif /* STATE_H */
