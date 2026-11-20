/*
 * exec.h - load a program file from the disk and run it.
 */
#ifndef exec_h__
#define exec_h__

#include "lib/console.h"

#define EXEC_MAX_FILE   (1024 * 1024)   /* largest program file: 1 MB */

/* What the shell's 'exec' does: read 'name', load it into the program
   area and run it, with its output and any error on 'con'. Refuses while
   another program's windows are open (they call into the program area).
   Returns true if the program ran and ended normally. */
bool exec_file(const char* name, console_t* con);

#endif
