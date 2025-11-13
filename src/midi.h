/**
 * @file midi.h
 * @brief Handling MIDI devices.
 * @author Jakob Kastelic
 */

#ifndef MIDI_H
#define MIDI_H

#include "state.h"

void refresh_midi_devices(struct state *state);

void init_midi(struct state *state);

void poll_midi(struct state *state);

#endif // MIDI_H
