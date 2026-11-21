/*
 * shell_higgson.c - the `higgson` command (see shell_higgson.h).
 *
 * The compiler's assembly never has to touch the disk: it goes straight
 * from hg_compile to asm_assemble in memory, and only the finished
 * program is written. With an .ASM output name the assembly is written
 * instead, to read in Notepad or assemble later with `asm`.
 */
#include "shell/shell_higgson.h"
#include "higgson/higgson.h"
#include "asm/asm.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define HIGGSON_MAX_OUTPUT  (64 * 1024)     /* largest program it writes */
#define ASM_EXTENSION       ".ASM"

/* True if 'name' ends in .ASM, in any mix of capitals. */
static bool wants_assembly(const char* name) {
    uint32_t length = strlen(name);
    uint32_t ext = strlen(ASM_EXTENSION);

    if (length < ext) {
        return false;
    }
    for (uint32_t i = 0; i < ext; i++) {
        char c = name[length - ext + i];
        if (c >= 'a' && c <= 'z') {
            c -= 'a' - 'A';
        }
        if (c != ASM_EXTENSION[i]) {
            return false;
        }
    }
    return true;
}

static void write_output(console_t* con, const char* name, const uint8_t* data, uint32_t size) {
    fat12_status status = fat12_write(name, data, size);
    if (status == FAT12_OK) {
        console_printf(con, "higgson: wrote %s (%u bytes)\n", name, size);
    } else {
        console_printf(con, "higgson: %s: %s\n", fat12_status_message(status), name);
    }
}

/* Assemble the compiler's output and write the program. The assembler
   failing would be a bug in the compiler, not in the user's program, so
   the message says which line of the generated assembly it was. */
static void assemble_output(console_t* con, const char* name, const hg_result_t* compiled) {
    uint8_t* program = kmalloc(HIGGSON_MAX_OUTPUT);
    if (!program) {
        console_print(con, "higgson: out of memory\n");
        return;
    }
    asm_result_t result = asm_assemble(compiled->text, compiled->length, program, HIGGSON_MAX_OUTPUT);
    if (result.ok) {
        write_output(con, name, program, result.size);
    } else {
        console_printf(con, "higgson: the assembler stopped at line %d of the assembly: %s\n",
                       result.line, result.message);
    }
    kfree(program);
}

void shell_higgson(console_t* con, const char* in_name, const char* out_name,
                   const char* source, uint32_t size) {
    uint32_t used_before, used_after, heap_free;

    heap_stats(&used_before, &heap_free);
    hg_result_t result = hg_compile(source, size);
    if (!result.ok && result.line) {
        console_printf(con, "higgson: %s line %d: %s\n", in_name, result.line, result.message);
    } else if (!result.ok) {
        console_printf(con, "higgson: %s: %s\n", in_name, result.message);
    } else if (wants_assembly(out_name)) {
        write_output(con, out_name, (const uint8_t*)result.text, result.length);
    } else {
        assemble_output(con, out_name, &result);
    }
    hg_result_free(&result);

    /* For the tests (HIGGSON.md): the heap must be back where it was. */
    heap_stats(&used_after, &heap_free);
    serial_printf("higgson: heap used %u before, %u after\n", used_before, used_after);
}
