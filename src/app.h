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

void init_state(struct state *state);
void render_ui(struct state *state);

#endif /* APP_H */
