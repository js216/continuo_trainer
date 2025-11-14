// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file midi.cpp
 * @brief Handling MIDI devices.
 * @author Jakob Kastelic
 */

#include "RtMidi.h"
#include "state.h"
#include <algorithm>
#include <cstring>
#include <string>

void refresh_midi_devices(struct state *state)
{
   state->midi_devices.clear();
   try {
      RtMidiIn midi_in;
      unsigned int n_ports = midi_in.getPortCount();

      for (unsigned int i = 0; i < n_ports; ++i) {
         state->midi_devices.push_back(midi_in.getPortName(i));
      }

      if (state->midi_devices.empty()) {
         state->midi_devices.emplace_back("(no MIDI devices)");
      }
   } catch (RtMidiError &error) {
      state->midi_devices.clear();
      state->midi_devices.push_back(std::string("RtMidi error: ") +
                                    error.getMessage());
   }
}

void init_midi(struct state *state)
{
   if (state->selected_device < 0 ||
       state->selected_device >= (int)state->midi_devices.size()) {
      state_status(state, "No MIDI device selected");
      return;
   }

   try {
      // Close previous MIDI input if any
      state->midi_in.reset();

      // Create new RtMidiIn and open the selected port
      state->midi_in = std::make_unique<RtMidiIn>();
      state->midi_in->openPort(state->selected_device);
      state->midi_in->ignoreTypes(false, false, false);

      state_status(state, "MIDI input opened");
   } catch (RtMidiError &error) {
      state_status(state, "RtMidi error: ", error.getMessage().c_str());
      state->midi_in.reset(); // ensure it's null
   }
}

void deinit_midi(struct state *state)
{
   if (state->midi_in) {
      // Closing the port is automatic on RtMidiIn destructor
      state->midi_in.reset(); // releases the unique_ptr

      state_status(state, "MIDI input disconnected");
   } else {
      state_status(state, "No MIDI device connected");
   }
}

void add_pressed_note(struct state *state, unsigned char note)
{
   auto &notes = state->pressed_notes;
   if (std::find(notes.begin(), notes.end(), note) == notes.end())
      notes.push_back(note);
}

void remove_pressed_note(struct state *state, unsigned char note)
{
   auto &notes = state->pressed_notes;
   auto it     = std::find(notes.begin(), notes.end(), note);
   if (it != notes.end())
      notes.erase(it);
}

void update_status(struct state *state)
{
   if (state->pressed_notes.empty()) {
      state_status(state, "All notes released");
   } else {
      std::string s = "Pressed: ";
      for (auto n : state->pressed_notes)
         s += std::to_string(n) + " ";
      state_status(state, s);
   }
}

void poll_midi(struct state *state)
{
   if (!state->midi_in)
      return;

   std::vector<unsigned char> message;
   bool changed = false;

   while (state->midi_in->getMessage(&message) != 0.0) {
      if (message.size() < 3)
         continue;

      unsigned char status   = message[0] & 0xF0U;
      unsigned char note     = message[1];
      unsigned char velocity = message[2];

      if (status == 0x90 && velocity > 0) {
         add_pressed_note(state, note);
         changed = true;
      } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
         remove_pressed_note(state, note);
         changed = true;
      }
   }

   if (changed)
      update_status(state);
}
