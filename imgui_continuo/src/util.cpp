// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * @file util.cpp
 * @brief Utility macros.
 * @author Jakob Kastelic
 */

#include "util.h"
#include <iostream>
#include <string>

void error(const std::string &msg)
{
   std::cerr << "ERROR: " << msg << "\n";
}
