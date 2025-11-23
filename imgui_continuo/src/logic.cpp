// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "state.h"
#include "util.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <vector>

void logic_clear(struct state *state)
{
   state->key = KEY_SIG_1_FLAT;
   state->pressed_notes.clear();
   state->chords.clear();
   state->active_col = 0;

   state_read_lesson("lessons/1.txt", state);
}

static bool logic_adjudicate(const struct column &col,
      enum midi_note realization)
{
   // If no bass exists yet, automatically bad
   if (col.bass.empty())
      return false;

   int realized_pc = static_cast<int>(realization) % 12;

   for (auto ans_note : col.answer)
   {
      int expected_pc = static_cast<int>(ans_note) % 12;
      if (realized_pc == expected_pc)
         return true;
   }

   return false;
}

static void process_note(struct state *state, midi_note realization)
{
   if (state->chords.empty())
      state->chords.emplace_back();

   if (state->active_col >= state->chords.size()) {
      ERROR("active_col out of range!");
      return;
   }

   column &col = state->chords[state->active_col];

   if (col.good.contains(realization) || col.bad.contains(realization))
      return;

   if (logic_adjudicate(col, realization))
      col.good.insert(realization);
   else
      col.bad.insert(realization);
}


static std::string midi_to_name(int midi)
{
   static const char *names[] = { "C", "C#", "D", "D#", "E",
      "F", "F#", "G", "G#", "A", "A#", "B" };
   int octave = (midi / 12) - 1;      // MIDI 0 = C-1
   int note   = midi % 12;
   return std::string(names[note]) + std::to_string(octave);
}

static std::string fig_to_string(const figure &f)
{
    std::stringstream ss;
    // Example: print accidental and number (adjust to your figure fields)
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

static void print_chord(const struct column &col)
{
    std::ofstream ofs("attempts.log", std::ios::app);
    if (!ofs.is_open()) {
        ERROR("Cannot open attempts.log");
        return;
    }

    // timestamp in seconds with 2 decimal places
    auto now = std::chrono::system_clock::now();
    auto dur = now.time_since_epoch();
    double ts = std::chrono::duration<double>(dur).count();
    ofs << std::fixed << std::setprecision(2) << ts << " ";

    // bass note (print lowest if multiple)
    if (!col.bass.empty()) {
        std::vector<midi_note> bass_notes(col.bass.begin(), col.bass.end());
        std::sort(bass_notes.begin(), bass_notes.end());
        ofs << midi_to_name(bass_notes.front()) << " ";
    } else {
        ofs << "- ";
    }

    // helper lambda to print comma-separated notes/figures
    auto print_notes = [&ofs](const auto &container) {
        bool first = true;
        for (auto n : container) {
            if (!first) ofs << ",";
            ofs << midi_to_name(n);
            first = false;
        }
        if (container.empty()) ofs << "-";
    };

    // figures (comma-delimited)
    bool first_fig = true;
    for (auto &fig : col.figures) {
        if (!first_fig) ofs << ",";
        ofs << fig_to_string(fig);  // you need a function to convert figure to string
        first_fig = false;
    }
    if (col.figures.empty()) ofs << "-";

    ofs << " ";

    // answer
    print_notes(col.answer);
    ofs << " ";

    // good
    print_notes(col.good);
    ofs << " ";

    // bad
    print_notes(col.bad);

    ofs << "\n";
}

void logic_receive(struct state *state)
{
   if (state->active_col >= state->chords.size())
      // TODO: lesson finished
      return;

   if (!state->pressed_notes.empty()) {
      // accumulate pressed notes into the current back column
      if (state->chords.empty())
         state->chords.emplace_back();

      for (auto note_val : state->pressed_notes)
         process_note(state, static_cast<midi_note>(note_val));

   } else {
      // all notes released → go to next column
      if (!state->chords.empty()) {
         column &col = state->chords[state->active_col];

         if (!col.good.empty() || !col.bad.empty()) {
            print_chord(col);
            state->active_col++;
         }
      }
   }
}

