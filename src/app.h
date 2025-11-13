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

struct app_state {
   char status[64];
   enum midi_note bassline[MAX_CHORDS];
   enum midi_note chords[NOTES_PER_CHORD][MAX_CHORDS];
};

void init_state(struct app_state *state);
void render_ui(struct app_state *state);

#endif /* APP_H */
