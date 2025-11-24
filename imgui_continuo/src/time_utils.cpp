// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Time and date manipulation.
 * @author Jakob Kastelic
 */

#include "time_utils.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <string>

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
   system_clock::time_point now_tp = system_clock::now();

   // Break current time into days since epoch
   auto now_sec = time_point_cast<seconds>(now_tp).time_since_epoch().count();
   constexpr int secs_per_day = 24 * 3600;
   auto days_now              = now_sec / secs_per_day;

   // Same for the given timestamp
   auto days_ts = static_cast<int64_t>(epoch_seconds) / secs_per_day;

   return days_now == days_ts;
}

std::string time_format(double seconds)
{
   std::array<char, 32> buf{};

   if (seconds < 60.0) {
      (void)std::snprintf(buf.data(), buf.size(), "%.0fs", seconds);
   } else if (seconds < 3600.0) {
      int mins = static_cast<int>(seconds / 60.0);
      int secs = static_cast<int>(seconds) % 60;
      (void)std::snprintf(buf.data(), buf.size(), "%02d:%02d", mins, secs);
   } else {
      int hours = static_cast<int>(seconds / 3600.0);
      int mins  = (static_cast<int>(seconds) % 3600) / 60;
      int secs  = static_cast<int>(seconds) % 60;
      (void)std::snprintf(buf.data(), buf.size(), "%d:%02d:%02d", hours, mins,
                          secs);
   }

   return {buf.data()};
}
