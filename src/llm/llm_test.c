/*
 * llm_test.c - the 'llm-test' shell command: checks the port against llama2.c.
 *
 * stories260K (a 260K-parameter model trained on TinyStories) with the
 * prompt "Once upon a time" at temperature 0 always writes the same story.
 * EXPECTED is what llama2.c's run.c wrote on a Mac:
 *
 *   ./run stories260K.bin -z tok512.bin -t 0.0 -n 256 -i "Once upon a time"
 *
 * If forward(), the tokenizer or the x87 math were wrong, the text would
 * drift away from it, usually within a few tokens.
 */
#include "llm/llm_test.h"
#include "llm/llm.h"
#include "mm/kmalloc.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define TEST_MODEL      "S260K.BIN"
#define TEST_TOKENIZER  "TOK512.BIN"
#define TEST_PROMPT     "Once upon a time"
#define TEST_STEPS      256             /* run.c's -n 256: prompt included */
#define OUTPUT_MAX      2048

/* run.c's output after the prompt (252 tokens). */
static const char EXPECTED[] =
    ", there was a little girl named Lily. She loved to play outside "
    "in the park. One day, she saw a big, red ball. She wanted to play "
    "with it, but it was too high.\n"
    "Lily's mom said, \"Lily, let's go to the park.\" Lily was sad and "
    "didn't know what to do. She said, \"I want to play with your ball, "
    "but I can't find it.\"\n"
    "Lily was sad and didn't know what to do. She said, \"I'm sorry, Lily. "
    "I didn't know what to do.\"\n"
    "Lily didn't want to help her mom, so she said, \"I'm sorry, mom. "
    "I didn't know what to do.\" Her mom said, \"Don't worry, Lily. We "
    "can help you.";

typedef struct {
    char text[OUTPUT_MAX];
    int length;
    int tokens;                 /* tokens generated */
    int matching_tokens;        /* tokens before the first difference */
    uint32_t ticks;             /* time spent generating */
} test_run_t;

/* Generate until run.c would have stopped, timing it. */
static void generate(llm_t* llm, test_run_t* run) {
    uint32_t start = timer_ticks();
    const char* piece;
    llm_step_t step;

    /* run.c runs the model while pos < steps; the sampled token waiting to
       be fed counts as used, hence <=. */
    while (llm_tokens_used(llm) <= TEST_STEPS &&
           (step = llm_next(llm, &piece)) <= LLM_STEP_TOKEN) {
        if (step != LLM_STEP_TOKEN) {
            continue;
        }
        int n = strlen(piece);
        if (run->length + n < OUTPUT_MAX) {
            memcpy(run->text + run->length, piece, n);
            run->length += n;
        }
        run->tokens++;
        /* Still matching after this piece? */
        if (run->matching_tokens == run->tokens - 1 &&
            run->length < (int)sizeof(EXPECTED) &&
            memcmp(run->text, EXPECTED, run->length) == 0) {
            run->matching_tokens++;
        }
    }
    run->text[run->length] = '\0';
    run->ticks = timer_ticks() - start;
}

static void report(console_t* con, const test_run_t* run) {
    console_printf(con, "%s%s\n\n", TEST_PROMPT, run->text);
    uint32_t ms = run->ticks * (1000 / TIMER_HZ);
    if (ms > 0) {
        console_printf(con, "llm-test: %d tokens in %u ms (%u tokens/s)\n",
                       run->tokens, ms, (uint32_t)run->tokens * 1000 / ms);
    }
    if (strcmp(run->text, EXPECTED) == 0) {
        console_printf(con, "llm-test: PASS, all %d tokens match llama2.c\n", run->tokens);
    } else {
        console_printf(con, "llm-test: FAIL, the first %d of %d tokens match llama2.c\n",
                       run->matching_tokens, run->tokens);
    }
}

bool llm_test(console_t* con, const char* model, const char* tokenizer) {
    uint32_t heap_before, heap_free;
    heap_stats(&heap_before, &heap_free);

    llm_t* llm;
    llm_status status = llm_load(model ? model : TEST_MODEL,
                                 tokenizer ? tokenizer : TEST_TOKENIZER, &llm);
    if (status != LLM_OK) {
        console_printf(con, "llm-test: %s\n", llm_status_message(status));
        return false;
    }
    test_run_t* run = kmalloc(sizeof(test_run_t));
    if (!run) {
        console_print(con, "llm-test: out of memory\n");
        llm_free(llm);
        return false;
    }
    memset(run, 0, sizeof(test_run_t));
    llm_start(llm, TEST_PROMPT);
    generate(llm, run);
    llm_free(llm);

    report(con, run);
    bool passed = strcmp(run->text, EXPECTED) == 0;
    int run_tokens = run->tokens;
    uint32_t run_ticks = run->ticks;
    kfree(run);

    /* Everything the model allocated must be back. */
    uint32_t heap_after;
    heap_stats(&heap_after, &heap_free);
    console_printf(con, "llm-test: heap used %u bytes before, %u after\n", heap_before, heap_after);
    serial_printf("llm-test: %s, %d tokens, %u ticks, heap %u -> %u\n", passed ? "PASS" : "FAIL",
                  run_tokens, run_ticks, heap_before, heap_after);
    return passed;
}
