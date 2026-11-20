/*
 * exec.c - load a program file from the disk and run it.
 *
 * Used by the shell's 'exec' and by the Run button under an app Iris made
 * (EXEC.md walks through the steps).
 */
#include "exec/exec.h"
#include "exec/elf.h"
#include "exec/program.h"
#include "exec/progwin.h"
#include "arch/idt.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"

#define KB  1024

/* Read the whole file into a new buffer, or print why not. */
static uint8_t* read_program(const char* name, uint32_t* size, console_t* con) {
    fat12_status status = fat12_size(name, size);
    if (status != FAT12_OK) {
        console_printf(con, "exec: %s: %s\n", fat12_status_message(status), name);
        return NULL;
    }
    if (*size > EXEC_MAX_FILE) {
        console_printf(con, "exec: %s is too big (the limit is %u KB)\n", name, EXEC_MAX_FILE / KB);
        return NULL;
    }
    uint8_t* image = kmalloc(*size ? *size : 1);
    uint32_t got;
    if (!image || fat12_read(name, image, *size, &got) != FAT12_OK || got != *size) {
        console_printf(con, "exec: couldn't read %s\n", name);
        kfree(image);
        return NULL;
    }
    return image;
}

static bool report_exit(const char* name, program_result_t result, console_t* con) {
    if (result.crashed) {
        console_printf(con, "exec: %s crashed: %s\n", name, idt_exception_name(result.exception));
        progwin_program_crashed();      /* windows it opened can't be trusted */
        return false;
    }
    if (result.exit_code != 0) {
        console_printf(con, "exec: %s exited with code %u\n", name, result.exit_code);
        return false;
    }
    return true;
}

bool exec_file(const char* name, console_t* con) {
    /* Loading would overwrite the code that the open windows call into. */
    const char* owner = progwin_owner();
    if (owner) {
        console_printf(con, "exec: close %s's windows first\n", owner);
        return false;
    }
    uint32_t size;
    uint8_t* image = read_program(name, &size, con);
    if (!image) {
        return false;
    }
    uint32_t entry = elf_load(image, size);
    kfree(image);       /* the program now lives in the program area */
    if (!entry) {
        console_printf(con, "exec: %s is not a valid planckOS program "
                            "(the serial log says why)\n", name);
        return false;
    }
    progwin_set_owner(name);
    return report_exit(name, program_run(entry, con), con);
}
