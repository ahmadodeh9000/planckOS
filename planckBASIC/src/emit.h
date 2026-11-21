#ifndef _emit__h
#define _emit__h

#include <stddef.h>

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} pcl_buffer_t;

typedef struct {
    const char* full_path;
    pcl_buffer_t header;
    pcl_buffer_t code;
    pcl_buffer_t data;
    int label_count;
} pcl_emitter_t;

void pcl_emitter_init(pcl_emitter_t* emitter, const char* full_path);
void pcl_emit(pcl_emitter_t* emitter, const char* fmt, ...);
void pcl_emit_line(pcl_emitter_t* emitter, const char* fmt, ...);
void pcl_header_line(pcl_emitter_t* emitter, const char* fmt, ...);
void pcl_data_line(pcl_emitter_t* emitter, const char* fmt, ...);
int  pcl_new_label(pcl_emitter_t* emitter);
void pcl_write_file(pcl_emitter_t* emitter);

#endif