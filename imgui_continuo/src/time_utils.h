// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file time_utils.h
 * @brief Time and date manipulation.
 * @author Jakob Kastelic
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <cstddef>

double time_now(void);
bool time_is_today(double epoch_seconds);
void time_format(float seconds, char *buf, size_t buf_size);

#endif /* TIME_UTILS_H */
