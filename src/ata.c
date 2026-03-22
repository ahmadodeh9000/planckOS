#include "ata.h"
#include "util.h"

// ATA port numbers for primary bus
#define ATA_DATA        0x1F0   // read/write data
#define ATA_ERROR       0x1F1   // error info
#define ATA_SECTOR_COUNT 0x1F2  // how many sectors to read/write
#define ATA_LBA_LOW     0x1F3   // LBA bits 0-7
#define ATA_LBA_MID     0x1F4   // LBA bits 8-15
#define ATA_LBA_HIGH    0x1F5   // LBA bits 16-23
#define ATA_DRIVE       0x1F6   // drive select + LBA bits 24-27
#define ATA_STATUS      0x1F7   // status (read)
#define ATA_COMMAND     0x1F7   // command (write)

// Status bits
#define ATA_SR_BSY      0x80    // drive is busy
#define ATA_SR_DRQ      0x08    // data ready to transfer

// Commands
#define ATA_CMD_READ    0x20    // read sectors
#define ATA_CMD_WRITE   0x30    // write sectors

// Wait until drive is not busy and data is ready
static void ata_wait() {
    // Wait for BSY to clear
    while (inportb(ATA_STATUS) & ATA_SR_BSY);
    // Wait for DRQ to set (data ready)
    while (!(inportb(ATA_STATUS) & ATA_SR_DRQ));
}

int ata_detect() {
    outportb(ATA_DRIVE, 0xA0);  // select master drive
    outportb(ATA_LBA_LOW,  0);
    outportb(ATA_LBA_MID,  0);
    outportb(ATA_LBA_HIGH, 0);
    outportb(ATA_COMMAND, 0xEC); // IDENTIFY command

    uint8_t status = inportb(ATA_STATUS);
    if (status == 0) return 0;   // no drive

    // Wait for BSY to clear
    while (inportb(ATA_STATUS) & ATA_SR_BSY);

    // Check if it's actually ATA (not ATAPI)
    if (inportb(ATA_LBA_MID) != 0 || inportb(ATA_LBA_HIGH) != 0)
        return 0;

    // Wait for DRQ or ERR
    while (1) {
        status = inportb(ATA_STATUS);
        if (status & ATA_SR_DRQ) break;
        if (status & 0x01) return 0;  // error
    }

    // Read the 256 words of identify data (we don't use it, just drain it)
    for (int i = 0; i < 256; i++)
        inportw(ATA_DATA);

    return 1;
}

void ata_read(uint32_t lba, uint8_t count, uint16_t *buffer) {
    // Tell the drive what to read using LBA28 mode
    outportb(ATA_DRIVE,        0xE0 | ((lba >> 24) & 0x0F)); // master + LBA bits 24-27
    outportb(ATA_SECTOR_COUNT, count);
    outportb(ATA_LBA_LOW,      lba & 0xFF);           // bits 0-7
    outportb(ATA_LBA_MID,      (lba >> 8)  & 0xFF);   // bits 8-15
    outportb(ATA_LBA_HIGH,     (lba >> 16) & 0xFF);   // bits 16-23
    outportb(ATA_COMMAND,      ATA_CMD_READ);

    // Read 'count' sectors, each sector = 256 words = 512 bytes
    for (int s = 0; s < count; s++) {
        ata_wait();
        for (int i = 0; i < 256; i++)
            buffer[s * 256 + i] = inportw(ATA_DATA);
    }
}

void ata_write(uint32_t lba, uint8_t count, uint16_t *buffer) {
    outportb(ATA_DRIVE,        0xE0 | ((lba >> 24) & 0x0F));
    outportb(ATA_SECTOR_COUNT, count);
    outportb(ATA_LBA_LOW,      lba & 0xFF);
    outportb(ATA_LBA_MID,      (lba >> 8)  & 0xFF);
    outportb(ATA_LBA_HIGH,     (lba >> 16) & 0xFF);
    outportb(ATA_COMMAND,      ATA_CMD_WRITE);

    for (int s = 0; s < count; s++) {
        ata_wait();
        for (int i = 0; i < 256; i++)
            outportw(ATA_DATA, buffer[s * 256 + i]);
    }
}