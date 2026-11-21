/*
 * emit.c - the Higgson emitter (see emit.h).
 *
 * The buffers live in the compiler's arena, which never frees anything on
 * its own: a buffer that grows moves to a new, twice as big block and
 * leaves the old one behind until the whole arena is freed. Doubling keeps
 * that waste smaller than the final text.
 */
#include "higgson/emit.h"
#include "higgson/hg.h"
#include "higgson/util.h"

#define BUFFER_FIRST_CAPACITY 1024

/* Make room for 'length' more characters and the '\0'. */
static void hg_buffer_reserve(hg_buffer_t* buffer, size_t length) {
    if (buffer->length + length + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : BUFFER_FIRST_CAPACITY;
        while (buffer->length + length + 1 > capacity) {
            capacity *= 2;
        }
        char* data = hg_alloc(capacity);
        if (buffer->length) {
            hg_util_memcpy(data, buffer->data, buffer->length);
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
}

static void hg_buffer_append(hg_buffer_t* buffer, const char* text, size_t length) {
    hg_buffer_reserve(buffer, length);
    hg_util_memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

/* Measures the text first, then formats it straight into the buffer: a
   temporary copy would stay in the arena for the rest of the compile. */
static void hg_buffer_vformat(hg_buffer_t* buffer, const char* fmt, va_list args, int newline) {
    va_list copy;
    va_copy(copy, args);
    int length = hg_platform_vformat(NULL, 0, fmt, copy);
    va_end(copy);

    hg_buffer_reserve(buffer, length);
    hg_platform_vformat(buffer->data + buffer->length, length + 1, fmt, args);
    buffer->length += length;

    if (newline) {
        hg_buffer_append(buffer, "\n", 1);
    }
}

void hg_emitter_init(hg_emitter_t* emitter) {
    *emitter = (hg_emitter_t){ 0 };
    emitter->target = &emitter->code;
}

void hg_emitter_section(hg_emitter_t* emitter, hg_section_t section) {
    emitter->target = section == HG_SECTION_FUNCS ? &emitter->funcs : &emitter->code;
}

void hg_emitter_symbol(hg_emitter_t* emitter) {
    if (++emitter->symbol_count > HG_ASM_MAX_SYMBOLS) {
        hg_error(0, "program too big: planckOS's assembler holds %d labels and names",
                 HG_ASM_MAX_SYMBOLS);
    }
}

void hg_emit(hg_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    hg_buffer_vformat(emitter->target, fmt, args, 0);
    va_end(args);
}

void hg_emit_line(hg_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    hg_buffer_vformat(emitter->target, fmt, args, 1);
    va_end(args);
}

void hg_header_line(hg_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    hg_buffer_vformat(&emitter->header, fmt, args, 1);
    va_end(args);
}

void hg_data_line(hg_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    hg_buffer_vformat(&emitter->data, fmt, args, 1);
    va_end(args);
}

int hg_new_label(hg_emitter_t* emitter) {
    hg_emitter_symbol(emitter);
    return ++emitter->label_count;
}

/* Takes the place of pcl_write_file: the compiler core can't open files,
   so it hands the text to whoever called it (the higgson command, or
   tools/higgson-host on a computer). */
const char* hg_emitter_text(hg_emitter_t* emitter, uint32_t* length) {
    hg_buffer_t text = { 0 };

    hg_buffer_append(&text, "", 0);     /* never NULL, even when empty */
    if (emitter->header.length) hg_buffer_append(&text, emitter->header.data, emitter->header.length);
    if (emitter->code.length)   hg_buffer_append(&text, emitter->code.data, emitter->code.length);
    if (emitter->funcs.length)  hg_buffer_append(&text, emitter->funcs.data, emitter->funcs.length);
    if (emitter->data.length)   hg_buffer_append(&text, emitter->data.data, emitter->data.length);
    *length = text.length;
    return text.data;
}
