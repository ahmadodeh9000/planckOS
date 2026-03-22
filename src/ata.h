#ifndef ata_h__
#define ata_h__

#include "common.h"

// Read 'count' sectors starting at 'lba' into 'buffer'
// LBA = Logical Block Address, just a sector number starting at 0
void ata_read(uint32_t lba, uint8_t count, uint16_t *buffer);

// Write 'count' sectors starting at 'lba' from 'buffer'
void ata_write(uint32_t lba, uint8_t count, uint16_t *buffer);

// Returns 1 if a disk is present, 0 if not
int ata_detect();

#endif