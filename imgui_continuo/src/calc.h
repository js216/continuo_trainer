// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file calc.h
 * @brief Calculate statistics using a streaming API.
 * @author Jakob Kastelic
 */

#ifndef CALC_H
#define CALC_H

#include <ctime>
#include <unordered_map>
#include <vector>

struct attempt_record {
   int lesson_id;
   unsigned int col_id;
   double time;
   size_t good_count;
   size_t bad_count;
};

struct lesson_meta {
   int lesson_id;
   size_t total_columns;
   size_t allowed_mistakes;

   // Performance metrics
   int streak;
   double speed; // EMA of max_dt

   // SRS (Spaced Repetition) state
   double srs_ease;     // Multiplier (e.g., 2.5)
   double srs_interval; // In seconds
   std::time_t srs_due; // Unix timestamp

   // Working state for the current active attempt
   bool in_progress;
   unsigned int last_col_id;
   double last_time;
   double working_max_dt;
   size_t working_good;
   size_t working_bad;
   double working_duration;
};

struct stats {
   // Today's metrics
   double score_today;
   double duration_today;

   // Overall metrics
   int practice_streak;
   std::time_t last_practice_date;
   bool goal_met_today;

   // Global streaming state (to detect lesson switches/abandonment)
   attempt_record last_record;
   bool has_last_record;

   // Lesson metadata cache
   std::unordered_map<int, lesson_meta> lesson_cache;
};

// Data acess
void create_lesson_meta(struct stats &stats, int lesson_id, int len);
struct lesson_meta &calc_get_lesson_meta(struct stats &stats, int lesson_id);

// Streaming API
void calc_speed(struct stats &stats, const struct attempt_record &r);
void calc_lesson_streak(struct stats &stats, const struct attempt_record &r);
void calc_duration(struct stats &stats, const struct attempt_record &r);
void calc_score(struct stats &stats, const struct attempt_record &r);
void calc_practice_streak(struct stats &stats, const struct attempt_record &r,
                          double score_goal);

// SRS & Scheduling
void calc_schedule(struct stats &stats, const struct attempt_record &r);
int calc_next(int current_id, const std::vector<int> &lesson_ids,
              struct stats &stats);

#endif /* CALC_H */
