// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file calc.cpp
 * @brief Calculate statistics.
 * @author Jakob Kastelic
 */

#include "calc.h"
#include "time_utils.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>

static double calc_time_delta(double prev_time, double cur_time)
{
   constexpr double max_gap = 5.0;

   if (cur_time <= prev_time)
      return 0.0;

   double dt = cur_time - prev_time;
   if (dt > max_gap)
      dt = max_gap;

   return dt;
}

static double calc_score(double dt, size_t good_count, size_t bad_count)
{
   auto good = static_cast<double>(good_count);
   auto bad  = static_cast<double>(bad_count);

   double score = good - 1.5F * bad;

   if (score > 0.0F) {
      double speed_multiplier = 1.0F / (0.3F + dt);
      speed_multiplier *= speed_multiplier;

      score += score * speed_multiplier;
   }

   return score;
}

int calc_lesson_streak(const std::vector<attempt_record> &attempts,
                       int lesson_id, size_t chords_per_lesson)
{
   int streak = 0;

   // make a copy and reverse it
   std::vector<attempt_record> reversed_attempts = attempts;
   std::reverse(reversed_attempts.begin(), reversed_attempts.end());

   for (const auto &attempt : reversed_attempts) {
      if (attempt.lesson_id != lesson_id)
         continue;

      if (attempt.bad_count == 0 && attempt.good_count >= chords_per_lesson) {
         ++streak;
      } else {
         break; // streak ends at first failure
      }
   }

   return streak;
}

int calc_practice_streak(const std::vector<attempt_record> &attempts)
{
   int streak           = 0;
   std::time_t last_day = 0;

   // make a copy and reverse it
   std::vector<attempt_record> reversed_attempts = attempts;
   std::reverse(reversed_attempts.begin(), reversed_attempts.end());

   for (const auto &attempt : reversed_attempts) {
      auto attempt_time = static_cast<std::time_t>(attempt.time);

      if (last_day != 0) {
         std::tm tm_last = *std::localtime(&last_day);
         std::tm tm_curr = *std::localtime(&attempt_time);

         // skip multiple attempts on the same day
         if (tm_curr.tm_yday == tm_last.tm_yday &&
             tm_curr.tm_year == tm_last.tm_year) {
            continue;
         }

         // if days are not consecutive, streak ends
         if ((last_day - attempt_time) > 24L * 60 * 60) {
            break;
         }
      }

      last_day = attempt_time;
      ++streak;
   }

   return streak;
}

double calc_duration_today(const std::vector<attempt_record> &records)
{
   if (records.size() < 2)
      return 0.0;

   double duration     = 0.0;
   attempt_record last = records[0];

   for (size_t i = 1; i < records.size(); ++i) {
      const auto &cur = records[i];

      if (!time_is_today(cur.time) || !time_is_today(last.time)) {
         last = cur;
         continue;
      }

      double dt = calc_time_delta(last.time, cur.time);
      duration += dt;

      last = cur;
   }
   return duration;
}

double calc_score_today(const std::vector<attempt_record> &records)
{
   if (records.size() < 2)
      return 0.0;

   double score        = 0.0;
   attempt_record last = records[0];

   for (size_t i = 1; i < records.size(); ++i) {
      const auto &cur = records[i];

      if (!time_is_today(cur.time) || !time_is_today(last.time)) {
         last = cur;
         continue;
      }

      double dt = calc_time_delta(last.time, cur.time);
      score += calc_score(dt, cur.good_count, cur.bad_count);

      last = cur;
   }
   return score;
}
