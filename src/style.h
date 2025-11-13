/**
 * @file style.h
 * @brief App styling.
 * @author Jakob Kastelic
 */

#ifndef STYLE_H
#define STYLE_H

#define STYLE_WHITE  0xFFFFFFFFU
#define STYLE_BLACK  0x00000000U
#define STYLE_RED    0xFFF44336U
#define STYLE_PINK   0xFFE91E63U
#define STYLE_ORANGE 0xFFFF9800U
#define STYLE_AMBER  0xFFFFC107U
#define STYLE_YELLOW 0xFFFFEB3BU
#define STYLE_LIME   0xFFCDDC39U
#define STYLE_GREEN  0xFF4CAF50U
#define STYLE_TEAL   0xFF009688U
#define STYLE_CYAN   0xFF00BCD4U
#define STYLE_BLUE   0xFF2196F3U
#define STYLE_INDIGO 0xFF3F51BU
#define STYLE_PURPLE 0xFF9C27B0U
#define STYLE_BROWN  0xFF795548U
#define STYLE_GREY   0xFF9E9E9EU

void set_style(void);
void dark_mode(void);

#endif /* STYLE_H */
