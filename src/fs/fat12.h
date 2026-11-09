/*
 * fat12.h - FAT12 file system on the ATA data disk (root directory only).
 *
 * File names use the 8.3 form ("NOTES.TXT"); lowercase input is
 * upper-cased. Subdirectories are not supported.
 */
#ifndef fat12_h__
#define fat12_h__

#include "lib/common.h"

void fat12_init(void);

/* Print every file in the root directory with its size. */
void fat12_list(void);

/* Read a whole file into 'buffer', which must be big enough. Returns the
   file size, or 0 if the file doesn't exist. */
uint32_t fat12_read(const char* name, uint8_t* buffer);

/* Create or overwrite a file. Returns 1 on success, 0 on failure. */
int fat12_write(const char* name, uint8_t* data, uint32_t size);

/* Returns 1 if the file was deleted, 0 if it wasn't found. */
int fat12_delete(const char* name);

#endif
