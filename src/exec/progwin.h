/*
 * progwin.h - windows owned by a loaded program.
 *
 * Programs never see window_t pointers. They get a small id
 * (1..PROGWIN_MAX), and the kernel checks it on every syscall, so a buggy
 * program can't make the kernel touch random memory.
 * 
 * 
 * 
 * NOTE FROM AUTHOR: If you are reading this, I love you <3
 */
#ifndef progwin_h__
#define progwin_h__

#include "lib/common.h"

#define PROGWIN_MAX     4       /* windows one program may have open */

/* Open a window for the program. Returns its id, or 0 on failure. */
uint32_t progwin_create(const char* title, int width, int height);

/* Make the program's function 'fn' handle 'event' (WIN_EVENT_*) for
   window 'id'. Returns false if id, event or fn is invalid. */
bool progwin_set_callback(uint32_t id, uint32_t event, uint32_t fn);

/* Drawing, only allowed inside the window's paint callback. */
void progwin_fill_rect(uint32_t id, int x, int y, int w, int h, uint32_t color);
void progwin_draw_text(uint32_t id, int x, int y, const char* text, uint32_t color);

/* Ask for the window to be repainted (its paint callback runs soon). */
void progwin_invalidate(uint32_t id);

#endif