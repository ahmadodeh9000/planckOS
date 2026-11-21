#include "emit.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void pcl_emit_abort(const char* msg) {
    fprintf(stderr, "PCL EMITTER ERROR: %s\n", msg);
    exit(-1);
}

static void pcl_buffer_append(pcl_buffer_t* buffer, const char* text, size_t length) {
    if (buffer->length + length + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 1024;
        while (buffer->length + length + 1 > capacity) {
            capacity *= 2;
        }
        char* data = realloc(buffer->data, capacity);
        if (!data) {
            pcl_emit_abort("OUT OF MEMORY");
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static void pcl_buffer_vformat(pcl_buffer_t* buffer, const char* fmt, va_list args, int newline) {
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    char* text = malloc(length + 1);
    if (!text) {
        pcl_emit_abort("OUT OF MEMORY");
    }
    vsnprintf(text, length + 1, fmt, args);
    pcl_buffer_append(buffer, text, length);
    free(text);

    if (newline) {
        pcl_buffer_append(buffer, "\n", 1);
    }
}

void pcl_emitter_init(pcl_emitter_t* emitter, const char* full_path) {
    memset(emitter, 0, sizeof(*emitter));
    emitter->full_path = full_path;
}

void pcl_emit(pcl_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pcl_buffer_vformat(&emitter->code, fmt, args, 0);
    va_end(args);
}

void pcl_emit_line(pcl_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pcl_buffer_vformat(&emitter->code, fmt, args, 1);
    va_end(args);
}

void pcl_header_line(pcl_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pcl_buffer_vformat(&emitter->header, fmt, args, 1);
    va_end(args);
}

void pcl_data_line(pcl_emitter_t* emitter, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    pcl_buffer_vformat(&emitter->data, fmt, args, 1);
    va_end(args);
}

int pcl_new_label(pcl_emitter_t* emitter) {
    return ++emitter->label_count;
}

void pcl_write_file(pcl_emitter_t* emitter) {
    FILE* file = fopen(emitter->full_path, "w");
    if (!file) {
        pcl_emit_abort("COULD NOT OPEN OUTPUT FILE");
    }
    if (emitter->header.length) fwrite(emitter->header.data, 1, emitter->header.length, file);
    if (emitter->code.length)   fwrite(emitter->code.data, 1, emitter->code.length, file);
    if (emitter->data.length)   fwrite(emitter->data.data, 1, emitter->data.length, file);
    fclose(file);
}