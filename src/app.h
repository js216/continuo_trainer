/**
 * @file app.h
 * @brief Portable application logic interface.
 * @author Jakob Kastelic
 */

#ifndef APP_H
#define APP_H

#include <stddef.h>

#define MAX_NOTES 10

struct note {
   int position;     /**< Vertical position for rendering */
   const char *name; /**< Name displayed on button */
};

struct app_state {
   char text[64];         /**< Generic text buffer */
   char status[64];       /**< Status message */
   int notes[MAX_NOTES];  /**< Main note positions */
   int chords[MAX_NOTES]; /**< Chord note positions */
   int note_count;        /**< Number of main notes added (usually 1) */
   int chord_count;       /**< Number of chords added above main note */
};

static const struct note NOTES[] = {
    {8, "G2"},
    {7, "A2"},
    {6, "B2"},
    {5, "C3"},
    {4, "D3"},
    {3, "E3"},
    {2, "F3"},
    {1, "G4"},
    {0, "A4"},
};

#define NOTE_COUNT (sizeof(NOTES) / sizeof(NOTES[0]))

void init_state(struct app_state *state);
void render_ui(struct app_state *state);
void clear_notes(struct app_state *state);

#endif /* APP_H */
