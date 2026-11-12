/*
 * program.c - keeps track of the running program (at most one).
 *
 * program_run() records which console the program prints to, then calls
 * it through program_call (program.s). SYS_EXIT and CPU exceptions end the
 * program early through program_unwind, which makes program_call return.
 */
#include "exec/program.h"
#include "drivers/serial.h"

extern uint32_t program_call(uint32_t entry);
extern void program_unwind(uint32_t exit_code);

static bool running;
static console_t* running_console;
static program_result_t result;

program_result_t program_run(uint32_t entry, console_t* con) {
    result.crashed = false;
    result.exception = 0;
    running_console = con;
    running = true;

    uint32_t exit_code = program_call(entry);

    running = false;
    running_console = NULL;
    if (!result.crashed) {
        result.exit_code = exit_code;
    }
    serial_printf("program: ended, %s %u\n",
                  result.crashed ? "exception" : "exit code",
                  result.crashed ? result.exception : result.exit_code);
    return result;
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
