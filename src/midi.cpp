/**
 * @file midi.cpp
 * @brief Handling MIDI devices.
 * @author Jakob Kastelic
 */

#include <string>
#include "state.h"
#include "RtMidi.h"

void refresh_midi_devices(struct state *state)
{
    state->midi_devices.clear();
    try {
        RtMidiIn midiIn;
        unsigned int nPorts = midiIn.getPortCount();

        for (unsigned int i = 0; i < nPorts; ++i) {
            state->midi_devices.push_back(midiIn.getPortName(i));
        }

        if (state->midi_devices.empty()) {
            state->midi_devices.push_back("(no MIDI devices)");
        }
    }
    catch (RtMidiError &error) {
        state->midi_devices.clear();
        state->midi_devices.push_back(std::string("RtMidi error: ") +
                               error.getMessage());
    }
}

void init_midi(struct state* state)
{
    if (state->selected_device < 0 ||
        state->selected_device >= (int)state->midi_devices.size())
    {
        snprintf(state->status, sizeof(state->status),
                 "No MIDI device selected");
        return;
    }

    try {
        // Close previous MIDI input if any
        state->midi_in.reset();

        // Create new RtMidiIn and open the selected port
        state->midi_in = std::make_unique<RtMidiIn>();
        state->midi_in->openPort(state->selected_device);
        state->midi_in->ignoreTypes(false, false, false);

        snprintf(state->status, sizeof(state->status),
                 "MIDI input opened: %s",
                 state->midi_devices[state->selected_device].c_str());
    }
    catch (RtMidiError &error) {
        snprintf(state->status, sizeof(state->status),
                 "RtMidi error: %s", error.getMessage().c_str());
        state->midi_in.reset();  // ensure it's null
    }
}


void poll_midi(struct state* state)
{
    if (!state->midi_in) return;  // MIDI not initialized

    std::vector<unsigned char> message;
    double stamp;

    while ((stamp = state->midi_in->getMessage(&message)) != 0.0) {
        if (message.size() >= 3) {
            unsigned char status = message[0] & 0xF0;
            unsigned char note   = message[1];
            unsigned char velocity = message[2];

            if (status == 0x90 && velocity > 0) {
                snprintf(state->status, sizeof(state->status),
                         "Note ON: %u (vel %u)", note, velocity);
            } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
                snprintf(state->status, sizeof(state->status),
                         "Note OFF: %u", note);
            }
        }
    }
}

