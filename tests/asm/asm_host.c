/*
 * asm_host.c - runs the planckOS assembler on a normal computer.
 *
 * 'make asm-test' builds the files in src/asm together with this one, assembles
 * tests/asm/encoding.asm, and compares the code bytes (without the ELF
 * headers) with what NASM produces for the same file.
 *
 *   asm_host <input.asm> <output.bin>
 */
#include <stdio.h>
#include <stdlib.h>
#include "asm/asm.h"

#define MAX_SOURCE  (1024 * 1024)
#define MAX_OUTPUT  (1024 * 1024)

static char source[MAX_SOURCE];
static uint8_t output[MAX_OUTPUT];

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.asm> <output.bin>\n", argv[0]);
        return 2;
    }
    FILE* in = fopen(argv[1], "rb");
    if (!in) {
        perror(argv[1]);
        return 1;
    }
    size_t length = fread(source, 1, sizeof(source), in);
    fclose(in);

    asm_result_t result = asm_assemble(source, (uint32_t)length, output, sizeof(output));
    if (!result.ok) {
        fprintf(stderr, "%s:%d: %s\n", argv[1], result.line, result.message);
        return 1;
    }
    FILE* out = fopen(argv[2], "wb");
    if (!out) {
        perror(argv[2]);
        return 1;
    }
    fwrite(output + ASM_HEADER_SIZE, 1, result.size - ASM_HEADER_SIZE, out);
    fclose(out);
    return 0;
}
