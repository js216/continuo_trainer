// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file calc.cpp
 * @brief Calculate statistics.
 * @author Jakob Kastelic
 */

#include "calc.h"
#include "db.h"
#include "theory.h"
#include "time_utils.h"
#include <ctime>
#include <unordered_map>
#include <vector>

static double lesson_delta_seconds(const attempt_record &prev,
                                   const attempt_record &cur)
{
   if (cur.time <= prev.time)
      return 0.0;

   if (cur.col_id == 0) // new lesson
      return 0.0;

   return std::min(cur.time - prev.time, 5.0);
}

static void calc_day_totals(const std::vector<attempt_record> &recs,
                            std::vector<std::time_t> &days,
                            std::vector<double> &seconds)
{
   if (recs.empty())
      return;

   days.clear();
   seconds.clear();

   std::time_t curr_day = day_start(recs[0].time);
   days.push_back(curr_day);
   seconds.push_back(0.0);

   attempt_record prev = recs[0];

   for (size_t i = 1; i < recs.size(); ++i) {
      const auto &cur      = recs[i];
      const std::time_t ds = day_start(cur.time);

      if (ds != curr_day) {
         curr_day = ds;
         days.push_back(curr_day);
         seconds.push_back(0.0);
      }

      seconds.back() += lesson_delta_seconds(prev, cur);
      prev = cur;
   }
}

int calc_practice_streak(const std::vector<attempt_record> &recs, int goal_min)
{
   if (recs.size() < 2)
      return 0;

   std::vector<std::time_t> days;
   std::vector<double> sec_per_day;
   calc_day_totals(recs, days, sec_per_day);

   const double goal_sec = goal_min * 60.0;

   int streak = 0;
   for (int i = (int)sec_per_day.size() - 1; i >= 0; --i) {
      if (sec_per_day[i] < goal_sec)
         break;

      if (i != (int)sec_per_day.size() - 1 &&
          !is_consecutive_day(days[i + 1], days[i])) {
         break;
      }

      streak++;
   }

   return streak;
}

double calc_duration_today(const std::vector<attempt_record> &records)
{
   if (records.size() < 2)
      return 0.0;

   double sum_sec      = 0.0;
   attempt_record prev = records[0];

   for (size_t i = 1; i < records.size(); ++i) {
      const auto &cur = records[i];

      if (!time_is_today(prev.time) || !time_is_today(cur.time)) {
         prev = cur;
         continue;
      }

      sum_sec += lesson_delta_seconds(prev, cur);
      prev = cur;
   }

   return sum_sec;
}

int calc_lesson_streak(const std::vector<attempt_record> &attempts,
                       int lesson_id, size_t len)
{
   if (len == 0)
      return 0;

   int streak = 0;
   bool begin = false;

   for (const auto &attempt : attempts) {
      if (attempt.lesson_id != lesson_id)
         continue;

      if (attempt.col_id == 0)
         begin = true;

      if (begin && (attempt.col_id == len - 1))
         ++streak;

      if (attempt.bad_count != 0) {
         streak = 0;
         begin  = false;
      }
   }

   return streak;
}

double calc_speed(const std::vector<attempt_record> &records,
                  const int lesson_id)
{
   const double alpha = 2.0 / 6.0; // EMA roughly over last 5 attempts
   double ema         = 0.0;
   bool first_attempt = true;

   unsigned int last_col_id = 0;
   double last_time         = 0.0;
   double max_dt            = 0.0;
   bool first_in_attempt    = true;
   bool mistake_in_attempt  = false;

   for (const auto &rec : records) {
      if (rec.lesson_id != lesson_id)
         continue;

      // New lesson attempt if col_id resets
      if (!first_in_attempt && rec.col_id <= last_col_id) {
         if (!mistake_in_attempt) {
            // Update EMA with this attempt
            if (first_attempt) {
               ema           = max_dt;
               first_attempt = false;
            } else {
               ema = (alpha * max_dt) + ((1.0 - alpha) * ema);
            }
         }
         // Reset for next attempt
         max_dt             = 0.0;
         first_in_attempt   = true;
         mistake_in_attempt = false;
      }

      if (!first_in_attempt)
         max_dt = std::max(rec.time - last_time, max_dt);

      if (rec.bad_count > 0)
         mistake_in_attempt = true;

      last_time        = rec.time;
      last_col_id      = rec.col_id;
      first_in_attempt = false;
   }

   // Handle last attempt
   if (!first_in_attempt && !mistake_in_attempt) {
      if (first_attempt) {
         ema = max_dt;
      } else {
         ema = (alpha * max_dt) + ((1.0 - alpha) * ema);
      }
   }

   if (ema == 0)
      return 0;

   return 1 / ema;
}

// Fetch lesson metadata from cache or compute it from DB
static lesson_meta &get_lesson_meta(std::unordered_map<int, lesson_meta> &cache,
                                    int lesson_id)
{
   auto it = cache.find(lesson_id);
   if (it != cache.end())
      return it->second;

   std::vector<column> chords = db_load_lesson_chords(lesson_id);

   lesson_meta meta{};
   meta.lesson_id     = lesson_id;
   meta.total_columns = chords.size();
   meta.allowed_mistakes =
       static_cast<size_t>(meta.total_columns * 0.05); // 5% tolerance

   return cache[lesson_id] = std::move(meta);
}

// Compute the score for a single full lesson attempt
static double score_lesson_attempt(size_t good_count, size_t bad_count,
                                   double dt, const lesson_meta &meta)
{
   double good = static_cast<double>(good_count);
   double bad  = static_cast<double>(bad_count);

   if (bad <= meta.allowed_mistakes) {
      // Near-perfect: positive score with speed bonus
      double speed_multiplier = 1.0 / (0.3 + dt);
      speed_multiplier *= speed_multiplier;
      return good + good * speed_multiplier;
   }

   // Too many mistakes: negative score proportional to mistakes
   return -bad;
}

// Calculate total score for today by processing all records
double calc_score_today(const std::vector<attempt_record> &records,
                        std::unordered_map<int, lesson_meta> &cache)
{
   if (records.empty())
      return 0.0;

   double total_score = 0.0;

   size_t good_accum = 0;
   size_t bad_accum  = 0;
   double dt_accum   = 0.0;

   attempt_record last{};
   bool first_record  = true;
   int current_lesson = 0;

   auto finalize_attempt = [&](size_t good, size_t bad, double dt, int lesson) {
      if (lesson == 0)
         return 0.0; // nothing to score
      const lesson_meta &meta = get_lesson_meta(cache, lesson);
      return score_lesson_attempt(good, bad, dt, meta);
   };

   for (const auto &rec : records) {
      if (!time_is_today(rec.time))
         continue;

      bool is_new_attempt =
          (rec.col_id == 0 || rec.lesson_id != last.lesson_id);
      if (first_record || is_new_attempt) {
         // Score the previous attempt
         total_score +=
             finalize_attempt(good_accum, bad_accum, dt_accum, current_lesson);

         // Start new attempt
         good_accum     = rec.good_count;
         bad_accum      = rec.bad_count;
         dt_accum       = 0.0;
         current_lesson = rec.lesson_id;
         last           = rec;
         first_record   = false;
         continue;
      }

      // Accumulate ongoing attempt
      good_accum += rec.good_count;
      bad_accum += rec.bad_count;
      dt_accum += lesson_delta_seconds(last, rec);

      last = rec;
   }

   // Score the last attempt
   total_score +=
       finalize_attempt(good_accum, bad_accum, dt_accum, current_lesson);

   return total_score;
}
