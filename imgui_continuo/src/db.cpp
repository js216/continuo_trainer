// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file db.cpp
 * @brief Storing and loading persistent data.
 * @author Jakob Kastelic
 */

#include "state.h"
#include "theory.h"
#include "util.h"
#include "time_utils.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>

struct attempt_info {
   double time;
   int lesson_id;
   size_t good_count;
   size_t bad_count;
};

const char *config_file = "config.ini";
const char *attempts_file = "attempts.ini";

static void write_key_val(std::ofstream &f, const std::string &key,
                          const std::string &value)
{
   f << key << ": " << value << "\n";
}

static bool parse_key_val(const std::string &line, std::string &key,
                          std::string &value)
{
   auto pos = line.find(':');
   if (pos == std::string::npos)
      return false;

   key   = line.substr(0, pos);
   value = line.substr(pos + 1);

   // Trim whitespace
   auto trim = [](std::string &s) {
      s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                 return !std::isspace(ch);
              }));
      s.erase(std::find_if(s.rbegin(), s.rend(),
                           [](unsigned char ch) { return !std::isspace(ch); })
                  .base(),
              s.end());
   };
   trim(key);
   trim(value);

   return true;
}

void db_load(struct state *state)
{
   std::ifstream f(config_file);
   if (!f.is_open())
      return;

   std::string line;
   while (std::getline(f, line)) {
      if (line.empty() || line[0] == '#')
         continue;

      std::string key;
      std::string value;
      if (!parse_key_val(line, key, value))
         continue;

      if (key == "in_dev") {
         for (int i = 0; i < (int)state->midi_devices.size(); i++) {
            if (state->midi_devices[i] == value) {
               state->in_dev = i;
               break;
            }
         }
      } else if (key == "out_dev") {
         for (int i = 0; i < (int)state->midi_devices.size(); i++) {
            if (state->midi_devices[i] == value) {
               state->out_dev = i;
               break;
            }
         }
      } else if (key == "midi_forward") {
         std::transform(value.begin(), value.end(), value.begin(), ::tolower);
         state->midi_forward = (value == "1" || value == "true");
      }
   }
}

void db_save(const struct state *state)
{
   std::ofstream f(config_file);
   if (!f.is_open())
      return;

   write_key_val(
       f, "in_dev",
       (state->in_dev >= 0 && state->in_dev < (int)state->midi_devices.size())
           ? state->midi_devices[state->in_dev]
           : "");
   write_key_val(
       f, "out_dev",
       (state->out_dev >= 0 && state->out_dev < (int)state->midi_devices.size())
           ? state->midi_devices[state->out_dev]
           : "");
   write_key_val(f, "midi_forward", state->midi_forward ? "true" : "false");
}

static key_sig parse_key(const std::string &token)
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

   ERROR("Unknown key: " + token);
   return KEY_SIG_0; // fallback
}

void db_parse_figures_token(const std::string &token, std::vector<figure> &figures)
{
   if (token == "-")
      return;

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
         ERROR("Invalid figure number: " + part);

      figures.push_back(fig);
   }
}

static midi_note parse_midi_note(const std::string &token)
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

   ERROR("Unknown note: " + token);
   return NOTES_E2; // fallback
}

static void parse_column_line(const std::string &line, struct column &col)
{
   std::istringstream iss(line);
   std::string bass_token;
   std::string figures_token;
   std::string answer_token;

   if (!(iss >> bass_token >> figures_token >> answer_token)) {
      ERROR("Invalid column line: " + line);
      return;
   }

   col.bass.insert(parse_midi_note(bass_token));
   db_parse_figures_token(figures_token, col.figures);

   std::stringstream ass(answer_token);
   std::string ans_tok;
   while (std::getline(ass, ans_tok, ',')) {
      col.answer.insert(parse_midi_note(ans_tok));
   }
}

std::string db_lesson_fname(const int id)
{
   std::string fname = std::to_string(id);
   return "lessons/" + fname + ".txt";
}

void db_read_lesson(struct state *state)
{
   std::string fname = db_lesson_fname(state->lesson_id);
   std::ifstream file(fname);
   if (!file.is_open()) {
      ERROR("Cannot open file: " + fname);
      return;
   }

   // Reset state
   state_clear_lesson(state);

   // Parse metadata
   std::string line;
   while (std::getline(file, line)) {
      if (line.empty())
         break;

      if (line.rfind("title:", 0) == 0) {
         // skip "title:" and leading spaces/tabs
         std::string trimmed = line.substr(6);
         trimmed.erase(0, trimmed.find_first_not_of(" \t"));

         // copy safely into buffer
         strncpy(state->lesson_title, trimmed.c_str(),
                 sizeof(state->lesson_title) - 1);
         state->lesson_title[sizeof(state->lesson_title) - 1] =
             '\0'; // ensure null termination
      } else if (line.rfind("key:", 0) == 0) {
         std::string keystr = line.substr(4);
         keystr.erase(0, keystr.find_first_not_of(" \t"));
         state->key = parse_key(keystr);
      }
   }

   // Parse column lines
   while (std::getline(file, line)) {
      if (line.empty())
         continue;

      struct column col;
      parse_column_line(line, col);
      state->chords.push_back(std::move(col));
   }
}

void db_write_lesson(struct state *state)
{
   std::string fname = db_lesson_fname(state->lesson_id);
   std::ofstream out(fname);
   if (!out.is_open()) {
      state->status = "Failed to open file for writing: " + fname;
      return;
   }

   // Lesson title
   out << "title: " << state->lesson_title << "\n";

   // Key signature
   out << "key: " << key_sig_to_string(state->key) << "\n\n";

   for (const auto &col : state->chords) {
      if (col.bass.empty() || col.answer.empty())
         continue;

      // Bass notes sorted low→high, take the lowest
      std::vector<midi_note> bass_notes(col.bass.begin(), col.bass.end());
      std::sort(bass_notes.begin(), bass_notes.end());
      out << midi_to_name(bass_notes.front()) << " ";

      // Figures (if any)
      if (!col.figures.empty()) {
         for (size_t i = 0; i < col.figures.size(); ++i) {
            if (i > 0)
               out << ",";
            out << fig_to_string(col.figures[i]);
         }
         out << " ";
      } else {
         out << "- ";
      }

      // Full chord tones sorted
      std::vector<midi_note> notes(col.answer.begin(), col.answer.end());
      std::sort(notes.begin(), notes.end());
      for (size_t i = 0; i < notes.size(); ++i) {
         if (i > 0)
            out << ",";
         out << midi_to_name(notes[i]);
      }

      out << "\n";
   }

   out.close();
   state->status = "Lesson saved to " + fname;
}

int db_load_last_lesson_id(const char *fname)
{
   std::ifstream f(fname);
   if (!f.is_open())
      return 1; // default: first lesson

   double ts     = 0;
   int lesson_id = 0;
   std::string rest_of_line;
   int last_lesson_id = 0;

   while (f >> ts >> lesson_id) {
      // throw away the rest of the line
      std::getline(f, rest_of_line);
      last_lesson_id = lesson_id;
   }

   return last_lesson_id;
}

static bool parse_attempt_line(const std::string &line,
                               struct attempt_info &out)
{
   if (line.empty())
      return false;

   std::istringstream iss(line);

   // timestamp
   double t = 0.0;
   iss >> t;
   if (iss.fail())
      return false;
   out.time = t;

   // lesson id
   iss >> out.lesson_id;

   // skip bass, figures, answer
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

static int compute_lesson_streak(std::ifstream &f, int lesson_id,
                                 int chords_per_lesson)
{
   f.clear();
   f.seekg(0);

   std::string line;
   int streak         = 0; // number of consecutive full lessons
   int chords_correct = 0; // consecutive correct chords in current lesson
   int chords_counted = 0; // chords counted in current lesson

   while (std::getline(f, line)) {
      attempt_info cur{};
      if (!parse_attempt_line(line, cur))
         continue;
      if (!time_is_today(cur.time))
         continue;

      // Skip lines from other lessons
      if (cur.lesson_id != lesson_id)
         continue;

      chords_counted++;
      if (cur.bad_count == 0)
         chords_correct++;
      else
         chords_correct = 0; // any error in lesson resets chord streak

      // When a full lesson is complete
      if (chords_counted == chords_per_lesson) {
         if (chords_correct == chords_per_lesson)
            streak++; // perfect lesson → increment streak
         else
            streak = 0; // mistakes → reset streak

         // reset counters for next lesson
         chords_counted = 0;
         chords_correct = 0;
      }
   }

   return streak;
}



static int compute_practice_streak()
{
   std::ifstream f(attempts_file);
   if (!f.is_open())
      return 0;

   // map from day (YYYYMMDD) -> total duration
   std::map<int, double> duration_by_day;

   std::string line;
   attempt_info last{-1.0, 0, 0, 0};

   while (std::getline(f, line)) {
      attempt_info cur{};
      if (!parse_attempt_line(line, cur))
         continue;

      // Convert cur.time (unix timestamp) to YYYYMMDD integer
      auto t     = static_cast<std::time_t>(cur.time);
      std::tm tm = *std::localtime(&t);
      int yyyymmdd =
          (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;

      if (last.time >= 0.0 && static_cast<int>((std::time_t)last.time) !=
                                  static_cast<int>((std::time_t)cur.time)) {
         double dt = cur.time - last.time;
         if (dt > 5.0)
            dt = 5.0;
         duration_by_day[yyyymmdd] += dt;
      }

      last = cur;
   }

   // Count consecutive days from today backwards where duration > 600s
   int streak         = 0;
   std::time_t now    = std::time(nullptr);
   std::tm tm_now     = *std::localtime(&now);
   int today_yyyymmdd = (tm_now.tm_year + 1900) * 10000 +
                        (tm_now.tm_mon + 1) * 100 + tm_now.tm_mday;

   for (int day = today_yyyymmdd;; --day) {
      auto it = duration_by_day.find(day);
      if (it == duration_by_day.end() || it->second < 600.0)
         break;
      streak++;
      // decrement day manually
      std::tm t       = {};
      t.tm_year       = day / 10000 - 1900;
      t.tm_mon        = (day / 100) % 100 - 1;
      t.tm_mday       = day % 100;
      std::time_t tt  = std::mktime(&t) - 24UL * 3600UL; // go back 1 day
      std::tm tm_prev = *std::localtime(&tt);
      day = (tm_prev.tm_year + 1900) * 10000 + (tm_prev.tm_mon + 1) * 100 +
            tm_prev.tm_mday + 1; // +1 because loop will decrement
   }

   return streak;
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


void db_reload_stats(struct state *state)
{
   state->duration_today = 0.0;
   state->score          = 0.0;
   state->lesson_streak  = 0;

   std::ifstream f(attempts_file);
   if (!f.is_open())
      return;

   std::string line;
   struct attempt_info last{-1.0, 0, 0, 0};

   while (std::getline(f, line)) {
      struct attempt_info cur{};
      if (!parse_attempt_line(line, cur))
         continue;
      if (!time_is_today(cur.time))
         continue;

      if (last.time >= 0.0) {
         double dt = cur.time - last.time;
         accumulate_duration_today(state, dt);
         state->score += score_formula(static_cast<double>(dt), cur.good_count,
                                       cur.bad_count);
      }

      last = cur;
   }

   // Compute most recent streak for the current lesson
   int chords_per_lesson = (int)state->chords.size();
   state->lesson_streak =
       compute_lesson_streak(f, state->lesson_id, chords_per_lesson);

   // Practice streak
   state->practice_streak = compute_practice_streak();
}

void db_store_attempt(const int lesson_id, const struct column &col)
{
   std::ofstream ofs(attempts_file, std::ios::app);
   if (!ofs.is_open()) {
      ERROR("Cannot open attempts file");
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
