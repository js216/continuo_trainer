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

void notes_staff(void);

void notes_single(const std::vector<midi_note> &notes, uint32_t color);

void notes_chords(const std::vector<std::unordered_set<midi_note>> &chords,
                  uint32_t color);

#endif /* NOTES_H */
