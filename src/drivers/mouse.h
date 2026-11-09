/*
 * mouse.h - PS/2 mouse.
 *
 * Movement and button changes become EVENT_MOUSE_MOVE / _DOWN / _UP
 * events (gui/event.h) with the pointer position in screen pixels.
 */
#ifndef mouse_h__
#define mouse_h__

#include "lib/common.h"

/* Enable the mouse. The pointer starts in the middle of a
   screen_width x screen_height screen and is kept inside it. */
void mouse_init(int screen_width, int screen_height);

#endif
