// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.h
 * @brief Application state definition.
 * @author Jakob Kastelic
 */

#ifndef STATE_H
#define STATE_H

#include "RtMidi.h"
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#define NOTES_PER_CHORD 10
#define MAX_CHORDS      20

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

struct state {
   // status bar
   std::string status;

   // sheet music display
   std::vector<enum midi_note> bassline;
   std::vector<std::unordered_set<midi_note>> chords_ok;
   std::vector<std::unordered_set<midi_note>> chords_bad;

   // MIDI devices
   std::vector<std::string> midi_devices;
   int selected_device = -1;
   std::unique_ptr<RtMidiIn> midi_in;

   // receiving notes into chords
   std::vector<unsigned char> pressed_notes;
};

template <typename... Args> void state_status(state *s, Args &&...args)
{
   std::ostringstream oss;
   (oss << ... << args);
   s->status = oss.str();
}

#endif /* STATE_H */
