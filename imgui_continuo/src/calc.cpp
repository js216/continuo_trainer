// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file calc.cpp
 * @brief Calculate statistics.
 * @author Jakob Kastelic
 */

#include "calc.h"
#include "time_utils.h"
#include "util.h"
#include <algorithm>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <cmath>

void calc_create_lesson_meta(struct stats &stats, int lesson_id,
                             std::size_t len)
{
   lesson_meta meta{};

   meta.lesson_id     = lesson_id;
   meta.total_columns = len;
   meta.allowed_mistakes =
       static_cast<size_t>(static_cast<double>(meta.total_columns) * 0.1F);

   meta.streak = 0;
   meta.speed  = 0.0;

   meta.srs_ease     = 2.5;
   meta.srs_interval = 0.0;
   meta.srs_due      = 0;

   meta.in_progress      = false;
   meta.last_col_id      = 0;
   meta.last_time        = 0.0;
   meta.working_max_dt   = 0.0;
   meta.working_good     = 0;
   meta.working_bad      = 0;
   meta.working_missed   = 0;
   meta.working_duration = 0.0;

   stats.lesson_cache.emplace(lesson_id, meta);
}

struct lesson_meta &calc_get_lesson_meta(struct stats &stats, int lesson_id)
{
   auto it = stats.lesson_cache.find(lesson_id);
   if (it != stats.lesson_cache.end())
      return it->second;

   static lesson_meta dummy{}; // fallback reference
   error("Lesson not found in cache");
   return dummy;
}

static double record_delta_seconds(const attempt_record &prev,
                                   const attempt_record &cur)
{
   if (cur.time <= prev.time)
      return 0.0;
   // If it's a new lesson or restart, there's no delta from previous
   if (cur.col_id == 0 || cur.lesson_id != prev.lesson_id)
      return 0.0;

   return std::min(cur.time - prev.time, 5.0);
}

static void calc_duration(struct stats &stats, const struct attempt_record &r)
{
   if (!stats.has_last_record) {
      stats.last_record     = r;
      stats.has_last_record = true;
      return;
   }

   // Only add duration if both records are from today
   if (time_is_today(stats.last_record.time) && time_is_today(r.time)) {
      stats.duration_today += record_delta_seconds(stats.last_record, r);
   }
}

static void calc_speed(struct stats &stats, const struct attempt_record &r)
{
   auto &meta = calc_get_lesson_meta(stats, r.lesson_id);

   if (meta.lives_left < 1)
      return;

   // Determine dt from the previous record *within this lesson attempt*
   double dt = 0.0;
   if (meta.in_progress && r.col_id > meta.last_col_id) {
      dt = std::min(r.time - meta.last_time, 5.0);
   }

   // Update max_dt for this specific attempt
   if (r.col_id == 0) {
      meta.working_max_dt = 0.0; // Reset on start
   } else {
      meta.working_max_dt = std::max(meta.working_max_dt, dt);
   }

   // If lesson finished, update the EMA
   if (r.col_id == meta.total_columns - 1) {
      const double alpha = 2.0 / 6.0; // EMA over ~5 attempts
      meta.speed =
          (alpha * 1 / meta.working_max_dt) + ((1.0 - alpha) * meta.speed);
   }
}

static void apply_mistake_penalty(struct stats &stats, const attempt_record &r)
{
   if (r.bad_count > 0)
      stats.score_today -= static_cast<double>(r.bad_count);

   if (r.missed_count > 0)
      stats.score_today -= static_cast<double>(r.missed_count);
}

void calc_reset_working_state(struct lesson_meta &meta)
{
   meta.working_good     = 0;
   meta.working_bad      = 0;
   meta.working_missed   = 0;
   meta.working_duration = 0.0;
   meta.lives_left = std::max(1, static_cast<int>(meta.allowed_mistakes));
}

static void update_working_state(struct lesson_meta &meta, const attempt_record &r,
                                 double dt)
{
   if ((r.col_id == 0) || (r.col_id == meta.total_columns - 1))
      calc_reset_working_state(meta);

   meta.working_good += r.good_count;
   meta.working_bad += r.bad_count;
   meta.working_missed += r.missed_count;
   meta.working_duration += dt;
   meta.lives_left -= meta.working_bad + meta.working_missed;
   meta.lives_left = std::max(0, meta.lives_left);
}

static void maybe_apply_completion_bonus(struct stats &stats, lesson_meta &meta)
{
   if (meta.lives_left < 1)
      return;

   if (meta.total_columns == 0)
      return;

   // Completion bonus logic
   double good_score = static_cast<double>(meta.working_good);
   double avg_dt     = meta.working_duration / (double)meta.total_columns;

   double speed_mult = 1.0 / (0.3 + avg_dt);
   speed_mult *= speed_mult; // squared

   double bonus = good_score + (good_score * speed_mult);
   stats.score_today += bonus;
}

static void update_streak(lesson_meta &meta, const attempt_record &r)
{
   if (meta.lives_left < 1) {
      meta.streak = 0;
      return;
   }

   if (r.col_id == meta.total_columns - 1) {
      meta.streak++;
   }
}

static void calc_score(struct stats &stats, const struct attempt_record &r)
{
   if (!time_is_today(r.time))
      return; // Only count score from today's attempts

   auto &meta = calc_get_lesson_meta(stats, r.lesson_id);

   // (1) Immediate penalties
   apply_mistake_penalty(stats, r);

   // (2) Accumulate dt only if progressing
   double dt = 0.0;
   if (meta.in_progress && r.col_id > meta.last_col_id) {
      dt = std::min(r.time - meta.last_time, 5.0);
   }

   update_working_state(meta, r, dt);

   // (3) Award bonus only on successful completion
   if (r.col_id == meta.total_columns - 1) {
      maybe_apply_completion_bonus(stats, meta);
   }

   // (4) Update streak based on mistakes
   update_streak(meta, r);
}

static void calc_practice_streak(struct stats &stats,
                                 const struct attempt_record &r,
                                 double score_goal)
{
   if (!time_is_today(r.time))
      return; // Only process today's records

   std::time_t today     = time_day_start(r.time);
   std::time_t yesterday = today - 86400;

   // Check if we just crossed the goal threshold
   if (stats.score_today >= score_goal && !stats.goal_met_today) {
      stats.goal_met_today = true;

      // Check continuity
      if (time_day_start(static_cast<double>(stats.last_practice_date)) ==
          yesterday) {
         stats.practice_streak++;
      } else if (time_day_start(
                     static_cast<double>(stats.last_practice_date)) == today) {
         // Already counted (should be covered by goal_met_today, but safe
         // check)
      } else {
         // Streak broken or new
         stats.practice_streak = 1;
      }
      stats.last_practice_date = static_cast<std::time_t>(r.time);
   }
}

static void update_srs_state(lesson_meta &meta)
{
   // Fail / abandon → reset interval
   if (meta.quality < 3.0)
      meta.srs_interval = 0.0;

   // Update interval
   if (meta.srs_interval == 0.0) {
      // Short initial interval for typing practice (4h)
      meta.srs_interval = 4.0 * 3600.0;
   } else {
      meta.srs_interval *= meta.srs_ease;
   }

   // Continuous SM-2 ease adjustment
   double delta =
       0.1 - (5.0 - meta.quality) * (0.08 + (5.0 - meta.quality) * 0.02);
   meta.srs_ease += delta;
   meta.srs_ease = std::max(1.3, meta.srs_ease);

   // Due time
   std::time_t now = std::time(nullptr);
   meta.srs_due    = now + static_cast<long>(meta.srs_interval);
}

static void handle_abandonment(struct stats &stats,
                               const struct attempt_record &r)
{
   if (!stats.has_last_record)
      return;

   bool same_lesson = (r.lesson_id == stats.last_record.lesson_id);
   bool restart     = (r.col_id == 0);

   if (same_lesson && !restart)
      return; // Still on same lesson attempt, not abandoned

   auto &prev_meta = calc_get_lesson_meta(stats, stats.last_record.lesson_id);

   // If previous was in progress and not at the end → abandoned
   if (prev_meta.in_progress &&
       prev_meta.last_col_id != prev_meta.total_columns - 1) {
      prev_meta.quality = 0.0;
      update_srs_state(prev_meta);
      prev_meta.in_progress = false;
   }
}
static double compute_pace_score(double working_max_dt, double speed)
{
   const double eps = 1e-9;

   // Instant performance factor (smaller is better)
   const double target_dt = 1.0;
   double instant_factor  = target_dt / (working_max_dt + eps);
   instant_factor         = std::clamp(instant_factor, 0.0, 1.0);

   // Historical speed factor (larger is better)
   const double target_speed = 0.1;
   double historical_factor  = speed / (target_speed + eps);
   historical_factor         = std::clamp(historical_factor, 0.0, 1.0);

   // Blend the two contributions
   const double w_instant    = 0.5;
   const double w_historical = 0.5;

   double pace_score =
       instant_factor * w_instant + historical_factor * w_historical;

   return pace_score;
}

static double compute_quality(const lesson_meta &meta)
{
   const double eps = 1e-9;
   const double total_events =
       static_cast<double>(meta.working_good + meta.working_bad +
                           meta.working_missed) +
       eps;

   // Mistake score
   double bad_ratio =
       static_cast<double>(meta.working_bad + meta.working_missed) /
       total_events;
   const double max_bad_ratio = 0.30; // tolerable mistake rate
   double mistake_score       = 1.0 - (bad_ratio / max_bad_ratio);
   mistake_score              = std::clamp(mistake_score, 0.0, 1.0);

   // Pace score
   double pace_score = compute_pace_score(meta.working_max_dt, meta.speed);

   // Streak score
   const double full_streak = 5.0; // streak ≥5 → full credit
   double streak_score      = std::clamp(meta.streak / full_streak, 0.0, 1.0);

   // Weighted blend
   const double w_mistake = 0.50;
   const double w_pace    = 0.25;
   const double w_streak  = 0.25;

   double smooth_score = mistake_score * w_mistake + pace_score * w_pace +
                         streak_score * w_streak;

   double quality = smooth_score * 5.0;

   return quality;
}

static void calc_schedule(struct stats &stats, const struct attempt_record &r)
{
   handle_abandonment(stats, r);

   auto &meta = calc_get_lesson_meta(stats, r.lesson_id);

   if (r.col_id == meta.total_columns - 1) {
      meta.quality = compute_quality(meta);
      update_srs_state(meta);
   }

   stats.last_record     = r;
   stats.has_last_record = true;
}

static int pick_easier_lesson(const std::vector<int> &lesson_ids, stats &stats,
                              int current_best)
{
   const auto &best_meta = calc_get_lesson_meta(stats, current_best);
   std::vector<int> easier_alternatives;

   for (int id : lesson_ids) {
      if (id == current_best)
         continue;

      const auto &meta = calc_get_lesson_meta(stats, id);

      // Only consider lessons that are due or past due
      if (meta.srs_due > std::time(nullptr))
         continue;

      // Treat lower working_max_dt as easier
      if (meta.working_max_dt < best_meta.working_max_dt)
         easier_alternatives.push_back(id);
   }

   if (easier_alternatives.empty())
      return current_best;

   static std::mt19937 rng(std::random_device{}());
   std::uniform_int_distribution<size_t> idx_dist(
       0, easier_alternatives.size() - 1);
   return easier_alternatives[idx_dist(rng)];
}

int calc_next(const std::vector<int> &lesson_ids, struct stats &stats)
{
   if (lesson_ids.empty())
      return -1;

   int best_candidate     = -1;
   std::time_t lowest_due = std::numeric_limits<std::time_t>::max();
   std::time_t now        = std::time(nullptr);

   // Step 1: Find the earliest due lesson
   for (int id : lesson_ids) {
      const auto &meta = calc_get_lesson_meta(stats, id);
      std::time_t due  = meta.srs_due;

      // Treat srs_due == 0 (new lesson) as due right now
      if (due == 0)
         due = now;

      if (due < lowest_due) {
         lowest_due     = due;
         best_candidate = id;
      }
   }

   // Step 2: Occasionally pick an easier lesson to boost motivation
   static std::mt19937 rng(std::random_device{}());
   std::uniform_real_distribution<double> dist(0.0, 1.0);

   const double easier_prob = 0.10; // 10% chance
   if (dist(rng) < easier_prob)
      best_candidate = pick_easier_lesson(lesson_ids, stats, best_candidate);

   return best_candidate;
}

void calc_stats(struct stats &stats, int score_goal,
                const struct attempt_record &r)
{
   calc_duration(stats, r);
   calc_speed(stats, r);
   calc_score(stats, r);
   calc_practice_streak(stats, r, score_goal);
   calc_schedule(stats, r);

   // Calculate if lesson is still in progress
   auto &meta = calc_get_lesson_meta(stats, r.lesson_id);
   if (r.col_id == meta.total_columns - 1)
      meta.in_progress = false;
   else
      meta.in_progress = true;

   // Update working progress tracking for next step
   meta.last_col_id = r.col_id;
   meta.last_time   = r.time;
}
