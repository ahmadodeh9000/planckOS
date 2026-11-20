/* fat12.h (host shim) - "the disk" is the directory in $IRIS_DIR. */
#ifndef fat12_h__
#define fat12_h__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define FAT12_NAME_MAX 13
typedef enum { FAT12_OK, FAT12_NOT_FOUND } fat12_status;

static inline FILE* fat12_open(const char* name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", getenv("IRIS_DIR") ? getenv("IRIS_DIR") : ".", name);
    return fopen(path, "rb");
}
static inline fat12_status fat12_size(const char* name, uint32_t* size) {
    FILE* f = fat12_open(name);
    if (!f) return FAT12_NOT_FOUND;
    fseek(f, 0, SEEK_END);
    *size = (uint32_t)ftell(f);
    fclose(f);
    return FAT12_OK;
}
static inline fat12_status fat12_read(const char* name, uint8_t* buffer, uint32_t capacity,
                                      uint32_t* got) {
    FILE* f = fat12_open(name);
    if (!f) return FAT12_NOT_FOUND;
    *got = (uint32_t)fread(buffer, 1, capacity, f);
    fclose(f);
    return FAT12_OK;
}
#endif
