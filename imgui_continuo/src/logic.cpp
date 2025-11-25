// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file logic.cpp
 * @brief Logic for deciding what notes to use.
 * @author Jakob Kastelic
 */

#include "logic.h"
#include "state.h"
#include "time_utils.h"
#include "util.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct attempt_info {
   double time;
   int lesson_id;
   size_t good_count;
   size_t bad_count;
};

static bool parse_attempt_line(const std::string &line,
                               struct attempt_info &out)
{
   if (line.empty())
      return false;

   std::istringstream iss(line);

   // timestamp
   double t = 0.0;
   iss >> t;
   if (iss.fail() || !time_is_today(t))
      return false;

   // lesson id
   iss >> out.lesson_id;

   // skip the next three fields (bass, figures, answer)
   std::string ignore;
   for (int i = 0; i < 3 && iss; ++i)
      iss >> ignore;

   // good notes
   std::string good_notes;
   iss >> good_notes;
   out.good_count =
       (good_notes == "-")
           ? 0
           : std::count(good_notes.begin(), good_notes.end(), ',') + 1;

   // bad notes
   std::string bad_notes;
   iss >> bad_notes;
   out.bad_count =
       (bad_notes == "-")
           ? 0
           : std::count(bad_notes.begin(), bad_notes.end(), ',') + 1;

   out.time = t;
   return true;
}

static void accumulate_duration_today(struct state *state, double dt)
{
   constexpr double max_attempt_gap = 5.0F;

   if (dt > 0.0) {
      if (dt > max_attempt_gap)
         dt = max_attempt_gap;
      state->duration_today += static_cast<double>(dt);
   }
}

static double score_formula(double dt, size_t good_count, size_t bad_count)
{
   auto good = static_cast<double>(good_count);
   auto bad  = static_cast<double>(bad_count);

   // base accuracy penalty
   double score = good - 1.5F * bad;

   if (score > 0.0F) {
      // only reward speed if net accuracy positive
      double speed_multiplier = 1.0F / (0.3F + dt);
      speed_multiplier *= speed_multiplier;

      score += score * speed_multiplier; // amplify only positive base
   }

   return score;
}

static void logic_reload_stats(struct state *state)
{
   state->duration_today = 0.0F;
   state->score          = 0.0F;

   std::ifstream f("attempts.log");
   if (!f.is_open())
      return;

   std::string line;
   struct attempt_info last{-1.0, 0, 0, 0};

   while (std::getline(f, line)) {
      struct attempt_info cur = {};
      if (!parse_attempt_line(line, cur))
         continue;

      if (last.time >= 0.0) {
         double dt = cur.time - last.time;
         accumulate_duration_today(state, dt);
         state->score += score_formula(static_cast<double>(dt), cur.good_count,
                                       cur.bad_count);
      }

      last = cur;
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

   auto matches_pc = [realized_pc](auto n) {
      return realized_pc == (int(n) % 12);
   };

   return std::ranges::any_of(col.answer, matches_pc);
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

static void print_chord(const int lesson_id, const struct column &col)
{
   std::ofstream ofs("attempts.log", std::ios::app);
   if (!ofs.is_open()) {
      ERROR("Cannot open attempts.log");
      return;
   }

   // timestamp in seconds with 2 decimal places
   ofs << std::fixed << std::setprecision(2) << time_now() << " ";

   // lesson id
   ofs << lesson_id << " ";

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
         if (!first)
            ofs << ",";
         ofs << midi_to_name(n);
         first = false;
      }
      if (container.empty())
         ofs << "-";
   };

   // figures (comma-delimited)
   bool first_fig = true;
   for (const auto &fig : col.figures) {
      if (!first_fig)
         ofs << ",";
      ofs << fig_to_string(
          fig); // you need a function to convert figure to string
      first_fig = false;
   }
   if (col.figures.empty())
      ofs << "-";

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

static double last_chord_duration(struct state *state)
{
   if (state->active_col >= state->chords.size())
      return 0.0F;

   if (state->active_col > 0) {
      const unsigned int prev_idx   = state->active_col - 1;
      const struct column &col      = state->chords[state->active_col];
      const struct column &prev_col = state->chords[prev_idx];
      return col.time - prev_col.time;
   }
   return 0.0F;
}

static void score_chord(struct state *state)
{
   if (state->active_col >= state->chords.size())
      return;

   const struct column &col = state->chords[state->active_col];

   const double dt = last_chord_duration(state);
   accumulate_duration_today(state, dt);

   state->score += score_formula(dt, col.good.size(), col.bad.size());
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
         col.time = time_now();
         score_chord(state);
         print_chord(state->lesson_id, col);
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
   if (state->pressed_notes.empty()) {
      if (state->active_col < state->chords.size()) {
         struct column &col = state->chords[state->active_col];
         if (!col.bass.empty() || !col.answer.empty()) {
            // Finalize this column, move to the next
            state->active_col++;
         }
      }
      return;
   }

   // Notes currently pressed → ensure there is a column to record into
   if (state->chords.empty() || state->active_col >= state->chords.size()) {
      state->chords.emplace_back();
   }

   struct column &col = state->chords[state->active_col];

   // Determine lowest pressed note
   unsigned char lowest = *std::min_element(state->pressed_notes.begin(),
                                            state->pressed_notes.end());

   // Insert into bass + answer sets
   col.bass.insert(static_cast<midi_note>(lowest));
   for (auto n : state->pressed_notes) {
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
