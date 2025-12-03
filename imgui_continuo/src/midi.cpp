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
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

void refresh_midi_devices(struct state *state)
{
   state->midi.midi_devices.clear();
   try {
      RtMidiIn midi_in;
      const unsigned int n_ports = midi_in.getPortCount();

      for (unsigned int i = 0; i < n_ports; ++i) {
         state->midi.midi_devices.push_back(midi_in.getPortName(i));
      }

      if (state->midi.midi_devices.empty()) {
         state->midi.midi_devices.emplace_back("(no MIDI devices)");
      }
   } catch (RtMidiError &error) {
      state->midi.midi_devices.clear();
      state->midi.midi_devices.push_back(std::string("RtMidi error: ") +
                                         error.getMessage());
   }
}

static int midi_to_idx(const std::vector<std::string> &list,
                       const std::string &dev_name)
{
   if (dev_name.empty())
      return -1;

   for (std::size_t i = 0; i < list.size(); i++) {
      if (list[i] == dev_name)
         return static_cast<int>(i);
   }

   return -1;
}

void init_midi_in(struct state *state)
{
   const int idx =
       midi_to_idx(state->midi.midi_devices, state->settings.in_dev);

   if (idx >= 0) {
      try {
         state->midi.midi_in.reset();
         state->midi.midi_in = std::make_unique<RtMidiIn>();
         state->midi.midi_in->openPort(idx);
         state->midi.midi_in->ignoreTypes(false, false, false);
         state->ui.status = "MIDI input opened";
      } catch (RtMidiError &error) {
         state->ui.status =
             std::string("RtMidi input error: ") + error.getMessage();
         state->midi.midi_in.reset();
      }
   } else {
      state->ui.status = "No MIDI input device selected";
   }
}

void init_midi_out(struct state *state)
{
   const int idx =
       midi_to_idx(state->midi.midi_devices, state->settings.out_dev);

   if (idx >= 0) {
      try {
         state->midi.midi_out.reset();
         state->midi.midi_out = std::make_unique<RtMidiOut>();
         state->midi.midi_out->openPort(idx);
         state->ui.status = "MIDI output opened";
      } catch (RtMidiError &error) {
         state->ui.status =
             std::string("RtMidi output error: ") + error.getMessage();
         state->midi.midi_out.reset();
      }
   } else {
      state->ui.status = "No MIDI output device selected";
   }
}

void deinit_midi_in(struct state *state)
{
   if (state->midi.midi_in) {
      state->midi.midi_in.reset();
      state->settings.in_dev.clear(); // instead of -1
      state->ui.status = "MIDI input disconnected";
   } else {
      state->ui.status = "No MIDI input connected";
   }
}

void deinit_midi_out(struct state *state)
{
   if (state->midi.midi_out) {
      state->midi.midi_out.reset();
      state->settings.out_dev.clear(); // instead of -1
      state->ui.status = "MIDI output disconnected";
   } else {
      state->ui.status = "No MIDI output connected";
   }
}

void test_midi_out(struct state *state)
{
   if (!state->midi.midi_out)
      return; // no output connected

   const unsigned char note     = NOTES_Cs4;
   const unsigned char velocity = 100;

   // Note On message: 0x90 = channel 1
   const std::vector<unsigned char> msg_on = {0x90, note, velocity};
   try {
      state->midi.midi_out->sendMessage(&msg_on);
   } catch (RtMidiError &error) {
      state->ui.status =
          std::string("MIDI test error (Note On): ") + error.getMessage();
      return;
   }

   // Wait a bit
   std::this_thread::sleep_for(std::chrono::milliseconds(250));

   // Note Off message: 0x80 = channel 1
   const std::vector<unsigned char> msg_off = {0x80, note, 0};
   try {
      state->midi.midi_out->sendMessage(&msg_off);
   } catch (RtMidiError &error) {
      state->ui.status =
          std::string("MIDI test error (Note Off): ") + error.getMessage();
      return;
   }

   state->ui.status = "MIDI test sent: C4";
}

void add_pressed_note(struct state *state, unsigned char note)
{
   auto &notes = state->midi.pressed_notes;
   if (std::ranges::find(notes, note) == notes.end())
      notes.push_back(note);
}

void remove_pressed_note(struct state *state, unsigned char note)
{
   auto &notes = state->midi.pressed_notes;
   auto it     = std::ranges::find(notes, note);
   if (it != notes.end())
      notes.erase(it);
}

void update_status(struct state *state)
{
   if (state->midi.pressed_notes.empty()) {
      state->ui.status = "All notes released";
   } else {
      std::string s = "Pressed: ";
      for (auto n : state->midi.pressed_notes)
         s += std::to_string(n) + " ";
      state->ui.status = s;
   }
}

void poll_midi(struct state *state)
{
   if (!state->midi.midi_in)
      return;

   std::vector<unsigned char> message;
   bool changed = false;

   while (state->midi.midi_in->getMessage(&message) != 0.0) {
      if (message.empty())
         continue;

      const unsigned char status   = message[0] & 0xF0U;
      const unsigned char note     = message[1];
      const unsigned char velocity = message[2];

      // Track pressed/released notes
      if (status == 0x90 && velocity > 0) {
         add_pressed_note(state, note);
         changed = true;
      } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
         remove_pressed_note(state, note);
         changed = true;
      }

      // Forward message to output immediately if enabled
      if (state->settings.midi_forward && state->midi.midi_out &&
          state->settings.in_dev !=
              state->settings.out_dev) // now string compare
      {
         try {
            state->midi.midi_out->sendMessage(&message);
         } catch (RtMidiError &error) {
            state->ui.status =
                std::string("MIDI forward error: ") + error.getMessage();
         }
      }
   }

   if (changed)
      update_status(state);
}
