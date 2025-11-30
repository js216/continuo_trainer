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
#include <iomanip>
#include <sstream>
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

std::time_t day_start(double epoch_seconds)
{
   const auto t = static_cast<std::time_t>(epoch_seconds);
   std::tm tm   = *std::localtime(&t);
   tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
   return std::mktime(&tm);
}

std::string time_format(double seconds)
{
   std::array<char, 64> buf{};

   const int total = static_cast<int>(seconds);
   const int days  = total / 86400;
   const int rem   = total % 86400;
   const int hours = rem / 3600;
   const int mins  = (rem % 3600) / 60;
   const int secs  = rem % 60;

   if (total < 60) {
      std::snprintf(buf.data(), buf.size(), "%ds", secs);
   } else if (total < 3600) {
      // “3 min”, “59 min”
      std::snprintf(buf.data(), buf.size(), "%d min", mins);
   } else if (days == 0) {
      // “1:02”, “23:59”
      std::snprintf(buf.data(), buf.size(), "%d:%02d", hours, mins);
   } else {
      // “2 days, 4:07”
      std::snprintf(buf.data(), buf.size(), "%d day%s, %d:%02d", days,
                    (days == 1 ? "" : "s"), hours, mins);
   }

   return std::string(buf.data());
}

std::string time_datestring(double epoch_seconds)
{
   // Convert to time_t (integral seconds)
   std::time_t t = static_cast<std::time_t>(epoch_seconds);

   // Convert to local calendar time
   std::tm tm_buf;
   localtime_r(&t, &tm_buf);

   // Format: YYYY-MM-DD HH:MM:SS
   std::ostringstream oss;
   oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
   return oss.str();
}
