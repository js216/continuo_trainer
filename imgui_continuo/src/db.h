// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file db.h
 * @brief Storing and loading persistent data.
 * @author Jakob Kastelic
 */

#ifndef DB_H
#define DB_H

void db_load(struct state *state);
void db_save(const struct state *state);
std::string db_lesson_fname(const int id);
void db_read_lesson(struct state *state);
void db_write_lesson(struct state *state);
int db_load_last_lesson_id(const char *fname);
void db_parse_figures_token(const std::string &token,
                         std::vector<figure> &figures);
void db_reload_stats(struct state *state);
void db_store_attempt(const int lesson_id, const struct column &col);

#endif /* DB_H */
