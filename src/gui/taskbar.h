/*
 * taskbar.h - the bar along the bottom of the screen: a launcher button,
 * one button per open window, and the uptime clock.
 */
#ifndef taskbar_h__
#define taskbar_h__

#include "lib/common.h"
#include "gfx/surface.h"
#include "gui/event.h"

void taskbar_init(int screen_width, int screen_height);

/* Handle a mouse press. Returns true if the taskbar used it (the click was
   on the taskbar), false to pass it on to the windows. */
bool taskbar_mouse_down(const event_t* e);

void taskbar_draw(surface_t* screen);

#endif
