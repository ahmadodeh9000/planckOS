/*
 * apps.h - the list of apps shown in the launcher menu.
 */
#ifndef apps_h__
#define apps_h__

typedef struct {
    const char* name;       /* menu label */
    void (*open)(void);     /* opens a new window of the app */
} app_entry;

extern const app_entry app_list[];
extern const int app_count;

#endif
