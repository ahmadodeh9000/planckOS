#ifndef fat12_h__
#define fat12_h__

#include "common.h"

typedef struct {
    uint8_t jump[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;      // always 512 
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;      // sectors before FAT
    uint8_t fat_count;              // usually 2
    uint16_t root_entry_count;
    uint16_t total_sectors;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

} __attribute__((packed)) fat12_boot_t;


typedef struct {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attributes;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t start_cluster;   // only 16 bits in FAT12, no cluster_high
    uint32_t file_size;
} __attribute__((packed)) fat12_entry_t;



/* FAT12 functions */

void     fat12_init();
void     fat12_list();
uint32_t fat12_read(const char *name, uint8_t *buffer);
int      fat12_write(const char *name, uint8_t *data, uint32_t size);
int      fat12_delete(const char *name);


#endif