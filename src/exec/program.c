/*
 * program.c - keeps track of the running program (at most one).
 *
 * program_run() records which console the program prints to, then calls
 * it through program_call (program.s). SYS_EXIT and CPU exceptions end the
 * program early through program_unwind, which makes program_call return.
 */
#include "exec/program.h"
#include "drivers/serial.h"

extern uint32_t program_call(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2);
extern void program_unwind(uint32_t exit_code);

static bool running;
static console_t* running_console;
static program_result_t result;

/* The shared core of program_run and program_callback. */
static program_result_t program_enter(uint32_t fn, uint32_t a0, uint32_t a1,
                                      uint32_t a2, console_t* con) {
    result.crashed = false;
    result.exception = 0;
    running_console = con;
    running = true;

    uint32_t exit_code = program_call(fn, a0, a1, a2);

    running = false;
    running_console = NULL;
    if (!result.crashed) {
        result.exit_code = exit_code;
    }
    return result;
}

program_result_t program_run(uint32_t entry, console_t* con) {
    program_result_t r = program_enter(entry, 0, 0, 0, con);
    serial_printf("program: ended, %s %u\n",
                  r.crashed ? "exception" : "exit code",
                  r.crashed ? r.exception : r.exit_code);
    return r;
}

program_result_t program_callback(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2) {
    if (running) {
        /* Never happens today; refuse rather than corrupt saved_esp. */
        serial_printf("program: callback while a program is running\n");
        return (program_result_t){ .crashed = true };
    }
    return program_enter(fn, a0, a1, a2, NULL);
}
console_t* program_console(void) {
    return running ? running_console : NULL;
}

void program_exit(uint32_t exit_code) {
    if (running) {
        program_unwind(exit_code);
    }
}

void program_crash(uint32_t exception) {
    if (running) {
        result.crashed = true;
        result.exception = exception;
        program_unwind(0);
    }
}
