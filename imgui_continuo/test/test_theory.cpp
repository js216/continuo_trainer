// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file test_theory.h
 * @brief Tests for the pure music theory routines.
 * @author Jakob Kastelic
 */

#include "theory.h"
#include <cstdio>

int main()
{
   printf("Testing preferred spellings for all keys and MIDI notes:\n\n");

   for (int ks = 0; ks < KEY_NUM; ks++) {
      key_sig key = static_cast<key_sig>(ks);
      printf("=== Key %s ===\n", th_key_sig_to_string(key).c_str());

      for (int m = 0; m < 128; m++) {
         auto mn           = static_cast<enum midi_note>(m);
         enum note_name nn = th_preferred_spelling(mn, key);

         // Fixed-width formatting: 3 chars for MIDI number, 4 for MIDI string,
         // 5 for note_name
         printf("MIDI %3d = %-4s --> %-5s\n", m, th_midi_to_string(mn).c_str(),
                th_nn_to_string(nn).c_str());
      }

      printf("\n");
   }

   return 0;
}
