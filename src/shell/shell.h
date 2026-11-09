/*
 * shell.h - the command shell.
 *
 * A shell prints to a console_t and is fed one typed key at a time, so it
 * can run on the VGA text screen (shell_run) or inside a Terminal window
 * (which calls shell_key from its on_key callback). Each shell_t has its
 * own input line, so several can run at once.
 */
#ifndef shell_h__
#define shell_h__

#include "lib/console.h"

typedef struct shell shell_t;

/* Start a shell on 'con': prints the banner and the first prompt. */
shell_t* shell_create(console_t* con);
void shell_destroy(shell_t* sh);

/* Feed one typed character: edits the line, or runs it on Enter. */
void shell_key(shell_t* sh, char key);

/* Text mode: run a shell on 'con', fed from the event queue, forever. */
void shell_run(console_t* con);

#endif
