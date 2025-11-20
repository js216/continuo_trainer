// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file notes.h
 * @brief Note manupulation routines.
 * @author Jakob Kastelic
 */

#ifndef NOTES_H
#define NOTES_H

#include "logic.h"
#include <span>
#include <stddef.h>
#include <stdint.h>

void notes_staff(struct state *state);

void notes_single(enum key_sig key, const std::vector<midi_note> &notes,
                  uint32_t color);

void notes_chords(enum key_sig key,
                  const std::vector<std::unordered_set<midi_note>> &chords,
                  uint32_t color);

void notes_figures(const std::vector<midi_note> &notes,
                   const std::vector<std::vector<figure>> &figures,
                   uint32_t color);

#endif /* NOTES_H */
