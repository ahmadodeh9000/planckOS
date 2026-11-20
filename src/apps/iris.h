/*
 * iris.h - the Iris app: a chat window with planckOS's assistant.
 */
#ifndef iris_h__
#define iris_h__

#include "gfx/surface.h"

void iris_open(void);

/* Her 16x16 face, for the launcher menu and the taskbar. */
void iris_draw_icon(surface_t* s, int x, int y);

#endif
