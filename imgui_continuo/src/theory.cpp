// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file theory.cpp
 * @brief Pure music theory routines.
 * @author Jakob Kastelic
 */

#include "theory.h"
#include "util.h"
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

std::string th_key_sig_to_string(const enum key_sig k)
{
   switch (k) {
      case KEY_SIG_0: return "C";
      case KEY_SIG_1_SHARP: return "G";
      case KEY_SIG_2_SHARP: return "D";
      case KEY_SIG_3_SHARP: return "A";
      case KEY_SIG_4_SHARP: return "E";
      case KEY_SIG_5_SHARP: return "B";
      case KEY_SIG_6_SHARP: return "F#";
      case KEY_SIG_7_SHARP: return "C#";
      case KEY_SIG_1_FLAT: return "F";
      case KEY_SIG_2_FLAT: return "Bb";
      case KEY_SIG_3_FLAT: return "Eb";
      case KEY_SIG_4_FLAT: return "Ab";
      case KEY_SIG_5_FLAT: return "Db";
      case KEY_SIG_6_FLAT: return "Gb";
      case KEY_SIG_7_FLAT: return "Cb";
      default: return "?";
   }
}

static std::string pitch_class_name(const int pc)
{
   switch (pc) {
      case 0: return "C";
      case 1: return "C#";
      case 2: return "D";
      case 3: return "D#";
      case 4: return "E";
      case 5: return "F";
      case 6: return "F#";
      case 7: return "G";
      case 8: return "G#";
      case 9: return "A";
      case 10: return "A#";
      case 11: return "B";
      default: return "?";
   }
}

std::string th_midi_to_name(const enum midi_note n)
{
   int octave = (n / 12) - 1; // n 0 = C-1
   int note   = n % 12;
   return std::string(pitch_class_name(note)) + std::to_string(octave);
}

std::string th_fig_to_string(const std::vector<figure> &figs)
{
   if (figs.empty())
      return "- ";

   std::stringstream ss;
   for (size_t i = 0; i < figs.size(); ++i) {
      if (i > 0)
         ss << ",";
      const auto &f = figs[i];
      switch (f.acc) {
         case ACC_NONE: break;
         case ACC_SHARP: ss << "#"; break;
         case ACC_FLAT: ss << "b"; break;
         case ACC_NATURAL: ss << "n"; break;
         case ACC_SLASH: ss << "/"; break;
         case ACC_NUM:
         default:; // do nothing
      }
      if (f.num != 0)
         ss << f.num;
   }
   ss << " "; // keep trailing space as before
   return ss.str();
}

enum accidental th_key_sig_accidental(enum key_sig key, enum midi_note n)
{
   static const int8_t table[KEY_NUM][12] = {
       // C  C# D  D# E  F  F# G  G# A  A# B
       {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0}, // C Major
       {0, 1, 0, 1, 0, 3, 0, 0, 1, 0, 1, 0}, // G Major
       {3, 0, 0, 1, 0, 3, 0, 0, 1, 0, 1, 0}, // D Major
       {3, 0, 0, 1, 0, 3, 0, 3, 0, 0, 1, 0}, // A Major
       {3, 0, 3, 0, 0, 3, 0, 3, 0, 0, 1, 0}, // E Major
       {3, 0, 3, 0, 0, 3, 0, 3, 0, 3, 0, 0}, // B Major
       {0, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 3}, // F# Major
       {0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 3}, // C# Major
       {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 3}, // F Major
       {0, 1, 0, 0, 3, 0, 1, 0, 1, 0, 0, 3}, // Bb Major
       {0, 1, 0, 0, 3, 0, 1, 0, 0, 3, 0, 3}, // Eb Major
       {3, 0, 3, 0, 3, 0, 1, 0, 0, 3, 0, 3}, // Ab Major
       {3, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 3}, // Db Major
       {3, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 0}, // Gb Major
       {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}, // Cb Major
   };

   int8_t ks = table[key][n % 12];

   if (ks == 1)
      return ACC_SHARP;
   if (ks == 2)
      return ACC_FLAT;
   if (ks == 3)
      return ACC_NATURAL;

   return ACC_NONE;
}

static int staff_adjust_for_key(enum midi_note n, enum key_sig k)
{
   static const int flat_key_pc_adjust[7][12] = {
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, // Bb
       {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0}, // Eb
       {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0}, // Ab
       {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0}, // Db
       {0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0}, // Gb
       {1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0}, // Cb
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}  // F
   };

   if (k >= KEY_SIG_1_FLAT && k <= KEY_SIG_7_FLAT) {
      int pc = n % 12;
      return flat_key_pc_adjust[k - KEY_SIG_1_FLAT][pc];
   }

   // Sharp keys and C major need no adjustment
   return 0;
}

int th_note_to_bass(const enum midi_note n, const enum key_sig k)
{
   int base = 0;

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

int th_note_to_treble(const enum midi_note n, const enum key_sig k)
{
   int base = 0;

   switch (n) {
      case NOTES_D4:
      case NOTES_Ds4: base = -1; break;
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

int th_key_sig_acc_count(enum key_sig key)
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

key_sig th_parse_key(const std::string &token)
{
   static const std::unordered_map<std::string, key_sig> key_map = {
       {"C",  KEY_SIG_0      },
       {"G",  KEY_SIG_1_SHARP},
       {"D",  KEY_SIG_2_SHARP},
       {"A",  KEY_SIG_3_SHARP},
       {"E",  KEY_SIG_4_SHARP},
       {"B",  KEY_SIG_5_SHARP},
       {"F#", KEY_SIG_6_SHARP},
       {"C#", KEY_SIG_7_SHARP},
       {"F",  KEY_SIG_1_FLAT },
       {"Bb", KEY_SIG_2_FLAT },
       {"Eb", KEY_SIG_3_FLAT },
       {"Ab", KEY_SIG_4_FLAT },
       {"Db", KEY_SIG_5_FLAT },
       {"Gb", KEY_SIG_6_FLAT },
       {"Cb", KEY_SIG_7_FLAT }
   };

   auto it = key_map.find(token);
   if (it != key_map.end())
      return it->second;

   error("Unknown key: " + token);
   return KEY_SIG_0; // fallback
}

std::vector<figure> th_parse_figures_from_str(const std::string &token)
{
   std::vector<figure> figures;

   if (token == "-")
      return figures;

   std::stringstream ss(token);
   std::string part;
   while (std::getline(ss, part, ',')) {
      if (part.empty())
         continue;

      figure fig{};
      fig.acc = ACC_NONE;
      fig.num = 0;

      size_t pos = 0;
      if (part[0] == '#') {
         fig.acc = ACC_SHARP;
         pos     = 1;
      } else if (part[0] == 'b') {
         fig.acc = ACC_FLAT;
         pos     = 1;
      } else if (part[0] == 'n') {
         fig.acc = ACC_NATURAL;
         pos     = 1;
      } else if (part[0] == '/') {
         fig.acc = ACC_SLASH;
         pos     = 1;
      }

      int value = 0;
      bool ok   = true;
      for (size_t i = pos; i < part.size(); ++i) {
         if (!isdigit(part[i])) {
            ok = false;
            break;
         }
         value = value * 10 + (part[i] - '0');
      }

      if (ok)
         fig.num = value;
      else
         error("Invalid figure number: " + part);

      figures.push_back(fig);
   }

   return figures;
}

midi_note th_parse_midi_note(const std::string &token)
{
   static const std::unordered_map<std::string, midi_note> note_map = {
       {"D2",  NOTES_D2 },
       {"D#2", NOTES_Ds2},
       {"E2",  NOTES_E2 },
       {"F2",  NOTES_F2 },
       {"F#2", NOTES_Fs2},
       {"G2",  NOTES_G2 },
       {"G#2", NOTES_Gs2},
       {"A2",  NOTES_A2 },
       {"A#2", NOTES_As2},
       {"B2",  NOTES_B2 },
       {"C3",  NOTES_C3 },
       {"C#3", NOTES_Cs3},
       {"D3",  NOTES_D3 },
       {"D#3", NOTES_Ds3},
       {"E3",  NOTES_E3 },
       {"F3",  NOTES_F3 },
       {"F#3", NOTES_Fs3},
       {"G3",  NOTES_G3 },
       {"G#3", NOTES_Gs3},
       {"A3",  NOTES_A3 },
       {"A#3", NOTES_As3},
       {"B3",  NOTES_B3 },
       {"C4",  NOTES_C4 },
       {"C#4", NOTES_Cs4},
       {"D4",  NOTES_D4 },
       {"D#4", NOTES_Ds4},
       {"E4",  NOTES_E4 },
       {"F4",  NOTES_F4 },
       {"F#4", NOTES_Fs4},
       {"G4",  NOTES_G4 },
       {"G#4", NOTES_Gs4},
       {"A4",  NOTES_A4 },
       {"A#4", NOTES_As4},
       {"B4",  NOTES_B4 },
       {"C5",  NOTES_C5 },
       {"C#5", NOTES_Cs5},
       {"D5",  NOTES_D5 },
       {"D#5", NOTES_Ds5},
       {"E5",  NOTES_E5 },
       {"F5",  NOTES_F5 },
       {"F#5", NOTES_Fs5},
       {"G5",  NOTES_G5 },
       {"G#5", NOTES_Gs5},
       {"A5",  NOTES_A5 },
       {"A#5", NOTES_As5},
   };

   auto it = note_map.find(token);
   if (it != note_map.end())
      return it->second;

   error("Unknown note: " + token);
   return NOTES_E2; // fallback
}
