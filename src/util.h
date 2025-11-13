// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * @file util.h
 * @brief Utility macros.
 * @author Jakob Kastelic
 */

#ifndef UTIL_H
#define UTIL_H

#include <iostream>

#define ERROR(msg)                                                             \
   do {                                                                        \
      std::cerr << "ERROR: " << msg << "\n";                                   \
   } while (0)

#endif // UTIL_H
