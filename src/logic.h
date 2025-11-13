/**
 * @file logic.h
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#ifndef LOGIC_H
#define LOGIC_H

#include <stddef.h>

#define NOTES_PER_CHORD 10
#define MAX_CHORDS      20

enum midi_note {
   NOTES_NONE = 0,
   NOTES_E2   = 40,
   NOTES_F2,
   NOTES_Fs2,
   NOTES_G2,
   NOTES_Gs2,
   NOTES_A2,
   NOTES_As2,
   NOTES_B2,
   NOTES_C3,
   NOTES_Cs3,
   NOTES_D3,
   NOTES_Ds3,
   NOTES_E3,
   NOTES_F3,
   NOTES_Fs3,
   NOTES_G3,
   NOTES_Gs3,
   NOTES_A3,
   NOTES_As3,
   NOTES_B3,
   NOTES_C4,
   NOTES_Cs4,
   NOTES_D4,
   NOTES_Ds4,
   NOTES_E4,
   NOTES_NUM
};

void logic_clear(enum midi_note *n_arr, size_t count);

void logic_pop(enum midi_note *n_arr, size_t count);

void logic_one(enum midi_note ch_arr[NOTES_PER_CHORD][MAX_CHORDS]);

#endif /* LOGIC_H */
