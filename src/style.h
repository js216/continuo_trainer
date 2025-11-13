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
#define STYLE_GRAY  0xBBBBBBBBU

void set_style(void);
void set_font(const char *ttf_path, float size_pixels);
void dark_mode(void);

#endif /* STYLE_H */
