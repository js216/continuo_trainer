// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file time_utils.h
 * @brief Time and date manipulation.
 * @author Jakob Kastelic
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ctime>
#include <string>

double time_now(void);
bool time_is_today(double epoch_seconds);
std::string time_format(double seconds);
std::time_t day_start(double epoch_seconds);
bool is_consecutive_day(std::time_t prev_day, std::time_t curr_day);

#endif /* TIME_UTILS_H */
