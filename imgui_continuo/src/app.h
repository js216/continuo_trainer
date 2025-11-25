// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file app.h
 * @brief Portable application logic interface.
 * @author Jakob Kastelic
 */

#ifndef APP_H
#define APP_H

#include "logic.h"
#include "notes.h"
#include <stddef.h>

void app_init(struct state *state);
void app_render(struct state *state);

#endif /* APP_H */
