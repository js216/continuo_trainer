// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file style.h
 * @brief App styling.
 * @author Jakob Kastelic
 */

#ifndef STYLE_H
#define STYLE_H

#include <cstdint>

#define STYLE_LINE_THICKNESS 1.5F

#define STYLE_WHITE  0xFFFFFFFFU
#define STYLE_RED    0xFF3636F4U
#define STYLE_GREEN  0xFF4CAF50U
#define STYLE_BLUE   0xFFAF4C50U
#define STYLE_GRAY   0xFFAAAAAAU
#define STYLE_YELLOW 0xFF50D0D0U

#define STYLE_BTN_H 40.0F
#define STYLE_PAD_X 10.0F
#define STYLE_PAD_Y 10.0F

enum anchor {
   ANCHOR_TOP_LEFT,
   ANCHOR_TOP_CENTER,
   ANCHOR_TOP_RIGHT,
   ANCHOR_CENTER_LEFT,
   ANCHOR_CENTER,
   ANCHOR_CENTER_RIGHT,
   ANCHOR_BOTTOM_LEFT,
   ANCHOR_BOTTOM_CENTER,
   ANCHOR_BOTTOM_RIGHT
};

struct font_config {
   float fontsize;
   enum anchor anch;
   uint32_t color;
   float border_size     = 0;
   uint32_t border_color = 0;
   float anchor_size     = 0;
   uint32_t anchor_color = 0;
};

void set_font(struct state *state);
void set_style(void);
void dark_mode(void);

void style_text(const char *text, float x, float y, const font_config *cfg);

#endif /* STYLE_H */
