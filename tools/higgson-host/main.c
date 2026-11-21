/*
 * main.c - higgson for a normal computer: compiles a .HGS file to the same
 * assembly the higgson command writes inside planckOS.
 *
 *   build/higgson-host/higgson IN.HGS OUT.ASM
 *
 * Errors look like the planckOS command's, and the exit code is 1.
 */
#include "higgson/higgson.h"

#include <stdio.h>
#include <stdlib.h>

static char* hg_read_file(const char* path, long* size) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* source = malloc(*size + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }

    *size = fread(source, 1, *size, file);
    fclose(file);
    return source;
}

static int hg_write_file(const char* path, const char* text, uint32_t length) {
    FILE* file = fopen(path, "wb");
    if (!file) {
        return 0;
    }
    int ok = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && ok;
}

static void hg_print_error(const char* in, const hg_result_t* result) {
    if (result->line) {
        fprintf(stderr, "higgson: %s line %d: %s\n", in, result->line, result->message);
    } else {
        fprintf(stderr, "higgson: %s: %s\n", in, result->message);
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: higgson <source.hgs> <output.asm>\n");
        return 1;
    }

    long size;
    char* source = hg_read_file(argv[1], &size);
    if (!source) {
        fprintf(stderr, "higgson: could not read %s\n", argv[1]);
        return 1;
    }

    hg_result_t result = hg_compile(source, (uint32_t)size);
    int ok = result.ok;
    if (!ok) {
        hg_print_error(argv[1], &result);
    } else if (!hg_write_file(argv[2], result.text, result.length)) {
        fprintf(stderr, "higgson: could not write %s\n", argv[2]);
        ok = 0;
    }

    hg_result_free(&result);
    free(source);
    return ok ? 0 : 1;
}
