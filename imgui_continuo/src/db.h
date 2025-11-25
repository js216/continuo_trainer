// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file db.h
 * @brief Storing and loading persistent data.
 * @author Jakob Kastelic
 */

#ifndef DB_H
#define DB_H

#include "theory.h"
#include <string>
#include <vector>

struct attempt_record {
   double time;
   size_t good_count;
   size_t bad_count;
   int lesson_id;
};

// settings key:value pairs
void db_store_key_val(const std::string &key, const std::string &value);
std::string db_load_key_val(const std::string &wanted_key);

// settings booleands
bool db_load_bool(const std::string &key);
void db_store_bool(const std::string &key, bool v);

// lesson general
bool db_lesson_exists(int lesson_id);
void db_clear_lesson(int lesson_id);

// lesson key:value pairs
std::string db_load_lesson_key_val(int lesson_id, const std::string &key);
void db_store_lesson_key_val(int lesson_id, const std::string &key,
                             const std::string &value);

// lesson chords
std::vector<struct column> db_load_lesson_chords(int lesson_id);
void db_store_lesson_chords(int lesson_id, const std::vector<column> &chords);

// attempts
int db_load_last_lesson_id(void);
std::vector<struct attempt_record> db_read_attempts(void);
void db_store_attempt(const int lesson_id, const struct column &col);

#endif /* DB_H */
