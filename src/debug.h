/*
 * @file debug.h
 *
 * Debug macros.
 *
 * Jakob Kastelic, 2025.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdlib.h>

#define ERROR(msg)                                                             \
   do {                                                                        \
      (void)fprintf(stderr, "ERROR: %s\n", msg);                               \
      abort();                                                                 \
   } while (0)

#endif // DEBUG_H

// end file debug.h
