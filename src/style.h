// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file style.h
 * @brief App styling.
 * @author Jakob Kastelic
 */

#ifndef STYLE_H
#define STYLE_H

#define STYLE_LINE_THICKNESS 1.5F

#define STYLE_WHITE 0xFFFFFFFFU
#define STYLE_RED   0xFF3636F4U
#define STYLE_GREEN 0xFF4CAF50U
#define STYLE_BLUE  0xFFAF4C50U
#define STYLE_GRAY  0xFFAAAAAAU

#define STYLE_BTN_H      40.0F
#define STYLE_PAD_X      7.0F
#define STYLE_PAD_Y      7.0F
#define STYLE_PAD_BORDER 1.0F

void set_style(void);
void set_font(void);
void dark_mode(void);

#endif /* STYLE_H */
