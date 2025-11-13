/**
 * @file logic.h
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#ifndef LOGIC_H
#define LOGIC_H

#include "notes.h"

void logic_clear(enum midi_note *n_arr, size_t count);

void logic_pop(enum midi_note *n_arr, size_t count);

#endif /* LOGIC_H */
