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
#include <string>
#include <vector>

#define NOTES_PER_CHORD 10
#define MAX_CHORDS      20

enum midi_note {
   NOTES_NONE = 0,
   NOTES_E2   = 40,
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
   NOTES_NUM
};

struct state {
   char status[256];

   std::vector<enum midi_note> bassline;
   std::vector<std::vector<midi_note>> chords_ok;
   std::vector<std::vector<midi_note>> chords_bad;

   std::vector<std::string> midi_devices;
   int selected_device = -1;
   std::unique_ptr<RtMidiIn> midi_in;

   std::vector<unsigned char> pressed_notes;
   bool all_released = true;
   int chord_index;
};

#endif /* STATE_H */
