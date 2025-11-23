// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Application state manipulation.
 * @author Jakob Kastelic
 */

#include "state.h"
#include "util.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

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

void read_bassline_from_file(const std::string &filename,
                             std::vector<column> &chords)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        ERROR("Cannot open file: " + filename);
        return;
    }

    chords.clear();
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::istringstream iss(line);
        int value = 0;
        if (!(iss >> value))
        {
            ERROR("Skipping invalid line: " + line);
            continue;
        }

        column col;
        col.bass.insert(static_cast<midi_note>(value));
        // leave figures, melody, good, bad empty for now
        chords.push_back(std::move(col));
    }
}

