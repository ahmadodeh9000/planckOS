/*
 * notepad.h - a simple text editor for files on the FAT12 disk.
 */
#ifndef notepad_h__
#define notepad_h__

/* Open an empty, unnamed document. */
void notepad_open(void);

/* Open 'name' from the disk (used by the Files app). */
void notepad_open_file(const char* name);

#endif
