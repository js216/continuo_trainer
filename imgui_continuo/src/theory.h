// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file theory.h
 * @brief Pure music theory routines.
 * @author Jakob Kastelic
 */

#ifndef THEORY_H
#define THEORY_H

#include <string>

#define NOTES_OUT_OF_RANGE (-100)

enum midi_note {
   NOTES_D2 = 38,
   NOTES_Ds2,
   NOTES_E2,
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
   NOTES_F4,
   NOTES_Fs4,
   NOTES_G4,
   NOTES_Gs4,
   NOTES_A4,
   NOTES_As4,
   NOTES_B4,
   NOTES_C5,
   NOTES_Cs5,
   NOTES_D5,
   NOTES_Ds5,
   NOTES_E5,
   NOTES_F5,
   NOTES_Fs5,
   NOTES_G5,
   NOTES_Gs5,
   NOTES_A5,
   NOTES_As5,
   NOTES_NUM
};

enum key_sig {
   KEY_SIG_0,
   KEY_SIG_1_SHARP,
   KEY_SIG_2_SHARP,
   KEY_SIG_3_SHARP,
   KEY_SIG_4_SHARP,
   KEY_SIG_5_SHARP,
   KEY_SIG_6_SHARP,
   KEY_SIG_7_SHARP,
   KEY_SIG_1_FLAT,
   KEY_SIG_2_FLAT,
   KEY_SIG_3_FLAT,
   KEY_SIG_4_FLAT,
   KEY_SIG_5_FLAT,
   KEY_SIG_6_FLAT,
   KEY_SIG_7_FLAT,
   KEY_NUM
};

enum accidental { ACC_NONE, ACC_SHARP, ACC_FLAT, ACC_NATURAL, ACC_NUM };

struct figure {
   int num;
   enum accidental acc;
};

std::string key_sig_to_string(enum key_sig);
std::string midi_to_name(enum midi_note n);
std::string fig_to_string(const struct figure &f);
enum accidental key_sig_accidental(enum key_sig key, midi_note n);
int note_to_bass(const enum midi_note n, const enum key_sig k);
int note_to_treble(const enum midi_note n, const enum key_sig k);
int key_sig_acc_count(enum key_sig key);

#endif /* THEORY_H */
