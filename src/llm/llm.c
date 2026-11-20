/*
 * llm.c - loads a model from the disk and runs it one token at a time.
 *
 * The generation loop is run.c's generate() turned inside out: instead of
 * a while loop that prints, llm_next() does one turn of that loop and
 * returns, so the caller decides when to run the next one. Prompt tokens
 * wait in 'pending' and are fed first; after the last one the model's own
 * choices are fed back in.
 */
#include "llm/llm.h"
#include "llm/model.h"
#include "llm/tokenizer.h"
#include "llm/sampler.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"
#include "drivers/rtc.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"

struct llm {
    model_t model;
    tokenizer_t tokenizer;
    sampler_t sampler;
    uint8_t* model_data;        /* the model file; the weights point into it */
    int* pending;               /* tokens still to feed (seq_len of them) */
    int pending_count;
    int pending_index;
    int pos;                    /* tokens fed so far = next position */
    int token;                  /* the last token fed */
    int next;                   /* the token sampled after it, if has_next */
    bool has_next;
};

/* ---- loading ------------------------------------------------------ */

/* Read a whole file into a new buffer. */
static llm_status read_file(const char* name, uint8_t** data, uint32_t* size) {
    if (fat12_size(name, size) != FAT12_OK) {
        return LLM_NO_FILE;
    }
    *data = kmalloc(*size ? *size : 1);
    if (!*data) {
        return LLM_NO_MEMORY;
    }
    uint32_t got;
    if (fat12_read(name, *data, *size, &got) != FAT12_OK || got != *size) {
        kfree(*data);
        return LLM_NO_FILE;
    }
    return LLM_OK;
}

/* Different every run: the wall clock plus the ticks since boot. */
static uint64_t random_seed(void) {
    rtc_time_t now;
    rtc_read(&now);
    uint64_t seed = ((uint64_t)now.hour << 32) | ((uint32_t)now.minute << 16) | now.second;
    return (seed << 8) ^ timer_ticks() ^ 0x9E3779B97F4A7C15ULL;
}

static llm_status load_tokenizer(llm_t* llm, const char* tokenizer_file) {
    uint8_t* data;
    uint32_t size;
    llm_status status = read_file(tokenizer_file, &data, &size);
    if (status != LLM_OK) {
        return status;
    }
    bool ok = tokenizer_init(&llm->tokenizer, data, size, llm->model.config.vocab_size);
    kfree(data);
    return ok ? LLM_OK : LLM_BAD_TOKENIZER;
}

static llm_status load_parts(llm_t* llm, const char* model_file, const char* tokenizer_file) {
    uint32_t size;
    llm_status status = read_file(model_file, &llm->model_data, &size);
    if (status != LLM_OK) {
        return status;
    }
    status = model_init(&llm->model, llm->model_data, size);
    if (status != LLM_OK) {
        return status;
    }
    status = load_tokenizer(llm, tokenizer_file);
    if (status != LLM_OK) {
        return status;
    }
    llm->pending = kmalloc(llm->model.config.seq_len * sizeof(int));
    if (!llm->pending) {
        return LLM_NO_MEMORY;
    }
    sampler_init(&llm->sampler, llm->model.config.vocab_size, 0.0f, random_seed());
    return LLM_OK;
}

llm_status llm_load(const char* model_file, const char* tokenizer_file, llm_t** out) {
    *out = NULL;
    llm_t* llm = kmalloc(sizeof(llm_t));
    if (!llm) {
        return LLM_NO_MEMORY;
    }
    memset(llm, 0, sizeof(llm_t));
    llm_status status = load_parts(llm, model_file, tokenizer_file);
    if (status != LLM_OK) {
        llm_free(llm);
        return status;
    }
    serial_printf("llm: loaded %s (dim %d, %d layers, %d heads, vocab %d, context %d)\n",
                  model_file, llm->model.config.dim, llm->model.config.n_layers,
                  llm->model.config.n_heads, llm->model.config.vocab_size,
                  llm->model.config.seq_len);
    *out = llm;
    return LLM_OK;
}

const char* llm_status_message(llm_status status) {
    switch (status) {
        case LLM_OK:            return "ok";
        case LLM_NO_FILE:       return "model or tokenizer file not found";
        case LLM_BAD_MODEL:     return "the model file is damaged or incomplete";
        case LLM_BAD_TOKENIZER: return "the tokenizer file doesn't match the model";
        case LLM_NO_MEMORY:     return "not enough memory for the model";
    }
    return "unknown error";
}

void llm_free(llm_t* llm) {
    if (!llm) {
        return;
    }
    model_free(&llm->model);
    tokenizer_free(&llm->tokenizer);
    kfree(llm->model_data);
    kfree(llm->pending);
    kfree(llm);
}

void llm_set_temperature(llm_t* llm, float temperature) {
    llm->sampler.temperature = temperature;
}

/* ---- generating --------------------------------------------------- */

int llm_tokens_used(llm_t* llm) {
    return llm->pos + (llm->pending_count - llm->pending_index) + (llm->has_next ? 1 : 0);
}

int llm_context_length(llm_t* llm) {
    return llm->model.config.seq_len;
}

int llm_count_tokens(llm_t* llm, const char* text, bool first) {
    int max = llm->model.config.seq_len;
    int* tokens = kmalloc(max * sizeof(int));
    if (!tokens) {
        return -1;
    }
    int n = tokenizer_encode(&llm->tokenizer, text, first, first, tokens, max);
    kfree(tokens);
    return n;
}

/* Move the queue to the front of 'pending' and append 'text' to it. */
static bool queue_text(llm_t* llm, const char* text, bool first) {
    int waiting = llm->pending_count - llm->pending_index;
    memmove(llm->pending, llm->pending + llm->pending_index, waiting * sizeof(int));
    llm->pending_index = 0;
    llm->pending_count = waiting;

    int room = llm->model.config.seq_len - llm->pos - waiting;
    int n = tokenizer_encode(&llm->tokenizer, text, first, first,
                             llm->pending + waiting, room);
    if (n < 0) {
        return false;
    }
    llm->pending_count += n;
    return true;
}

bool llm_start(llm_t* llm, const char* prompt) {
    llm->pos = 0;
    llm->pending_count = 0;
    llm->pending_index = 0;
    llm->has_next = false;
    return queue_text(llm, prompt, true);
}

bool llm_feed(llm_t* llm, const char* text) {
    /* The model's last choice hasn't been fed yet: it goes first. */
    if (llm->has_next) {
        int waiting = llm->pending_count - llm->pending_index;
        if (llm->pos + waiting + 1 > llm->model.config.seq_len) {
            return false;
        }
        llm->pending[llm->pending_count++] = llm->next;
        llm->has_next = false;
    }
    return queue_text(llm, text, false);
}

/* Byte tokens can be control characters; only pass on text and newlines
   (run.c's safe_printf does the same). */
static const char* printable_piece(const char* piece) {
    unsigned char c = piece[0];
    if (piece[0] != '\0' && piece[1] == '\0' && c != '\n' && (c < ' ' || c > '~')) {
        return "";
    }
    return piece;
}

llm_step_t llm_next(llm_t* llm, const char** piece) {
    *piece = "";
    int token;
    if (llm->pending_index < llm->pending_count) {
        token = llm->pending[llm->pending_index++];
    } else if (llm->has_next) {
        token = llm->next;
        llm->has_next = false;
    } else {
        return LLM_STEP_END;
    }
    if (llm->pos >= llm->model.config.seq_len) {
        return LLM_STEP_FULL;
    }

    float* logits = forward(&llm->model, token, llm->pos++);
    llm->token = token;
    if (llm->pending_index < llm->pending_count) {
        return LLM_STEP_PROMPT;     /* still reading: the logits don't matter */
    }

    int next = sample(&llm->sampler, logits);
    if (next == TOKEN_BOS) {
        return LLM_STEP_END;        /* BOS separates texts: this one is over */
    }
    llm->next = next;
    llm->has_next = true;
    *piece = printable_piece(tokenizer_decode(&llm->tokenizer, token, next));
    return LLM_STEP_TOKEN;
}
