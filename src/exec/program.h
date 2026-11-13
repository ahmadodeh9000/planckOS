/*
 * program.h - run a loaded program and get it back when it ends.
 *
 * A program ends in one of three ways: it returns from its entry point,
 * it calls SYS_EXIT, or it causes a CPU exception. In every case control
 * comes back to the caller of program_run(), and the OS keeps running.
 */
#ifndef program_h__
#define program_h__

#include "lib/common.h"
#include "lib/console.h"

typedef struct {
    bool crashed;           /* ended by a CPU exception */
    uint32_t exception;     /* which one, if crashed */
    uint32_t exit_code;     /* return value or SYS_EXIT code, if not crashed */
} program_result_t;

/* Call the program at 'entry'. Its SYS_PRINT output goes to 'con'. */
program_result_t program_run(uint32_t entry, console_t* con);

/* Call a program function from the GUI (a callback), as fn(a0, a1, a2).
   Output goes to the default console. Crashes are caught like in program_run. */
program_result_t program_callback(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2);

/* The console of the running program, or NULL if none is running. */
console_t* program_console(void);

/* SYS_EXIT: end the running program. Returns only if none is running. */
void program_exit(uint32_t exit_code);

/* A CPU exception happened: if a program is running, end it and return to
   program_run's caller. Returns only if no program is running. */
void program_crash(uint32_t exception);

#endif
