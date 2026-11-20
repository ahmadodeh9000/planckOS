/*
 * apps.c - registers every app with the launcher. To add an app, include
 * its header and add one line to app_list (the icon is optional).
 */
#include "apps/apps.h"
#include "apps/terminal.h"
#include "apps/files.h"
#include "apps/notepad.h"
#include "apps/clock.h"
#include "apps/about.h"
#include "apps/iris.h"
#include "drivers/power.h"

const app_entry app_list[] = {
    { "Terminal",  terminal_open, NULL },
    { "Files",     files_open,    NULL },
    { "Notepad",   notepad_open,  NULL },
    { "Clock",     clock_open,    NULL },
    { "About",     about_open,    NULL },
    { "Iris",      iris_open,     iris_draw_icon },
    { "Shut down", power_off,     NULL },
};

const int app_count = sizeof(app_list) / sizeof(app_list[0]);
