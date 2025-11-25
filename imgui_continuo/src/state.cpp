// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file state.cpp
 * @brief Application state manipulation.
 * @author Jakob Kastelic
 */

#include "state.h"
#include "theory.h"
#include "util.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// global state for debug only
float global_tune;

void state_clear_lesson(struct state *state)
{
   std::fill(std::begin(state->lesson_title), std::end(state->lesson_title),
             '\0');
   state->key = KEY_SIG_0;
   state->chords.clear();
   state->pressed_notes.clear();
   state->active_col = 0;
}
