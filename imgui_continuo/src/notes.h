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
void notes_draw(const struct state *state);

#endif /* NOTES_H */
