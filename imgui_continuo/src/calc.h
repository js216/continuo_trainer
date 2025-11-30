// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file calc.h
 * @brief Calculate statistics.
 * @author Jakob Kastelic
 */

#ifndef CALC_H
#define CALC_H

#include "theory.h"
#include <stddef.h>
#include <unordered_map>
#include <vector>

struct attempt_record;

// stats
void state_reload_stats(struct state *state);

int calc_lesson_streak(const std::vector<attempt_record> &attempts,
                       int lesson_id, size_t len);
double calc_duration_today(const std::vector<attempt_record> &records);
double calc_speed(const std::vector<attempt_record> &records,
                  const int lesson_id);
double calc_score_today(const std::vector<attempt_record> &records,
                        std::unordered_map<int, lesson_meta> &cache);
int calc_practice_streak(const std::vector<attempt_record> &records,
                         double score_goal,
                         std::unordered_map<int, lesson_meta> &cache);
double calc_difficulty(int lesson_id,
                       const std::vector<attempt_record> &records,
                       std::unordered_map<int, lesson_meta> &cache);
int calc_next(int current_id, const std::vector<int> &lesson_ids,
              const std::vector<attempt_record> &records,
              std::unordered_map<int, lesson_meta> &cache);

#endif /* CALC_H */
