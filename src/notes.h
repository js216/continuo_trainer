/**
 * @file notes.h
 * @brief Note manupulation routines.
 * @author Jakob Kastelic
 */

#ifndef NOTES_H
#define NOTES_H

#include "logic.h"
#include <stddef.h>
#include <stdint.h>

void notes_staff(void);

void notes_dots(const enum midi_note *n_arr, size_t count, uint32_t color);

void notes_chords(const enum midi_note ch_arr[NOTES_PER_CHORD][MAX_CHORDS],
                  uint32_t color);

#endif /* NOTES_H */
