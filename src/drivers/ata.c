/*
 * ata.c - ATA PIO (programmed I/O) on the primary IDE bus.
 *
 * The CPU copies every 16-bit word through the data port itself; no DMA
 * and no interrupts. The data disk is the primary *slave*: QEMU puts
 * -drive index=1 (the old -hdb) there, and the boot CD-ROM sits on the
 * secondary bus.
 */
#include "drivers/ata.h"
#include "arch/io.h"

/* Primary bus I/O ports */
#define ATA_DATA            0x1F0
#define ATA_SECTOR_COUNT    0x1F2
#define ATA_LBA_LOW         0x1F3   /* LBA bits 0-7   */
#define ATA_LBA_MID         0x1F4   /* LBA bits 8-15  */
#define ATA_LBA_HIGH        0x1F5   /* LBA bits 16-23 */
#define ATA_DRIVE           0x1F6   /* drive select + LBA bits 24-27 */
#define ATA_STATUS          0x1F7   /* read  */
#define ATA_COMMAND         0x1F7   /* write */

/* Status bits */
#define ATA_SR_BSY          0x80    /* drive is busy               */
#define ATA_SR_DRQ          0x08    /* drive wants to transfer data */
#define ATA_SR_ERR          0x01

/* Commands */
#define ATA_CMD_READ        0x20
#define ATA_CMD_WRITE       0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

/* Drive select register: bit 4 picks the slave, bit 6 enables LBA mode,
   bits 5 and 7 must be set on older drives. */
#define ATA_SELECT_SLAVE        0xB0
#define ATA_SELECT_SLAVE_LBA    0xF0

#define WORDS_PER_SECTOR    (ATA_SECTOR_SIZE / 2)

static void ata_wait_not_busy(void) {
    while (inportb(ATA_STATUS) & ATA_SR_BSY) {
    }
}

/* Wait until the drive is ready to move one sector of data. */
static void ata_wait_data(void) {
    ata_wait_not_busy();
    while (!(inportb(ATA_STATUS) & ATA_SR_DRQ)) {
    }
}

bool ata_detect(void) {
    outportb(ATA_DRIVE, ATA_SELECT_SLAVE);
    outportb(ATA_LBA_LOW, 0);
    outportb(ATA_LBA_MID, 0);
    outportb(ATA_LBA_HIGH, 0);
    outportb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    if (inportb(ATA_STATUS) == 0) {
        return false;   /* nothing on the bus */
    }
    ata_wait_not_busy();

    /* ATAPI devices (CD-ROMs) put a signature in these two registers. */
    if (inportb(ATA_LBA_MID) != 0 || inportb(ATA_LBA_HIGH) != 0) {
        return false;
    }

    for (;;) {
        uint8_t status = inportb(ATA_STATUS);
        if (status & ATA_SR_DRQ) {
            break;
        }
        if (status & ATA_SR_ERR) {
            return false;
        }
    }

    /* The 256 words of IDENTIFY data must be read out, even unused. */
    for (int i = 0; i < WORDS_PER_SECTOR; i++) {
        inportw(ATA_DATA);
    }
    return true;
}

/* Select the slave in LBA28 mode and send the sector range and command. */
static void ata_start(uint32_t lba, uint8_t count, uint8_t command) {
    outportb(ATA_DRIVE, ATA_SELECT_SLAVE_LBA | ((lba >> 24) & 0x0F));
    outportb(ATA_SECTOR_COUNT, count);
    outportb(ATA_LBA_LOW, lba & 0xFF);
    outportb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outportb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outportb(ATA_COMMAND, command);
}

void ata_read(uint32_t lba, uint8_t count, uint16_t* buffer) {
    ata_start(lba, count, ATA_CMD_READ);

    for (int s = 0; s < count; s++) {
        ata_wait_data();
        for (int i = 0; i < WORDS_PER_SECTOR; i++) {
            buffer[s * WORDS_PER_SECTOR + i] = inportw(ATA_DATA);
        }
    }
}

void ata_write(uint32_t lba, uint8_t count, uint16_t* buffer) {
    ata_start(lba, count, ATA_CMD_WRITE);

    for (int s = 0; s < count; s++) {
        ata_wait_data();
        for (int i = 0; i < WORDS_PER_SECTOR; i++) {
            outportw(ATA_DATA, buffer[s * WORDS_PER_SECTOR + i]);
        }
    }

    /* Make the drive commit its write cache to the disk. */
    outportb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_not_busy();
}
