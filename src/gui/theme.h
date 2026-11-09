/*
 * theme.h - colors and sizes shared by the window manager, taskbar and
 * widgets, so the whole desktop looks consistent. Change them here.
 */
#ifndef theme_h__
#define theme_h__

#include "gfx/surface.h"

#define THEME_DESKTOP           RGB(0x20, 0x4A, 0x87)
#define THEME_WINDOW_BG         RGB(0xEE, 0xEE, 0xEC)
#define THEME_BORDER            RGB(0x2E, 0x34, 0x36)
#define THEME_TITLE_ACTIVE      RGB(0x34, 0x65, 0xA4)
#define THEME_TITLE_INACTIVE    RGB(0x88, 0x8A, 0x85)
#define THEME_TITLE_TEXT        RGB(0xFF, 0xFF, 0xFF)
#define THEME_CLOSE_BUTTON      RGB(0xCC, 0x00, 0x00)
#define THEME_TEXT              RGB(0x00, 0x00, 0x00)
#define THEME_TEXT_LIGHT        RGB(0xFF, 0xFF, 0xFF)
#define THEME_BUTTON            RGB(0xD3, 0xD7, 0xCF)
#define THEME_BUTTON_BORDER     RGB(0x55, 0x57, 0x53)
#define THEME_FIELD             RGB(0xFF, 0xFF, 0xFF)
#define THEME_SELECTION         RGB(0x72, 0x9F, 0xCF)
#define THEME_TASKBAR           RGB(0x2E, 0x34, 0x36)
#define THEME_TASKBAR_BUTTON    RGB(0x55, 0x57, 0x53)
#define THEME_TASKBAR_ACTIVE    RGB(0x34, 0x65, 0xA4)

#define THEME_BORDER_WIDTH      1
#define THEME_TITLE_HEIGHT      22
#define THEME_CLOSE_SIZE        16
#define THEME_TASKBAR_HEIGHT    28

#endif
