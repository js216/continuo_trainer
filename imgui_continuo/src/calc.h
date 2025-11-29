// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file calc.h
 * @brief Calculate statistics.
 * @author Jakob Kastelic
 */

#ifndef CALC_H
#define CALC_H

#include <stddef.h>
#include <vector>

struct attempt_record;

int calc_lesson_streak(const std::vector<attempt_record> &attempts,
                       int lesson_id, size_t len);
int calc_practice_streak(const std::vector<attempt_record> &recs, int goal_min);
double calc_duration_today(const std::vector<attempt_record> &records);
double calc_score_today(const std::vector<attempt_record> &records);
double calc_speed(const std::vector<attempt_record> &records,
                  const int lesson_id);

#endif /* CALC_H */
