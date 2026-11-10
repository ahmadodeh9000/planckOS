/*
 * apps.c - registers every app with the launcher. To add an app, include
 * its header and add one line to app_list.
 */
#include "apps/apps.h"
#include "apps/terminal.h"
#include "apps/files.h"
#include "apps/notepad.h"
#include "apps/clock.h"
#include "apps/about.h"
#include "drivers/power.h"

const app_entry app_list[] = {
    { "Terminal",  terminal_open },
    { "Files",     files_open },
    { "Notepad",   notepad_open },
    { "Clock",     clock_open},
    { "About",     about_open },
    { "Shut down", power_off },
};

const int app_count = sizeof(app_list) / sizeof(app_list[0]);
