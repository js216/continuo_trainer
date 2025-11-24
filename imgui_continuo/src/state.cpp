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
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

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

static key_sig parse_key(const std::string &token)
{
   static const std::unordered_map<std::string, key_sig> key_map = {
      {"C", KEY_SIG_0}, {"G", KEY_SIG_1_SHARP}, {"D", KEY_SIG_2_SHARP},
      {"A", KEY_SIG_3_SHARP}, {"E", KEY_SIG_4_SHARP}, {"B", KEY_SIG_5_SHARP},
      {"F#", KEY_SIG_6_SHARP}, {"C#", KEY_SIG_7_SHARP},
      {"F", KEY_SIG_1_FLAT}, {"Bb", KEY_SIG_2_FLAT}, {"Eb", KEY_SIG_3_FLAT},
      {"Ab", KEY_SIG_4_FLAT}, {"Db", KEY_SIG_5_FLAT}, {"Gb", KEY_SIG_6_FLAT},
      {"Cb", KEY_SIG_7_FLAT}
   };

   auto it = key_map.find(token);
   if (it != key_map.end())
      return it->second;

   ERROR("Unknown key: " + token);
   return KEY_SIG_0; // fallback
}

static void parse_figures_token(const std::string &token,
      std::vector<figure> &figures)
{
   if (token == "-")
      return;

   std::stringstream ss(token);
   std::string part;
   while (std::getline(ss, part, ','))
   {
      if (part.empty())
         continue;

      figure fig{};
      fig.acc = ACC_NONE;
      fig.num = 0;

      size_t pos = 0;
      if (part[0] == '#')
      {
         fig.acc = ACC_SHARP;
         pos = 1;
      }
      else if (part[0] == 'b')
      {
         fig.acc = ACC_FLAT;
         pos = 1;
      }

      int value = 0;
      bool ok = true;
      for (size_t i = pos; i < part.size(); ++i)
      {
         if (!isdigit(part[i]))
         {
            ok = false;
            break;
         }
         value = value * 10 + (part[i] - '0');
      }

      if (ok)
         fig.num = value;
      else
         ERROR("Invalid figure number: " + part);

      figures.push_back(fig);
   }
}

static midi_note parse_midi_note(const std::string &token)
{
   static const std::unordered_map<std::string, midi_note> note_map = {
      {"D2", NOTES_D2}, {"D#2", NOTES_Ds2},
      {"E2", NOTES_E2}, {"F2", NOTES_F2}, {"F#2", NOTES_Fs2}, {"G2", NOTES_G2},
      {"G#2", NOTES_Gs2}, {"A2", NOTES_A2}, {"A#2", NOTES_As2}, {"B2", NOTES_B2},
      {"C3", NOTES_C3}, {"C#3", NOTES_Cs3}, {"D3", NOTES_D3}, {"D#3", NOTES_Ds3},
      {"E3", NOTES_E3}, {"F3", NOTES_F3}, {"F#3", NOTES_Fs3}, {"G3", NOTES_G3},
      {"G#3", NOTES_Gs3}, {"A3", NOTES_A3}, {"A#3", NOTES_As3}, {"B3", NOTES_B3},
      {"C4", NOTES_C4}, {"C#4", NOTES_Cs4}, {"D4", NOTES_D4}, {"D#4", NOTES_Ds4},
      {"E4", NOTES_E4}, {"F4", NOTES_F4}, {"F#4", NOTES_Fs4}, {"G4", NOTES_G4},
      {"G#4", NOTES_Gs4}, {"A4", NOTES_A4}, {"A#4", NOTES_As4}, {"B4", NOTES_B4},
      {"C5", NOTES_C5}, {"C#5", NOTES_Cs5}, {"D5", NOTES_D5}, {"D#5", NOTES_Ds5},
      {"E5", NOTES_E5}, {"F5", NOTES_F5}, {"F#5", NOTES_Fs5}, {"G5", NOTES_G5},
      {"G#5", NOTES_Gs5}, {"A5", NOTES_A5}, {"A#5", NOTES_As5},
   };

   auto it = note_map.find(token);
   if (it != note_map.end())
      return it->second;

   ERROR("Unknown note: " + token);
   return NOTES_E2; // fallback
}

static void parse_column_line(const std::string &line, column &col)
{
   std::istringstream iss(line);
   std::string bass_token, figures_token, answer_token;

   if (!(iss >> bass_token >> figures_token >> answer_token))
   {
      ERROR("Invalid column line: " + line);
      return;
   }

   col.bass.insert(parse_midi_note(bass_token));
   parse_figures_token(figures_token, col.figures);

   std::stringstream ass(answer_token);
   std::string ans_tok;
   while (std::getline(ass, ans_tok, ','))
   {
      col.answer.insert(parse_midi_note(ans_tok));
   }
}

void state_read_lesson(const std::string &filename, state *state)
{
   std::ifstream file(filename);
   if (!file.is_open())
   {
      ERROR("Cannot open file: " + filename);
      return;
   }

   // Reset state
   state->chords.clear();
   state->pressed_notes.clear();
   state->active_col = 0;

   // Parse metadata
   std::string line;
   while (std::getline(file, line))
   {
      if (line.empty())
         break;

      if (line.rfind("title:", 0) == 0)
      {
         // skip "title:" and leading spaces/tabs
         std::string trimmed = line.substr(6);
         trimmed.erase(0, trimmed.find_first_not_of(" \t"));

         // copy safely into buffer
         strncpy(state->lesson_title, trimmed.c_str(), sizeof(state->lesson_title) - 1);
         state->lesson_title[sizeof(state->lesson_title) - 1] = '\0'; // ensure null termination
      }
      else if (line.rfind("key:", 0) == 0)
      {
         std::string keystr = line.substr(4);
         keystr.erase(0, keystr.find_first_not_of(" \t"));
         state->key = parse_key(keystr);
      }
   }

   // Parse column lines
   while (std::getline(file, line))
   {
      if (line.empty())
         continue;

      column col;
      parse_column_line(line, col);
      state->chords.push_back(std::move(col));
   }
}
