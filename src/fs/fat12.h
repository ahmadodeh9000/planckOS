/*
 * fat12.h - FAT12 file system on the ATA data disk (root directory only).
 *
 * File names use the 8.3 form ("NOTES.TXT"); lowercase input is
 * upper-cased. Subdirectories are not supported. Nothing here prints:
 * every call reports what happened with a fat12_status, and callers turn
 * that into a message with fat12_status_message().
 */
#ifndef fat12_h__
#define fat12_h__

#include "lib/common.h"

#define FAT12_NAME_MAX  13      /* "NAME.EXT" plus '\0' */

typedef enum {
    FAT12_OK,
    FAT12_NOT_FOUND,
    FAT12_DIR_FULL,
    FAT12_DISK_FULL,
    FAT12_NO_DISK,      /* no disk, or it isn't FAT12 */
} fat12_status;

typedef struct {
    char name[FAT12_NAME_MAX];
    uint32_t size;
} fat12_file_t;

void fat12_init(void);

/* Fill 'files' with up to 'max' files from the root directory. Returns
   how many were stored. */
int fat12_list(fat12_file_t* files, int max);

/* Read up to 'capacity' bytes of a file into 'buffer'. '*bytes_read' gets
   the number of bytes stored. */
fat12_status fat12_read(const char* name, uint8_t* buffer, uint32_t capacity, uint32_t* bytes_read);

/* Create or overwrite a file. */
fat12_status fat12_write(const char* name, const uint8_t* data, uint32_t size);

fat12_status fat12_delete(const char* name);

const char* fat12_status_message(fat12_status status);

#endif
