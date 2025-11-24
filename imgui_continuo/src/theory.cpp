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
        { 0,0,0,0,0,0,0,0,0,0,0,0 },            // C
        { 0,0,0,0,0,0,1,0,0,0,0,0 },            // G
        { 0,1,0,0,0,0,1,0,0,0,0,0 },            // D
        { 0,1,0,0,0,0,1,0,1,0,0,0 },            // A
        { 0,1,0,1,0,0,1,0,1,0,0,0 },            // E
        { 0,1,0,1,0,0,1,0,1,0,1,0 },            // B
        { 0,1,0,1,1,0,1,0,1,0,1,0 },            // F#
        { 1,1,0,1,1,0,1,0,1,0,1,1 },            // C#
        { 0,0,0,0,0,0,0,0,0,0,-1,0 },           // F
        { 0,0,0,0,0,0,0,0,0,0,-1,-1 },          // Bb
        { 0,0,0,0,0,0,0,-1,0,0,-1,-1 },         // Eb
        { 0,-1,0,0,0,0,0,-1,0,0,-1,-1 },        // Ab
        { 0,-1,0,0,0,-1,0,-1,0,0,-1,-1 },       // Db
        { -1,-1,0,0,0,-1,0,-1,0,0,-1,-1 },      // Gb
        { -1,-1,0,-1,0,-1,0,-1,0,0,-1,-1 }      // Cb
    };

    int pc = (int)n % 12;
    int8_t ks = table[key][pc];

    bool actual_sharp =
        (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);

    bool actual_flat = actual_sharp; // MIDI enum only has sharps

    enum accidental result;

    if (ks > 0)            result = actual_sharp ? ACC_NONE : ACC_SHARP;
    else if (ks < 0)       result = actual_flat ? ACC_NONE : ACC_FLAT;
    else                   // key wants natural
    {
        if (actual_sharp)   result = ACC_SHARP;
        else if (actual_flat) result = ACC_FLAT;
        else                result = ACC_NONE;
    }

    return result;
}

