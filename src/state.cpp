// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Application state manipulation.
 * @author Jakob Kastelic
 */

#include "state.h"
#include <fstream>
#include <string>

void state_load(struct state *state)
{
   if (state->config_file.empty())
      return;

   std::ifstream f(state->config_file);
   if (!f.is_open())
      return;

   std::string line;
   if (std::getline(f, line)) {
      // line contains previously selected device name
      if (!line.empty()) {
         for (int i = 0; i < (int)state->midi_devices.size(); i++) {
            if (state->midi_devices[i] == line) {
               state->selected_device = i;
               break;
            }
         }
      }
   }
}

void state_save(const struct state *state)
{
   if (state->config_file.empty())
      return;

   if (state->selected_device < 0 ||
       state->selected_device >= (int)state->midi_devices.size())
      return;

   std::ofstream f(state->config_file);
   if (!f.is_open())
      return;

   f << state->midi_devices[state->selected_device] << "\n";
}
