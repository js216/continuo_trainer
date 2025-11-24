// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file theory.cpp
 * @brief Pure music theory routines.
 * @author Jakob Kastelic
 */

#include "theory.h"
#include <string>
#include <sstream>
#include <unordered_set>
#include <array>

std::string key_sig_to_string(const enum key_sig k)
{
    switch (k) {
        case KEY_SIG_0       : return "C";
        case KEY_SIG_1_SHARP : return "G";
        case KEY_SIG_2_SHARP : return "D";
        case KEY_SIG_3_SHARP : return "A";
        case KEY_SIG_4_SHARP : return "E";
        case KEY_SIG_5_SHARP : return "B";
        case KEY_SIG_6_SHARP : return "F#";
        case KEY_SIG_7_SHARP : return "C#";
        case KEY_SIG_1_FLAT  : return "F";
        case KEY_SIG_2_FLAT  : return "Bb";
        case KEY_SIG_3_FLAT  : return "Eb";
        case KEY_SIG_4_FLAT  : return "Ab";
        case KEY_SIG_5_FLAT  : return "Db";
        case KEY_SIG_6_FLAT  : return "Gb";
        case KEY_SIG_7_FLAT  : return "Cb";
        default              : return "?";
    }
}


std::string midi_to_name(const enum midi_note n)
{
   static const char *names[] = { "C", "C#", "D", "D#", "E",
      "F", "F#", "G", "G#", "A", "A#", "B" };
   int octave = (n / 12) - 1;      // n 0 = C-1
   int note   = n % 12;
   return std::string(names[note]) + std::to_string(octave);
}

std::string fig_to_string(const struct figure &f)
{
   std::stringstream ss;
   switch (f.acc) {
      case ACC_NONE: break;
      case ACC_SHARP: ss << "#"; break;
      case ACC_FLAT: ss << "b"; break;

      case ACC_NATURAL:
      case ACC_NUM:
                     ; // do nothing
   }
   if (f.num != 0)
      ss << f.num;
   return ss.str();
}

enum accidental key_sig_accidental(enum key_sig key, enum midi_note n)
{
    static const int8_t table[KEY_NUM][12] = {
       // C  C# D  D# E  F  F# G  G# A  A# B
        { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 }, // C Major
        { 0, 1, 0, 1, 0, 3, 0, 0, 1, 0, 1, 0 }, // G Major
        { 3, 0, 0, 1, 0, 3, 0, 0, 1, 0, 1, 0 }, // D Major
        { 3, 0, 0, 1, 0, 3, 0, 3, 0, 0, 1, 0 }, // A Major
        { 3, 0, 3, 0, 0, 3, 0, 3, 0, 0, 1, 0 }, // E Major
        { 3, 0, 3, 0, 0, 3, 0, 3, 0, 3, 0, 0 }, // B Major
        { 0, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 3 }, // F# Major
        { 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 3 }, // C# Major
        { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 3 }, // F Major
        { 0, 1, 0, 0, 3, 0, 1, 0, 1, 0, 0, 3 }, // Bb Major
        { 0, 1, 0, 0, 3, 0, 1, 0, 0, 3, 0, 3 }, // Eb Major
        { 3, 0, 3, 0, 3, 0, 1, 0, 0, 3, 0, 3 }, // Ab Major
        { 3, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 3 }, // Db Major
        { 3, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 0 }, // Gb Major
        { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }, // Cb Major
    };

    int8_t ks = table[key][n % 12];

    if (ks == 1) return ACC_SHARP;
    if (ks == 2) return ACC_FLAT;
    if (ks == 3) return ACC_NATURAL;

    return ACC_NONE;
}


static int staff_adjust_for_key(enum midi_note n, enum key_sig k)
{
    static const int flat_key_pc_adjust[7][12] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 }, // Bb
        { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0 }, // Eb
        { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0 }, // Ab
        { 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0 }, // Db
        { 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0 }, // Gb
        { 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0 }, // Cb
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 }  // F
    };

    if (k >= KEY_SIG_1_FLAT && k <= KEY_SIG_7_FLAT) {
        int pc = n % 12;
        return flat_key_pc_adjust[k - KEY_SIG_1_FLAT][pc];
    }

    // Sharp keys and C major need no adjustment
    return 0;
}


int note_to_bass(const enum midi_note n, const enum key_sig k)
{
    int base;

    switch (n) {
        case NOTES_D2:
        case NOTES_Ds2: base = -3; break;
        case NOTES_E2: base = -2; break;
        case NOTES_F2:
        case NOTES_Fs2: base = -1; break;
        case NOTES_G2:
        case NOTES_Gs2: base = 0; break;
        case NOTES_A2:
        case NOTES_As2: base = 1; break;
        case NOTES_B2: base = 2; break;
        case NOTES_C3:
        case NOTES_Cs3: base = 3; break;
        case NOTES_D3:
        case NOTES_Ds3: base = 4; break;
        case NOTES_E3: base = 5; break;
        case NOTES_F3:
        case NOTES_Fs3: base = 6; break;
        case NOTES_G3:
        case NOTES_Gs3: base = 7; break;
        case NOTES_A3:
        case NOTES_As3: base = 8; break;
        case NOTES_B3: base = 9; break;
        case NOTES_C4:
        case NOTES_Cs4: base = 10; break;
        case NOTES_D4:
        case NOTES_Ds4: base = 11; break;
        default: return NOTES_OUT_OF_RANGE;
    }

    return base + staff_adjust_for_key(n, k);
}

int note_to_treble(const enum midi_note n, const enum key_sig k)
{
    int base;

    switch (n) {
        case NOTES_E4: base = 0; break;
        case NOTES_F4:
        case NOTES_Fs4: base = 1; break;
        case NOTES_G4:
        case NOTES_Gs4: base = 2; break;
        case NOTES_A4:
        case NOTES_As4: base = 3; break;
        case NOTES_B4: base = 4; break;
        case NOTES_C5:
        case NOTES_Cs5: base = 5; break;
        case NOTES_D5:
        case NOTES_Ds5: base = 6; break;
        case NOTES_E5: base = 7; break;
        case NOTES_F5:
        case NOTES_Fs5: base = 8; break;
        case NOTES_G5:
        case NOTES_Gs5: base = 9; break;
        case NOTES_A5:
        case NOTES_As5: base = 10; break;
        default: return NOTES_OUT_OF_RANGE;
    }

    return base + staff_adjust_for_key(n, k);
}

int key_sig_acc_count(enum key_sig key)
{
   switch (key) {
      case KEY_SIG_1_SHARP: return 1;
      case KEY_SIG_2_SHARP: return 2;
      case KEY_SIG_3_SHARP: return 3;
      case KEY_SIG_4_SHARP: return 4;
      case KEY_SIG_5_SHARP: return 5;
      case KEY_SIG_6_SHARP: return 6;
      case KEY_SIG_7_SHARP: return 7;
      case KEY_SIG_1_FLAT: return -1;
      case KEY_SIG_2_FLAT: return -2;
      case KEY_SIG_3_FLAT: return -3;
      case KEY_SIG_4_FLAT: return -4;
      case KEY_SIG_5_FLAT: return -5;
      case KEY_SIG_6_FLAT: return -6;
      case KEY_SIG_7_FLAT: return -7;
      default: return 0;
   }
}

