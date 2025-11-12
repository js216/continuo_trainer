/**
 * @file app.h
 * @brief Portable application logic interface.
 * @author Jakob Kastelic
 */

#ifndef APP_H
#define APP_H

#include <stddef.h>

struct app_state {
   char status[64];
};

void init_state(struct app_state *state);
void render_ui(struct app_state *state);

#endif /* APP_H */
