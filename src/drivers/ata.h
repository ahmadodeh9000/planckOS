/*
 * ata.h - ATA PIO driver for the data disk (primary slave).
 *
 * Sectors are 512 bytes; 'lba' is the sector number starting at 0.
 * Buffers are uint16_t because the data port transfers 16 bits at a time.
 */
#ifndef ata_h__
#define ata_h__

#include "lib/common.h"

#define ATA_SECTOR_SIZE 512

bool ata_detect(void);
void ata_read(uint32_t lba, uint8_t count, uint16_t* buffer);
void ata_write(uint32_t lba, uint8_t count, uint16_t* buffer);

#endif
