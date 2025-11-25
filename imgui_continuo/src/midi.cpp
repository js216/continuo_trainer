// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file midi.cpp
 * @brief Handling MIDI devices.
 * @author Jakob Kastelic
 */

#include "RtMidi.h"
#include "state.h"
#include "theory.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

static int midi_to_idx(const std::vector<std::string> &list,
                       const std::string &dev_name)
{
   if (dev_name.empty())
      return -1;

   for (int i = 0; i < (int)list.size(); i++) {
      if (list[i] == dev_name)
         return i;
   }
   return -1;
}

void init_midi_in(struct state *state)
{
   const int idx = midi_to_idx(state->midi_devices, state->in_dev);

   if (idx >= 0) {
      try {
         state->midi_in.reset();
         state->midi_in = std::make_unique<RtMidiIn>();
         state->midi_in->openPort(idx);
         state->midi_in->ignoreTypes(false, false, false);
         state->status = "MIDI input opened";
      } catch (RtMidiError &error) {
         state->status =
             std::string("RtMidi input error: ") + error.getMessage();
         state->midi_in.reset();
      }
   } else {
      state->status = "No MIDI input device selected";
   }
}

void init_midi_out(struct state *state)
{
   const int idx = midi_to_idx(state->midi_devices, state->out_dev);

   if (idx >= 0) {
      try {
         state->midi_out.reset();
         state->midi_out = std::make_unique<RtMidiOut>();
         state->midi_out->openPort(idx);
         state->status = "MIDI output opened";
      } catch (RtMidiError &error) {
         state->status =
             std::string("RtMidi output error: ") + error.getMessage();
         state->midi_out.reset();
      }
   } else {
      state->status = "No MIDI output device selected";
   }
}

void deinit_midi_in(struct state *state)
{
   if (state->midi_in) {
      state->midi_in.reset();
      state->in_dev.clear(); // instead of -1
      state->status = "MIDI input disconnected";
   } else {
      state->status = "No MIDI input connected";
   }
}

void deinit_midi_out(struct state *state)
{
   if (state->midi_out) {
      state->midi_out.reset();
      state->out_dev.clear(); // instead of -1
      state->status = "MIDI output disconnected";
   } else {
      state->status = "No MIDI output connected";
   }
}

void test_midi_out(struct state *state)
{
   if (!state->midi_out)
      return; // no output connected

   unsigned char note     = NOTES_Cs4;
   unsigned char velocity = 100;

   // Note On message: 0x90 = channel 1
   std::vector<unsigned char> msg_on = {0x90, note, velocity};
   try {
      state->midi_out->sendMessage(&msg_on);
   } catch (RtMidiError &error) {
      state->status =
          std::string("MIDI test error (Note On): ") + error.getMessage();
      return;
   }

   // Wait a bit
   std::this_thread::sleep_for(std::chrono::milliseconds(250));

   // Note Off message: 0x80 = channel 1
   std::vector<unsigned char> msg_off = {0x80, note, 0};
   try {
      state->midi_out->sendMessage(&msg_off);
   } catch (RtMidiError &error) {
      state->status =
          std::string("MIDI test error (Note Off): ") + error.getMessage();
      return;
   }

   state->status = "MIDI test sent: C4";
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
      state->status = "All notes released";
   } else {
      std::string s = "Pressed: ";
      for (auto n : state->pressed_notes)
         s += std::to_string(n) + " ";
      state->status = s;
   }
}

void poll_midi(struct state *state)
{
   if (!state->midi_in)
      return;

   std::vector<unsigned char> message;
   bool changed = false;

   while (state->midi_in->getMessage(&message) != 0.0) {
      if (message.empty())
         continue;

      unsigned char status   = message[0] & 0xF0U;
      unsigned char note     = message[1];
      unsigned char velocity = message[2];

      // Track pressed/released notes
      if (status == 0x90 && velocity > 0) {
         add_pressed_note(state, note);
         changed = true;
      } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
         remove_pressed_note(state, note);
         changed = true;
      }

      // Forward message to output immediately if enabled
      if (state->midi_forward && state->midi_out &&
          state->in_dev != state->out_dev) // now string compare
      {
         try {
            state->midi_out->sendMessage(&message);
         } catch (RtMidiError &error) {
            state->status =
                std::string("MIDI forward error: ") + error.getMessage();
         }
      }
   }

   if (changed)
      update_status(state);
}
