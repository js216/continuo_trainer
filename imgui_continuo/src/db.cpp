// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file db.cpp
 * @brief Storing and loading persistent data.
 * @author Jakob Kastelic
 */

#include "db.h"
#include "theory.h"
#include "time_utils.h"
#include "util.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

const char *config_file   = "settings.ini";
const char *attempts_file = "attempts.log";

static std::string escape_value(const std::string &s)
{
   std::string out;
   out.reserve(s.size());
   for (char c : s) {
      if (c == '\n')
         out += "\\n";
      else if (c == '\r')
         out += "\\r";
      else if (c == '\\')
         out += "\\\\";
      else
         out += c;
   }
   return out;
}

static std::string unescape_value(const std::string &s)
{
   std::string out;
   out.reserve(s.size());
   for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '\\' && i + 1 < s.size()) {
         char n = s[i + 1];
         if (n == 'n') {
            out += '\n';
            i++;
            continue;
         }
         if (n == 'r') {
            out += '\r';
            i++;
            continue;
         }
         if (n == '\\') {
            out += '\\';
            i++;
            continue;
         }
      }
      out += s[i];
   }
   return out;
}

void db_store_key_val(const std::string &key, const std::string &value)
{
   const std::string safe_value = escape_value(value);

   // Read entire existing file
   std::ifstream fin(config_file);
   std::vector<std::string> lines;

   if (fin.is_open()) {
      std::string line;
      while (std::getline(fin, line)) {
         // Keep only lines that don't match the key:
         if (!(line.rfind(key + ":", 0) == 0)) {
            lines.push_back(line);
         }
      }
   }
   fin.close();

   // Append new entry
   lines.push_back(key + ": " + safe_value);

   // Rewrite file
   std::ofstream fout(config_file, std::ios::trunc);
   if (!fout.is_open())
      return;

   for (const auto &l : lines)
      fout << l << "\n";
}

std::string db_load_key_val(const std::string &wanted_key)
{
   std::ifstream f(config_file);
   if (!f.is_open())
      return "";

   std::string line;
   while (std::getline(f, line)) {
      if (line.empty() || line[0] == '#')
         continue;

      auto pos = line.find(':');
      if (pos == std::string::npos)
         continue;

      std::string key   = line.substr(0, pos);
      std::string value = line.substr(pos + 1);

      // trim left
      while (!value.empty() && std::isspace((unsigned char)value.front()))
         value.erase(value.begin());
      // trim right
      while (!value.empty() && std::isspace((unsigned char)value.back()))
         value.pop_back();

      if (key == wanted_key)
         return unescape_value(value);
   }

   return "";
}

bool db_load_bool(const std::string &key)
{
   const bool default_value = false;
   std::string v            = db_load_key_val(key);
   if (v.empty())
      return default_value;

   std::transform(v.begin(), v.end(), v.begin(), ::tolower);
   return (v == "1" || v == "true" || v == "yes" || v == "on");
}

void db_store_bool(const std::string &key, bool v)
{
   db_store_key_val(key, v ? "true" : "false");
}

void db_store_int(const std::string &key, int v)
{
   db_store_key_val(key, std::to_string(v));
}

int db_load_int(const std::string &key)
{
    std::string v = db_load_key_val(key);
    if (v.empty())
        return 0;

    try {
        return std::stoi(v);
    }
    catch (...) {
        return 0;
    }
}

static std::string db_lesson_fname(const int id)
{
   std::string fname = std::to_string(id);
   return "lessons/" + fname + ".txt";
}

bool db_lesson_exists(int lesson_id)
{
   return std::filesystem::exists(db_lesson_fname(lesson_id));
}

void db_clear_lesson(int lesson_id)
{
   // Open the file in truncation mode to erase everything
   // File will be closed automatically when 'out' goes out of scope
   std::ofstream out(db_lesson_fname(lesson_id), std::ios::trunc);
}

static void parse_column_line(const std::string &line, struct column &col)
{
   std::istringstream iss(line);
   std::string bass_token;
   std::string figures_token;
   std::string answer_token;

   if (!(iss >> bass_token >> figures_token >> answer_token)) {
      error("Invalid column line: " + line);
      return;
   }

   col.bass.insert(th_parse_midi_note(bass_token));
   col.figures = th_parse_figures_from_str(figures_token);

   std::stringstream ass(answer_token);
   std::string ans_tok;
   while (std::getline(ass, ans_tok, ',')) {
      col.answer.insert(th_parse_midi_note(ans_tok));
   }
}

std::string db_load_lesson_key_val(int lesson_id, const std::string &key)
{
   std::ifstream file(db_lesson_fname(lesson_id));
   if (!file.is_open())
      return {};

   std::string line;
   while (std::getline(file, line)) {
      if (line.empty())
         break;
      if (line.rfind(key + ":", 0) == 0) {
         std::string val = line.substr(key.size() + 1);
         val.erase(0, val.find_first_not_of(" \t")); // trim leading spaces/tabs
         return val;
      }
   }

   return {};
}

std::vector<column> db_load_lesson_chords(int lesson_id)
{
   std::vector<column> chords;
   std::ifstream file(db_lesson_fname(lesson_id));
   if (!file.is_open())
      return chords;

   std::string line;
   bool in_columns = false;

   while (std::getline(file, line)) {
      if (!in_columns) {
         if (line.empty()) {
            in_columns = true; // metadata finished
         }
         continue;
      }

      if (!line.empty()) {
         column col;
         parse_column_line(line, col);
         chords.push_back(std::move(col));
      }
   }

   return chords;
}

void db_store_lesson_key_val(int lesson_id, const std::string &key,
                             const std::string &value)
{
   std::ofstream out(db_lesson_fname(lesson_id), std::ios::app);
   if (!out.is_open())
      return;

   out << key << ": " << value << "\n";
}

void db_store_lesson_chords(int lesson_id, const std::vector<column> &chords)
{
   std::ofstream out(db_lesson_fname(lesson_id), std::ios::app);
   if (!out.is_open())
      return;

   out << "\n"; // separate metadata from chords

   for (const auto &col : chords) {
      if (col.bass.empty() || col.answer.empty())
         continue;

      std::vector<midi_note> bass_notes(col.bass.begin(), col.bass.end());
      std::sort(bass_notes.begin(), bass_notes.end());
      out << th_midi_to_name(bass_notes.front()) << " ";

      out << th_fig_to_string(col.figures);

      std::vector<midi_note> notes(col.answer.begin(), col.answer.end());
      std::sort(notes.begin(), notes.end());
      for (size_t i = 0; i < notes.size(); ++i) {
         if (i > 0)
            out << ",";
         out << th_midi_to_name(notes[i]);
      }

      out << "\n";
   }
}

int db_load_last_lesson_id(void)
{
   std::ifstream f(attempts_file);
   if (!f.is_open())
      return 1; // default: first lesson

   int lesson_id = 0;
   std::string rest_of_line;
   int last_lesson_id = 0;

   while (f >> lesson_id) {
      // throw away the rest of the line
      std::getline(f, rest_of_line);
      last_lesson_id = lesson_id;
   }

   return last_lesson_id;
}

static bool parse_attempt_line(const std::string &line, attempt_record &out)
{
   if (line.empty())
      return false;

   std::istringstream iss(line);

   // lesson id
   iss >> out.lesson_id;

   // column id
   iss >> out.col_id;

   // timestamp
   double t = 0.0;
   iss >> t;
   if (iss.fail())
      return false;
   out.time = t;

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

std::vector<attempt_record> db_read_attempts()
{
   std::vector<attempt_record> records;
   std::ifstream file(attempts_file);
   if (!file.is_open())
      return records;

   std::string line;
   while (std::getline(file, line)) {
      attempt_record rec{};
      if (parse_attempt_line(line, rec)) {
         records.push_back(rec);
      }
   }

   return records;
}

void db_store_attempt(const int lesson_id, unsigned int col_id, const struct column &col)
{
   std::ofstream ofs(attempts_file, std::ios::app);
   if (!ofs.is_open()) {
      error("Cannot open attempts file");
      return;
   }

   // lesson id
   ofs << lesson_id << " ";

   // column id
   ofs << col_id << " ";

   // timestamp in seconds with 2 decimal places
   ofs << std::fixed << std::setprecision(2) << time_now() << " ";

   // bass note (print lowest if multiple)
   if (!col.bass.empty()) {
      std::vector<midi_note> bass_notes(col.bass.begin(), col.bass.end());
      std::sort(bass_notes.begin(), bass_notes.end());
      ofs << th_midi_to_name(bass_notes.front()) << " ";
   } else {
      ofs << "- ";
   }

   // helper lambda to print comma-separated notes/figures
   auto print_notes = [&ofs](const auto &container) {
      bool first = true;
      for (auto n : container) {
         if (!first)
            ofs << ",";
         ofs << th_midi_to_name(n);
         first = false;
      }
      if (container.empty())
         ofs << "-";
   };

   // figures (comma-delimited)
   ofs << th_fig_to_string(col.figures);

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
