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
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>

#define MAX_ATTEMPT_GAP 5.0f

static void logic_reload_stats(struct state *state)
{
    using namespace std::chrono;

    state->duration_today = 0.0f;

    std::ifstream f("attempts.log");
    if (!f.is_open())
        return;

    std::string line;
    double last_time = -1.0;

    while (std::getline(f, line))
    {
        if (line.empty())
            continue;

        std::istringstream iss(line);
        double t = 0.0;
        iss >> t;
        if (iss.fail() || !state_is_today(t))
            continue;

        if (last_time >= 0.0)
        {
            double diff = t - last_time;
            if (diff > 0.0)
            {
                if (diff > MAX_ATTEMPT_GAP)
                    diff = MAX_ATTEMPT_GAP;
                state->duration_today += static_cast<float>(diff);
            }
        }

        last_time = t;
    }
}

void logic_clear(struct state *state)
{
   if (state->lesson_id <= 0)
      state->lesson_id = 1;

   if (std::filesystem::exists(state_lesson_fname(state->lesson_id))) {
      // load lesson
      state->edit_lesson = false;
      state_read_lesson(state);
      state->status = "Loaded lesson " + std::to_string(state->lesson_id);
   } else {
      // enter edit mode
      state_clear_lesson(state);
      state->edit_lesson = true;
   }

   logic_reload_stats(state);
}

static bool logic_adjudicate(const struct column &col,
      enum midi_note realization)
{
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

   struct column &col = state->chords[state->active_col];

   if (col.good.contains(realization) || col.bad.contains(realization))
      return;

   if (logic_adjudicate(col, realization))
      col.good.insert(realization);
   else
      col.bad.insert(realization);
}


static void print_chord(const struct column &col)
{
   std::ofstream ofs("attempts.log", std::ios::app);
   if (!ofs.is_open()) {
      ERROR("Cannot open attempts.log");
      return;
   }

   // timestamp in seconds with 2 decimal places
   ofs << std::fixed << std::setprecision(2) << state_time() << " ";

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

static float last_chord_duration(struct state *state)
{
   if (state->active_col >= state->chords.size())
      return 0.0F;

   const int prev_idx = state->active_col - 1;
   if (prev_idx >= 0) {
      const struct column &col = state->chords[state->active_col];
      const struct column &prev_col = state->chords[prev_idx];
      const float dt = col.time - prev_col.time;
      return dt;
   }
   return 0.0F;
}

static void score_chord(struct state *state)
{
   if (state->active_col >= state->chords.size())
      return;

   const struct column &col = state->chords[state->active_col];

   // note accuracy
   state->score += col.good.size();
   state->score -= col.bad.size();

   // speed
   const float dt = last_chord_duration(state);
   state->duration_today += dt;
   if (dt < 0)
      ERROR("Negative time difference");
   else if (dt < 1.0F)
      state->score += 5 * col.good.size();
   else if (dt < 3.0F)
      state->score += 1 * col.good.size();
}

static void logic_play(struct state *state)
{
   // cannot play without chords
   if (state->chords.empty())
      return;

   // lesson finished
   if (state->active_col >= state->chords.size()) {
      logic_clear(state);
      state->status = "Done!";
      return;
   }

   // all notes released : go to next column
   if (state->pressed_notes.empty()) {
      struct column &col = state->chords[state->active_col];

      if (!col.good.empty() || !col.bad.empty()) {
         col.time = state_time();
         score_chord(state);
         print_chord(col);
         state->active_col++;
      }
   }

   else {
      // accumulate pressed notes into the current back column
      if (state->chords.empty())
         state->chords.emplace_back();

      for (auto note_val : state->pressed_notes)
         process_note(state, static_cast<midi_note>(note_val));

   }
}

static void logic_record(struct state *state)
{
   // If nothing pressed → finalize column if anything was recorded
   if (state->pressed_notes.empty())
   {
      if (state->active_col < state->chords.size())
      {
         struct column &col = state->chords[state->active_col];
         if (!col.bass.empty() || !col.answer.empty())
         {
            // Finalize this column, move to the next
            state->active_col++;
         }
      }
      return;
   }

   // Notes currently pressed → ensure there is a column to record into
   if (state->chords.empty() ||
         state->active_col >= state->chords.size())
   {
      state->chords.emplace_back();
   }

   struct column &col = state->chords[state->active_col];

   // Determine lowest pressed note
   unsigned char lowest = state->pressed_notes[0];
   for (auto n : state->pressed_notes)
   {
      if (n < lowest)
         lowest = n;
   }

   // Insert into bass + answer sets
   col.bass.insert(static_cast<midi_note>(lowest));
   for (auto n : state->pressed_notes)
   {
      if (n != lowest)
         col.answer.insert(static_cast<midi_note>(n));
   }
}

void logic_receive(struct state *state)
{
   if (state->edit_lesson)
      logic_record(state);
   else
      logic_play(state);
}
