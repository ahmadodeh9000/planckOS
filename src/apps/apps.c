/*
 * apps.c - registers every app with the launcher. To add an app, include
 * its header and add one line to app_list.
 */
#include "apps/apps.h"
#include "apps/demo.h"
#include "apps/terminal.h"
#include "apps/notepad.h"
#include "apps/files.h"
#include "apps/about.h"

const app_entry app_list[] = {
    { "Terminal",    terminal_open },
    { "Files",       files_open },
    { "Notepad",     notepad_open },
    { "About",       about_open },
    { "Widget demo", demo_open },
};

const int app_count = sizeof(app_list) / sizeof(app_list[0]);
