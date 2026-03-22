#ifndef fat32_h__
#define fat32_h__

#include "common.h"

// The boot sector — sits at sector 0, tells us everything about the filesystem
typedef struct {
    uint8_t  jump[3];           // boot jump instruction
    uint8_t  oem[8];            // OEM name
    uint16_t bytes_per_sector;  // almost always 512
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;  // sectors before the FAT table
    uint8_t  fat_count;         // number of FAT tables (usually 2)
    uint16_t root_entry_count;  // 0 for FAT32
    uint16_t total_sectors_16;  // 0 for FAT32
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;// 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // FAT32 extended fields
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;      // cluster number of root directory
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];        // "FAT32   "
} __attribute__((packed)) fat32_boot_t;

// One directory entry — 32 bytes
typedef struct {
    uint8_t  name[8];           // filename, space padded
    uint8_t  ext[3];            // extension, space padded
    uint8_t  attributes;        // 0x10 = directory, 0x20 = file
    uint8_t  reserved;
    uint8_t  created_tenths;
    uint16_t created_time;
    uint16_t created_date;
    uint16_t accessed_date;
    uint16_t cluster_high;      // high 16 bits of starting cluster
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t cluster_low;       // low 16 bits of starting cluster
    uint32_t file_size;         // size in bytes
} __attribute__((packed)) fat32_entry_t;

// Initialize the FAT32 driver — reads the boot sector
int fat32_init();

// Read a file into buffer — returns file size or 0 if not found
// filename format: "FILENAME", "FILENAMEXT" (8+3, uppercase, no dot)
uint32_t fat32_read_file(const char *filename, uint8_t *buffer);

// List all files in the root directory
void fat32_list_root();


// Write a file
int fat32_write_file(const char *filename, uint8_t *data, uint32_t size);

// Delete a file
int fat32_delete_file(const char *filename);

#endif