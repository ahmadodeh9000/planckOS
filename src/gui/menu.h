/*
 * menu.h - the launcher menu that pops up above the taskbar's launcher
 * button and lists the registered apps (apps/apps.c).
 */
#ifndef menu_h__
#define menu_h__

#include "lib/common.h"
#include "gfx/surface.h"
#include "gui/event.h"

/* 'launcher' is the taskbar button that opens the menu; the menu pops up
   right above it. */
void menu_init(rect_t launcher);

/* Handle a mouse press. A press on the launcher button toggles the menu,
   a press on an item launches that app; both return true. Any other press
   closes the menu and returns false so it can be handled elsewhere. */
bool menu_mouse_down(const event_t* e);

/* Track the pointer to highlight the item under it. */
void menu_mouse_move(const event_t* e);

void menu_draw(surface_t* screen);

#endif
