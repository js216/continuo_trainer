// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Time and date manipulation.
 * @author Jakob Kastelic
 */

#include "time_utils.h"
#include <chrono>

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
    auto days_now = now_sec / (24 * 3600);

    // Same for the given timestamp
    auto days_ts = static_cast<int64_t>(epoch_seconds) / (24 * 3600);

    return days_now == days_ts;
}

void time_format(float seconds, char *buf, size_t buf_size)
{
    if (seconds < 60.0f)
        snprintf(buf, buf_size, "%.0fs", seconds);
    else if (seconds < 3600.0f)
        snprintf(buf, buf_size, "%02d:%02d",
                 int(seconds / 60.0f),
                 int(seconds) % 60);
    else
        snprintf(buf, buf_size, "%d:%02d:%02d",
                 int(seconds / 3600.0f),
                 (int(seconds) % 3600) / 60,
                 int(seconds) % 60);
}
