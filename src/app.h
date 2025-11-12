/**
 * @file app.h
 * @brief Portable application logic.
 * @author Jakob Kastelic
 */

#ifndef APP_H
#define APP_H

#include "imgui.h"
#include <stdbool.h>

struct app_state {
   char text[128];
   char status[128];
   int notes[10];
   int note_count;
};

void init_state(struct app_state *state);

void draw_staff(ImDrawList *draw_list, ImVec2 pos, float width, float spacing,
                ImU32 color);
void draw_notes(ImDrawList *draw_list, ImVec2 pos, float spacing,
                struct app_state *state, ImU32 color);

void add_note(struct app_state *state, int position, const char *name);
void action_add_c(struct app_state *state);
void action_add_g(struct app_state *state);
void action_clear(struct app_state *state);

void render_ui(struct app_state *state);

#endif // APP_H
