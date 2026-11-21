#include "lex.h"
#include "emit.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

static char* pcl_read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(source, 1, size, file);
    source[read] = '\0';
    fclose(file);
    return source;
}

int main(int argc, char** argv) {
    printf("PCL Compiler\n");

    if (argc != 3) {
        fprintf(stderr, "Usage: pclc <source> <output.asm>\n");
        return 1;
    }

    char* source = pcl_read_file(argv[1]);
    if (!source) {
        fprintf(stderr, "Error: Could not read %s\n", argv[1]);
        return 1;
    }

    pcl_lexer_t lexer;
    pcl_emitter_t emitter;
    pcl_parser_t parser;

    pcl_lexer_init(&lexer, source);
    pcl_emitter_init(&emitter, argv[2]);
    pcl_parser_init(&parser, &lexer, &emitter);

    pcl_parser_program(&parser);
    pcl_write_file(&emitter);
    printf("Compiling completed.\n");

    free(source);
    return 0;
}