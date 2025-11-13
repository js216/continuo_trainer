// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.h
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#ifndef LOGIC_H
#define LOGIC_H

#include "state.h"
#include <stddef.h>

void logic_clear(struct state *state);

void logic_pop(struct state *state);

void logic_good(struct state *state);

void logic_bad(struct state *state);

void logic_interpret(struct state *state);

#endif /* LOGIC_H */
