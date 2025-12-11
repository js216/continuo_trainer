// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file theory.cpp
 * @brief Pure music theory routines.
 * @author Jakob Kastelic
 */

#include "theory.h"
#include "util.h"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// TODO: remove this
#include <iostream>

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

static int midi_octave(const enum midi_note n)
{
   return (n / 12) - 1; // n 0 = C-1
}

static int midi_pc(const enum midi_note n)
{
   return n % 12;
}

std::string th_midi_to_string(const enum midi_note n)
{
   return std::string(pitch_class_name(midi_pc(n))) +
          std::to_string(midi_octave(n));
}

static std::string enharm_pc_name(int pos_in_oct)
{
   switch (pos_in_oct) {
      case 0: return "C";
      case 1: return "C#";
      case 2: return "Db";
      case 3: return "D";
      case 4: return "D#";
      case 5: return "Eb";
      case 6: return "E";
      case 7: return "Fb";
      case 8: return "E#";
      case 9: return "F";
      case 10: return "F#";
      case 11: return "Gb";
      case 12: return "G";
      case 13: return "G#";
      case 14: return "Ab";
      case 15: return "A";
      case 16: return "A#";
      case 17: return "Bb";
      case 18: return "B";
      case 19: return "Cb";
      case 20: return "B#";
      default: return "?";
   }
}

std::string th_nn_to_string(const note_name nn)
{
   constexpr int NAMES_PER_OCTAVE = 21;

   int idx = static_cast<int>(nn);
   if (idx < 0 || idx >= static_cast<int>(NN_NUM))
      return "?";

   const int octave     = idx / NAMES_PER_OCTAVE - 1;
   const int pos_in_oct = idx % NAMES_PER_OCTAVE;

   return enharm_pc_name(pos_in_oct) + std::to_string(octave);
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
   return ss.str();
}

enum accidental th_key_sig_accidental(enum key_sig key, enum note_name nn)
{
   static const int8_t table[KEY_NUM][21] = {
       // C C# Db D  D# Eb E  Fb E# F  F# Gb G  G# Ab A  A# Bb B  Cb B#
       {0, 1, 2, 0, 1, 2, 0, 2, 1, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 2, 1}, // C
       {0, 1, 2, 0, 1, 2, 0, 2, 1, 3, 0, 2, 0, 1, 2, 0, 1, 2, 0, 2, 1}, // G
       {3, 0, 2, 0, 1, 2, 0, 2, 1, 3, 0, 2, 0, 1, 2, 0, 1, 2, 0, 2, 1}, // D
       {3, 0, 2, 0, 1, 2, 0, 2, 1, 3, 0, 2, 3, 0, 2, 0, 1, 2, 0, 2, 1}, // A
       {3, 0, 2, 3, 0, 2, 0, 2, 1, 3, 0, 2, 3, 0, 2, 0, 1, 2, 0, 2, 1}, // E
       {3, 0, 2, 3, 0, 2, 0, 2, 1, 3, 0, 2, 3, 0, 2, 3, 0, 2, 0, 2, 1}, // B
       {3, 0, 2, 3, 0, 2, 3, 2, 0, 3, 0, 2, 3, 0, 2, 3, 0, 2, 0, 2, 1}, // F#
       {3, 0, 2, 3, 0, 2, 3, 2, 0, 3, 0, 2, 3, 0, 2, 3, 0, 2, 3, 2, 0}, // C#
       {0, 1, 2, 0, 1, 2, 0, 2, 1, 0, 1, 2, 0, 1, 2, 0, 1, 0, 3, 2, 1}, // F
       {0, 1, 2, 0, 1, 0, 3, 2, 1, 0, 1, 2, 0, 1, 2, 0, 1, 0, 3, 2, 1}, // Bb
       {0, 1, 2, 0, 1, 0, 3, 2, 1, 0, 1, 2, 0, 1, 0, 3, 1, 0, 3, 2, 1}, // Eb
       {0, 1, 0, 3, 1, 0, 3, 2, 1, 0, 1, 2, 0, 1, 0, 3, 1, 0, 3, 2, 1}, // Ab
       {0, 1, 0, 3, 1, 0, 3, 2, 1, 0, 1, 0, 3, 1, 0, 3, 1, 0, 3, 2, 1}, // Db
       {3, 1, 0, 3, 1, 0, 3, 2, 1, 0, 1, 0, 3, 1, 0, 3, 1, 0, 3, 0, 1}, // Gb
       {3, 1, 0, 3, 1, 0, 3, 0, 1, 3, 1, 0, 3, 1, 0, 3, 1, 0, 3, 0, 1}, // Cb
   };

   const int8_t ks = table[key][nn % 21];

   if (ks == 1)
      return ACC_SHARP;
   if (ks == 2)
      return ACC_FLAT;
   if (ks == 3)
      return ACC_NATURAL;

   return ACC_NONE;
}

static int staff_adjust_for_key(enum note_name nn, enum key_sig key)
{
   return 0; // TODO: remove this
   static const int flat_key_pc_adjust[7][12] = {
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, // Bb
       {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0}, // Eb
       {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0}, // Ab
       {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0}, // Db
       {0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0}, // Gb
       {1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0}, // Cb
       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}  // F
   };

   // Map 21-note-per-octave note_name to pitch class 0..11
   static const int note_pc[21] = {0, 1, 1, 2, 3, 3,  4,  4,  5, 5, 6,
                                   6, 7, 8, 8, 9, 10, 10, 11, 0, 0};

   int adj = 0;
   if (key >= KEY_SIG_1_FLAT && key <= KEY_SIG_7_FLAT) {
      int pc = note_pc[nn % 21]; // pitch class within octave
      adj    = flat_key_pc_adjust[key - KEY_SIG_1_FLAT][pc];
   }

   return adj; // sharp keys and C major
}

int th_note_to_bass(const enum note_name nn, const enum key_sig key)
{
   int base = 0;
   switch (nn) {
      case NN_Cb1: base = -4; break;
      case NN_C2: base = -4; break;
      case NN_Cs2: base = -4; break;
      case NN_Db2: base = -3; break;
      case NN_D2: base = -3; break;
      case NN_Ds2: base = -3; break;
      case NN_Eb2: base = -2; break;
      case NN_E2: base = -2; break;
      case NN_Fb2: base = -1; break;
      case NN_Es2: base = -2; break;
      case NN_F2: base = -1; break;
      case NN_Fs2: base = -1; break;
      case NN_Gb2: base = 0; break;
      case NN_G2: base = 0; break;
      case NN_Gs2: base = 0; break;
      case NN_Ab2: base = 1; break;
      case NN_A2: base = 1; break;
      case NN_As2: base = 1; break;
      case NN_Bb2: base = 2; break;
      case NN_B2: base = 2; break;
      case NN_Cb2: base = 3; break;
      case NN_Bs2: base = 2; break;
      case NN_C3: base = 3; break;
      case NN_Cs3: base = 3; break;
      case NN_Db3: base = 4; break;
      case NN_D3: base = 4; break;
      case NN_Ds3: base = 4; break;
      case NN_Eb3: base = 5; break;
      case NN_E3: base = 5; break;
      case NN_Fb3: base = 6; break;
      case NN_Es3: base = 6; break;
      case NN_F3: base = 6; break;
      case NN_Fs3: base = 6; break;
      case NN_Gb3: base = 7; break;
      case NN_G3: base = 7; break;
      case NN_Gs3: base = 7; break;
      case NN_Ab3: base = 8; break;
      case NN_A3: base = 8; break;
      case NN_As3: base = 8; break;
      case NN_Bb3: base = 9; break;
      case NN_B3: base = 9; break;
      case NN_Cb3: base = 10; break;
      case NN_Bs3: base = 9; break;
      case NN_C4: base = 10; break;
      case NN_Cs4: base = 10; break;
      default: return NOTES_OUT_OF_RANGE;
   }

   return base + staff_adjust_for_key(nn, key);
}

int th_note_to_treble(const enum note_name nn, const enum key_sig key)
{
   int base = 0;

   switch (nn) {
      case NN_Db4: base = -1; break;
      case NN_D4: base = -1; break;
      case NN_Ds4: base = -1; break;
      case NN_Eb4: base = 0; break;
      case NN_E4: base = 0; break;
      case NN_Fb4: base = 1; break;
      case NN_Es4: base = 0; break;
      case NN_F4: base = 1; break;
      case NN_Fs4: base = 1; break;
      case NN_Gb4: base = 2; break;
      case NN_G4: base = 2; break;
      case NN_Gs4: base = 2; break;
      case NN_Ab4: base = 3; break;
      case NN_A4: base = 3; break;
      case NN_As4: base = 3; break;
      case NN_Bb4: base = 4; break;
      case NN_B4: base = 4; break;
      case NN_Cb4: base = 5; break;
      case NN_Bs4: base = 4; break;
      case NN_C5: base = 5; break;
      case NN_Cs5: base = 5; break;
      case NN_Db5: base = 6; break;
      case NN_D5: base = 6; break;
      case NN_Ds5: base = 6; break;
      case NN_Eb5: base = 7; break;
      case NN_E5: base = 7; break;
      case NN_Fb5: base = 8; break;
      case NN_Es5: base = 7; break;
      case NN_F5: base = 8; break;
      case NN_Fs5: base = 8; break;
      case NN_Gb5: base = 9; break;
      case NN_G5: base = 9; break;
      case NN_Gs5: base = 9; break;
      case NN_Ab5: base = 10; break;
      case NN_A5: base = 10; break;
      case NN_As5: base = 10; break;
      case NN_Bb5: base = 11; break;
      default: return NOTES_OUT_OF_RANGE;
   }

   return base + staff_adjust_for_key(nn, key);
}

static int octave_start_nn(const enum midi_note n)
{
   switch (midi_octave(n)) {
      case -1: return NN_C_1;
      case 0: return NN_C0;
      case 1: return NN_C1;
      case 2: return NN_C2;
      case 3: return NN_C3;
      case 4: return NN_C4;
      case 5: return NN_C5;
      case 6: return NN_C6;
      case 7: return NN_C7;
      case 8: return NN_C8;
      case 9: return NN_C9;
      default: return NN_C0; // fallback
   }
}

enum note_name th_preferred_spelling(enum midi_note n, enum key_sig key)
{
   const int ks            = th_key_sig_acc_count(key);
   const bool prefer_flats = (ks < 0);
   const int base          = octave_start_nn(n); // start of 21-note block

   int idx = 0;

   switch (midi_pc(n)) {
      case 0: idx = 0; break;                       // C
      case 1: idx = prefer_flats ? 2 : 1; break;    // C#/Db
      case 2: idx = 3; break;                       // D
      case 3: idx = prefer_flats ? 5 : 4; break;    // D#/Eb
      case 4: idx = 6; break;                       // E
      case 5: idx = 9; break;                       // F
      case 6: idx = prefer_flats ? 11 : 10; break;  // F#/Gb
      case 7: idx = 12; break;                      // G
      case 8: idx = prefer_flats ? 14 : 13; break;  // G#/Ab
      case 9: idx = 15; break;                      // A
      case 10: idx = prefer_flats ? 17 : 16; break; // A#/Bb
      case 11: idx = 18; break;                     // B
      default: idx = 0; break;
   }

   auto nn = static_cast<note_name>(base + idx);
   return nn;
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
         value = (value * 10) + (part[i] - '0');
      }

      if (ok)
         fig.num = value;
      else
         error("Invalid figure number: " + part);

      figures.push_back(fig);
   }

   return figures;
}

enum midi_note th_parse_midi_note(const std::string &token)
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

std::unordered_set<enum midi_note>
th_get_missed(const std::unordered_set<enum midi_note> &answer,
              const std::unordered_set<enum midi_note> &good)
{
   // Build a set of pitch classes that were played correctly
   std::unordered_set<int> good_pc;
   for (auto n : good) {
      good_pc.insert(midi_pc(n));
   }

   // Collect missed notes from answer
   std::unordered_set<enum midi_note> missed;
   for (auto n : answer) {
      if (!good_pc.contains(midi_pc(n)))
         missed.insert(n);
   }

   return missed;
}
