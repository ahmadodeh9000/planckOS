/*
 * apps.h - the list of apps shown in the launcher menu.
 */
#ifndef apps_h__
#define apps_h__

#include "gfx/surface.h"

typedef struct {
    const char* name;       /* menu label */
    void (*open)(void);     /* opens a new window of the app */
    void (*draw_icon)(surface_t* s, int x, int y);  /* 16x16 icon, or NULL */
} app_entry;

extern const app_entry app_list[];
extern const int app_count;

#endif
