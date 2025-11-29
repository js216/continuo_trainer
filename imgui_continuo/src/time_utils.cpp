// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Time and date manipulation.
 * @author Jakob Kastelic
 */

#include "time_utils.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

struct tm;

double time_now(void)
{
   auto now = std::chrono::system_clock::now();
   auto dur = now.time_since_epoch();
   return std::chrono::duration<double>(dur).count();
}

bool time_is_today(double epoch_seconds)
{
   using namespace std::chrono;

   // Current time
   auto now_tp  = system_clock::now();
   auto now_sec = time_point_cast<seconds>(now_tp).time_since_epoch().count();

   // Determine the current local timezone offset in seconds
   auto now_tt             = static_cast<std::time_t>(now_sec);
   const std::tm *local_tm = std::localtime(&now_tt);
   auto offset =
       (local_tm->tm_hour * 3600 + local_tm->tm_min * 60 + local_tm->tm_sec) -
       (now_sec % 86400);

   // Apply the same offset to both timestamps
   constexpr int secs_per_day = 24 * 3600;
   auto days_now              = (now_sec + offset) / secs_per_day;
   auto days_ts = (static_cast<int64_t>(epoch_seconds) + offset) / secs_per_day;

   return days_now == days_ts;
}

std::string time_format(double seconds)
{
   std::array<char, 32> buf{};

   if (seconds < 60.0) {
      (void)std::snprintf(buf.data(), buf.size(), "%.0fs", seconds);
   } else if (seconds < 3600.0) {
      const int mins = static_cast<int>(seconds / 60.0);
      const int secs = static_cast<int>(seconds) % 60;
      (void)std::snprintf(buf.data(), buf.size(), "%02d:%02d", mins, secs);
   } else {
      const int hours = static_cast<int>(seconds / 3600.0);
      const int mins  = (static_cast<int>(seconds) % 3600) / 60;
      const int secs  = static_cast<int>(seconds) % 60;
      (void)std::snprintf(buf.data(), buf.size(), "%d:%02d:%02d", hours, mins,
                          secs);
   }

   return {buf.data()};
}

std::time_t day_start(double epoch_seconds)
{
   const auto t = static_cast<std::time_t>(epoch_seconds);
   std::tm tm   = *std::localtime(&t);
   tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
   return std::mktime(&tm);
}

bool is_consecutive_day(std::time_t prev_day, std::time_t curr_day)
{
   const std::tm tm_prev = *std::localtime(&prev_day);
   const std::tm tm_curr = *std::localtime(&curr_day);
   return tm_curr.tm_year == tm_prev.tm_year &&
          tm_curr.tm_yday + 1 == tm_prev.tm_yday;
}
