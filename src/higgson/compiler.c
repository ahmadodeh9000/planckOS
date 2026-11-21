/*
 * compiler.c - hg_compile: runs the lexer, parser and emitter on a source.
 *
 * This file sets up one compilation: the arena all its memory comes from,
 * the lexer, parser and emitter (in the arena too, not on the stack, which
 * is only 8 KB in the kernel), and the place an error jumps back to.
 */
#include "higgson/higgson.h"
#include "higgson/hg.h"
#include "higgson/lex.h"
#include "higgson/emit.h"
#include "higgson/parser.h"
#include "higgson/util.h"

#define HG_ARENA_SIZE   (512 * 1024)    /* all one compilation may use */

typedef struct {
    const char* source;
    uint32_t length;
    const char* text;           /* the assembly, once compiled */
    uint32_t text_length;
} hg_job_t;

/* The compilation in progress, for hg_alloc and hg_error. */
static hg_arena_t* arena;
static int error_line;
static char error_message[HG_ERROR_MAX];

void* hg_alloc(uint32_t size) {
    void* memory = hg_arena_alloc(arena, size);
    if (!memory) {
        hg_error(0, "program too big (out of compiler memory)");
    }
    return memory;
}

void hg_error(int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    hg_platform_vformat(error_message, HG_ERROR_MAX, fmt, args);
    va_end(args);
    error_line = line;
    hg_platform_fail();
}

static void hg_compile_job(void* arg) {
    hg_job_t* job = arg;

    /* A '\0'-terminated copy: the lexer reads one past the last character. */
    char* source = hg_alloc(job->length + 1);
    hg_util_memcpy(source, job->source, job->length);
    source[job->length] = '\0';

    hg_lexer_t* lexer = hg_alloc(sizeof(hg_lexer_t));
    hg_emitter_t* emitter = hg_alloc(sizeof(hg_emitter_t));
    hg_parser_t* parser = hg_alloc(sizeof(hg_parser_t));

    hg_lexer_init(lexer, source, job->length);
    hg_emitter_init(emitter);
    hg_parser_init(parser, lexer, emitter);
    hg_parser_program(parser);
    job->text = hg_emitter_text(emitter, &job->text_length);
}

static void hg_set_error(hg_result_t* result, int line, const char* message) {
    uint32_t length = hg_util_strlen(message);
    if (length >= HG_ERROR_MAX) {
        length = HG_ERROR_MAX - 1;
    }
    hg_util_memcpy(result->message, message, length);
    result->message[length] = '\0';
    result->line = line;
    result->ok = false;
}

hg_result_t hg_compile(const char* source, uint32_t length) {
    hg_result_t result = { 0 };
    hg_job_t job = { source, length, NULL, 0 };

    if (!hg_arena_init(&result.arena, HG_ARENA_SIZE)) {
        hg_set_error(&result, 0, "not enough memory to compile");
        return result;
    }
    arena = &result.arena;
    if (hg_platform_try(hg_compile_job, &job)) {
        result.ok = true;
        result.text = job.text;
        result.length = job.text_length;
    } else {
        hg_set_error(&result, error_line, error_message);
    }
    arena = NULL;
    return result;
}

void hg_result_free(hg_result_t* result) {
    hg_arena_free(&result->arena);
    result->text = NULL;
    result->length = 0;
}
