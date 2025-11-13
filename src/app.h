/**
 * @file app.h
 * @brief Portable application logic interface.
 * @author Jakob Kastelic
 */

#ifndef APP_H
#define APP_H

#include "notes.h"
#include <stddef.h>

#define APP_MAX_NOTES 20

struct app_state {
   char status[64];
   enum midi_note bassline[APP_MAX_NOTES];
};

void init_state(struct app_state *state);
void render_ui(struct app_state *state);

#endif /* APP_H */
