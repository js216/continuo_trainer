// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * @file util.h
 * @brief Utility macros.
 * @author Jakob Kastelic
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>

#define ERROR(msg)                                                             \
   do {                                                                        \
      (void)fprintf(stderr, "ERROR: %s\n", msg);                               \
      abort();                                                                 \
   } while (0)

#endif // UTIL_H
