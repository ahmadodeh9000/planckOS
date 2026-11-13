/*
 * progwin.h - windows owned by a loaded program.
 *
 * Programs never see window_t pointers. They get a small id
 * (1..PROGWIN_MAX), and the kernel checks it on every syscall, so a buggy
 * program can't make the kernel touch random memory.
 */
#ifndef progwin_h__
#define progwin_h__

#include "lib/common.h"

#define PROGWIN_MAX     4       /* windows one program may have open */
#define PROGWIN_NAME_MAX 13     /* program file name: "NAME.EXT" + '\0' */

/* Lifetime: a program's windows keep calling into its code after its
   _start has returned, so the code must stay where it is. 'exec' therefore
   refuses to load another program while progwin_owner() isn't NULL. */

/* Remember the name of the program that is about to run. */
void progwin_set_owner(const char* name);

/* The name of the program whose windows are still open, or NULL. */
const char* progwin_owner(void);

/* The program crashed: close all its windows and never call it again. */
void progwin_program_crashed(void);

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

/* Close the window at the next frame. Safe from the window's own callbacks. */
void progwin_close(uint32_t id);

#endif