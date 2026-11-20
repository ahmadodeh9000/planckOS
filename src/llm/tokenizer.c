/*
 * tokenizer.c - byte pair encoding, ported from llama2.c's run.c.
 *
 * Ported from run.c by Andrej Karpathy (https://github.com/karpathy/llama2.c,
 * MIT license, see src/llm/LICENSE.llama2c).
 *
 * Encoding starts with one token per character, then repeatedly merges
 * the neighboring pair whose merged string has the highest score in the
 * vocabulary, until no pair is in the vocabulary. Differences from run.c:
 *   - lookups are a linear search over the vocabulary (no qsort/bsearch).
 *     It's fast enough because most strings are rejected by their length.
 *   - each pair's merge result is remembered, so after a merge only the two
 *     new neighbors are looked up again, instead of every pair (run.c).
 *   - no sprintf/sscanf: pairs are compared in place, "<0x0A>" is parsed
 *     by hand.
 */
#include "llm/tokenizer.h"
#include "mm/kmalloc.h"
#include "lib/string.h"
#include "drivers/serial.h"

#define BYTE_TOKEN_OFFSET   3       /* tokens 3..258 are the bytes 0x00..0xFF */
#define UTF8_MAX_BYTES      4
#define BYTE_PIECE_LEN      6       /* "<0x0A>" */

/* ---- loading ------------------------------------------------------ */

/* Read a 32-bit value at 'offset', if there are 4 bytes left. */
static bool read_u32(const uint8_t* data, uint32_t size, uint32_t* offset, void* out) {
    if (size - *offset < sizeof(uint32_t)) {
        return false;
    }
    memcpy(out, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    return true;
}

/* Copy every token string into one block. File format, after the leading
   int max_token_length: per token, float score, int length, the bytes. */
static bool parse_vocab(tokenizer_t* t, const uint8_t* data, uint32_t size, uint32_t offset) {
    char* next_string = t->strings;

    for (int i = 0; i < t->vocab_size; i++) {
        int length;
        if (!read_u32(data, size, &offset, &t->vocab_scores[i]) ||
            !read_u32(data, size, &offset, &length) ||
            length < 0 || length > t->max_token_length || (uint32_t)length > size - offset) {
            return false;
        }
        memcpy(next_string, data + offset, length);
        next_string[length] = '\0';
        t->vocab[i] = next_string;
        t->vocab_length[i] = length;
        next_string += length + 1;
        offset += length;
    }
    return offset == size;      /* nothing may be left over */
}

bool tokenizer_init(tokenizer_t* t, const uint8_t* data, uint32_t size, int vocab_size) {
    memset(t, 0, sizeof(tokenizer_t));
    t->vocab_size = vocab_size;
    for (int i = 0; i < 256; i++) {
        t->byte_pieces[i * 2] = (char)i;
        t->byte_pieces[i * 2 + 1] = '\0';
    }

    uint32_t offset = 0;
    if (!read_u32(data, size, &offset, &t->max_token_length) || t->max_token_length <= 0) {
        serial_printf("llm: bad tokenizer header\n");
        return false;
    }
    t->vocab = kmalloc(vocab_size * sizeof(char*));
    t->vocab_length = kmalloc(vocab_size * sizeof(int));
    t->vocab_scores = kmalloc(vocab_size * sizeof(float));
    t->strings = kmalloc(size + vocab_size);    /* the bytes plus one '\0' each */
    if (!t->vocab || !t->vocab_length || !t->vocab_scores || !t->strings) {
        serial_printf("llm: out of memory for the tokenizer\n");
        tokenizer_free(t);
        return false;
    }
    if (!parse_vocab(t, data, size, offset)) {
        serial_printf("llm: tokenizer file doesn't match a %d-token vocabulary\n", vocab_size);
        tokenizer_free(t);
        return false;
    }
    return true;
}

void tokenizer_free(tokenizer_t* t) {
    kfree(t->vocab);
    kfree(t->vocab_length);
    kfree(t->vocab_scores);
    kfree(t->strings);
    memset(t, 0, sizeof(tokenizer_t));
}

/* ---- lookups ------------------------------------------------------ */

/* The token whose string is exactly s[0..length), or -1. */
static int str_lookup(const tokenizer_t* t, const char* s, int length) {
    for (int i = 0; i < t->vocab_size; i++) {
        if (t->vocab_length[i] == length && memcmp(t->vocab[i], s, length) == 0) {
            return i;
        }
    }
    return -1;
}

/* The token whose string is vocab[a] followed by vocab[b], or -1. */
static int pair_lookup(const tokenizer_t* t, int a, int b) {
    int len_a = t->vocab_length[a];
    int length = len_a + t->vocab_length[b];

    for (int i = 0; i < t->vocab_size; i++) {
        if (t->vocab_length[i] == length &&
            memcmp(t->vocab[i], t->vocab[a], len_a) == 0 &&
            memcmp(t->vocab[i] + len_a, t->vocab[b], length - len_a) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---- encoding ----------------------------------------------------- */

/* One token per UTF-8 character, or one per byte if the character isn't
   in the vocabulary (byte fallback). Returns the new token count. */
static int encode_chars(const tokenizer_t* t, const char* text, int* tokens, int n) {
    const char* c = text;

    while (*c) {
        int length = 1;
        /* Continuation bytes look like 10xxxxxx: keep them with their lead byte. */
        while (length < UTF8_MAX_BYTES && (c[length] & 0xC0) == 0x80) {
            length++;
        }
        int id = str_lookup(t, c, length);
        if (id != -1) {
            tokens[n++] = id;
        } else {
            for (int i = 0; i < length; i++) {
                tokens[n++] = (unsigned char)c[i] + BYTE_TOKEN_OFFSET;
            }
        }
        c += length;
    }
    return n;
}

/* The pair at i with the best merge score, or -1 if no pair merges. Ties
   go to the leftmost pair, as in run.c. */
static int best_merge(const tokenizer_t* t, const int* merged, int pairs) {
    int best = -1;
    float best_score = -1e10f;

    for (int i = 0; i < pairs; i++) {
        if (merged[i] != -1 && t->vocab_scores[merged[i]] > best_score) {
            best_score = t->vocab_scores[merged[i]];
            best = i;
        }
    }
    return best;
}

/* Merge pairs from tokens[first..n) until nothing merges. merged[i] is the
   token that tokens[i] and tokens[i+1] would merge into, or -1. */
static int merge_pairs(const tokenizer_t* t, int* tokens, int* merged, int first, int n) {
    for (int i = first; i < n - 1; i++) {
        merged[i] = pair_lookup(t, tokens[i], tokens[i + 1]);
    }
    for (;;) {
        int best = best_merge(t, merged + first, n - 1 - first);
        if (best < 0) {
            return n;
        }
        best += first;
        tokens[best] = merged[best];
        n--;
        for (int i = best + 1; i < n; i++) {
            tokens[i] = tokens[i + 1];
            merged[i] = merged[i + 1];
        }
        /* Only the pairs touching the new token changed. */
        if (best > first) {
            merged[best - 1] = pair_lookup(t, tokens[best - 1], tokens[best]);
        }
        if (best < n - 1) {
            merged[best] = pair_lookup(t, tokens[best], tokens[best + 1]);
        }
    }
}

int tokenizer_encode(tokenizer_t* t, const char* text, bool bos, bool dummy_prefix,
                     int* tokens, int max_tokens) {
    /* Worst case: one token per byte, plus BOS and the dummy prefix. */
    int capacity = strlen(text) + 2;
    int* work = kmalloc(capacity * sizeof(int));
    int* merged = kmalloc(capacity * sizeof(int));
    if (!work || !merged) {
        kfree(work);
        kfree(merged);
        return -1;
    }

    int n = 0;
    if (bos) {
        work[n++] = TOKEN_BOS;
    }
    int first = n;              /* BOS never merges */
    if (dummy_prefix && text[0] != '\0') {
        int space = str_lookup(t, " ", 1);
        if (space != -1) {
            work[n++] = space;
        }
    }
    n = encode_chars(t, text, work, n);
    n = merge_pairs(t, work, merged, first, n);

    if (n <= max_tokens) {
        memcpy(tokens, work, n * sizeof(int));
    } else {
        n = -1;
    }
    kfree(work);
    kfree(merged);
    return n;
}

/* ---- decoding ----------------------------------------------------- */

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Byte tokens are named "<0xHH>". Returns the byte, or -1 for other names. */
static int byte_piece_value(const char* piece) {
    if (strlen(piece) != BYTE_PIECE_LEN || piece[0] != '<' || piece[1] != '0' ||
        piece[2] != 'x' || piece[5] != '>') {
        return -1;
    }
    int high = hex_digit(piece[3]);
    int low = hex_digit(piece[4]);
    return (high < 0 || low < 0) ? -1 : high * 16 + low;
}

const char* tokenizer_decode(tokenizer_t* t, int prev_token, int token) {
    const char* piece = t->vocab[token];

    /* After BOS, SentencePiece drops the leading space (llama2.c PR #89). */
    if (prev_token == TOKEN_BOS && piece[0] == ' ') {
        piece++;
    }
    int byte = byte_piece_value(piece);
    if (byte >= 0) {
        piece = t->byte_pieces + byte * 2;
    }
    return piece;
}
