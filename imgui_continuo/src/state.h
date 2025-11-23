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
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#define NOTES_PER_CHORD 10
#define MAX_CHORDS      15

enum midi_note {
   NOTES_E2 = 40,
   NOTES_F2,
   NOTES_Fs2,
   NOTES_G2,
   NOTES_Gs2,
   NOTES_A2,
   NOTES_As2,
   NOTES_B2,
   NOTES_C3,
   NOTES_Cs3,
   NOTES_D3,
   NOTES_Ds3,
   NOTES_E3,
   NOTES_F3,
   NOTES_Fs3,
   NOTES_G3,
   NOTES_Gs3,
   NOTES_A3,
   NOTES_As3,
   NOTES_B3,
   NOTES_C4,
   NOTES_Cs4,
   NOTES_D4,
   NOTES_Ds4,
   NOTES_E4,
   NOTES_F4,
   NOTES_Fs4,
   NOTES_G4,
   NOTES_Gs4,
   NOTES_A4,
   NOTES_As4,
   NOTES_B4,
   NOTES_C5,
   NOTES_Cs5,
   NOTES_D5,
   NOTES_Ds5,
   NOTES_E5,
   NOTES_F5,
   NOTES_Fs5,
   NOTES_G5,
   NOTES_Gs5,
   NOTES_A5,
   NOTES_As5,
   NOTES_NUM
};

enum key_sig {
   KEY_SIG_0,
   KEY_SIG_1_SHARP,
   KEY_SIG_2_SHARP,
   KEY_SIG_3_SHARP,
   KEY_SIG_4_SHARP,
   KEY_SIG_5_SHARP,
   KEY_SIG_6_SHARP,
   KEY_SIG_7_SHARP,
   KEY_SIG_1_FLAT,
   KEY_SIG_2_FLAT,
   KEY_SIG_3_FLAT,
   KEY_SIG_4_FLAT,
   KEY_SIG_5_FLAT,
   KEY_SIG_6_FLAT,
   KEY_SIG_7_FLAT,
   KEY_NUM
};

enum accidental { ACC_NONE, ACC_SHARP, ACC_FLAT, ACC_NATURAL, ACC_NUM };

struct figure {
   int num;
   enum accidental acc;
};

struct column {
   std::unordered_set<midi_note> bass;
   std::vector<struct figure> figures;
   std::unordered_set<midi_note> melody;
   std::unordered_set<midi_note> good;
   std::unordered_set<midi_note> bad;
};

struct state {
   // program general
   std::string config_file;
   std::string status;
   ImFont *music_font;

   // sheet music display
   enum key_sig key;
   std::vector<struct column> chords;

   // MIDI devices and data
   std::vector<std::string> midi_devices;
   int selected_device = -1;
   std::unique_ptr<RtMidiIn> midi_in;
   std::vector<unsigned char> pressed_notes;
};

template <typename... Args> void state_status(state *state, Args &&...args)
{
   std::ostringstream oss;
   (oss << ... << args);
   state->status = oss.str();
}

void state_load(struct state *state);

void state_save(const struct state *state);

void read_bassline_from_file(const std::string &filename,
                             std::vector<column> &chords);

#endif /* STATE_H */
