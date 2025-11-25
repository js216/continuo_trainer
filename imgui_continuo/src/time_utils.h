// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file time_utils.h
 * @brief Time and date manipulation.
 * @author Jakob Kastelic
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <string>

double time_now(void);
bool time_is_today(double epoch_seconds);
std::string time_format(double seconds);

#endif /* TIME_UTILS_H */
