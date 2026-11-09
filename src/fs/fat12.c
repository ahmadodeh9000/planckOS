/*
 * fat12.c - FAT12 driver on top of ata.c.
 *
 * Disk layout (all sizes come from the boot sector, the BPB):
 *
 *   | boot sector | FAT copies | root directory | data clusters 2, 3, ... |
 *
 * The whole FAT is kept in RAM (fat_buffer) and every change to it is
 * written straight back to the first FAT copy. The root directory is read
 * from disk for each operation. A file is a chain of clusters: the FAT
 * entry of each cluster holds the number of the next one, or an
 * end-of-chain marker.
 */
#include "fs/fat12.h"
#include "drivers/ata.h"
#include "drivers/serial.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define FAT12_FREE              0x000
#define FAT12_END_OF_CHAIN      0xFFF   /* what we write to end a chain   */
#define FAT12_CHAIN_END_MIN     0xFF8   /* any value >= this ends a chain */
#define FAT12_FIRST_CLUSTER     2       /* clusters 0 and 1 are reserved  */

#define NAME_LEN                8
#define EXT_LEN                 3
#define NAME_83_LEN             (NAME_LEN + EXT_LEN)

#define ENTRY_END_OF_DIR        0x00    /* first name byte: no more entries */
#define ENTRY_DELETED           0xE5    /* first name byte: free slot       */
#define ATTR_LONG_NAME          0x0F
#define ATTR_ARCHIVE            0x20

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;      /* sectors before the first FAT */
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors;         /* 0 if the volume needs total_sectors_32 */
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed)) fat12_boot_t;

typedef struct {
    uint8_t  name[NAME_LEN];
    uint8_t  ext[EXT_LEN];
    uint8_t  attributes;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t start_cluster;         /* FAT12 has no high cluster word */
    uint32_t file_size;
} __attribute__((packed)) fat12_entry_t;

/* Layout, filled in by fat12_init() from the boot sector */
static uint32_t fat_start;          /* LBA of the first FAT          */
static uint32_t fat_sectors;
static uint32_t root_start;         /* LBA of the root directory     */
static uint32_t root_sectors;
static uint32_t root_entries;
static uint32_t data_start;         /* LBA of cluster 2              */
static uint8_t  sectors_per_cluster;
static uint32_t cluster_size;       /* bytes                         */
static uint32_t total_clusters;
static uint8_t* fat_buffer;         /* the whole FAT, in RAM         */
static bool     ready;              /* fat12_init found a volume     */

/* ---- names -------------------------------------------------------- */

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

/* "foo.txt" -> "FOO     TXT" (11 bytes, space padded, no '\0'). */
static void name_to_83(const char* in, char* out) {
    int dot = -1;

    memset(out, ' ', NAME_83_LEN);
    for (int i = 0; in[i]; i++) {
        if (in[i] == '.') {
            dot = i;
            break;
        }
    }

    int base_len = dot == -1 ? (int)strlen(in) : dot;
    for (int i = 0; i < base_len && i < NAME_LEN; i++) {
        out[i] = to_upper(in[i]);
    }
    if (dot != -1) {
        for (int i = 0; in[dot + 1 + i] && i < EXT_LEN; i++) {
            out[NAME_LEN + i] = to_upper(in[dot + 1 + i]);
        }
    }
}

/* "FOO     TXT" -> "FOO.TXT" */
static void entry_display_name(const fat12_entry_t* e, char* out) {
    int n = 0;

    for (int i = 0; i < NAME_LEN && e->name[i] != ' '; i++) {
        out[n++] = e->name[i];
    }
    if (e->ext[0] != ' ') {
        out[n++] = '.';
        for (int i = 0; i < EXT_LEN && e->ext[i] != ' '; i++) {
            out[n++] = e->ext[i];
        }
    }
    out[n] = '\0';
}

static bool entry_is_file(const fat12_entry_t* e) {
    return e->name[0] != ENTRY_DELETED && e->attributes != ATTR_LONG_NAME;
}

static bool entry_matches(const fat12_entry_t* e, const char* name_83) {
    return memcmp(e->name, name_83, NAME_LEN) == 0 &&
           memcmp(e->ext, name_83 + NAME_LEN, EXT_LEN) == 0;
}

/* ---- the FAT ------------------------------------------------------ */

/*
 * FAT12 packs two 12-bit entries into three bytes, so entry N starts at
 * byte N + N/2. Even entries use the low 12 bits of the 16-bit word there,
 * odd entries the high 12 bits.
 */
static uint16_t fat12_get(uint32_t cluster) {
    uint32_t offset = cluster + cluster / 2;
    uint16_t word = fat_buffer[offset] | (fat_buffer[offset + 1] << 8);

    return (cluster % 2 == 0) ? (word & 0x0FFF) : (word >> 4);
}

/* Write the FAT sector holding byte 'offset' back to disk. */
static void fat_flush_sector_of(uint32_t offset) {
    uint32_t sector = offset / ATA_SECTOR_SIZE;

    if (sector < fat_sectors) {
        ata_write(fat_start + sector, 1, (uint16_t*)(fat_buffer + sector * ATA_SECTOR_SIZE));
    }
}

static void fat12_set(uint32_t cluster, uint16_t value) {
    uint32_t offset = cluster + cluster / 2;

    if (cluster % 2 == 0) {
        fat_buffer[offset]     = value & 0xFF;
        fat_buffer[offset + 1] = (fat_buffer[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    } else {
        fat_buffer[offset]     = (fat_buffer[offset] & 0x0F) | ((value & 0x0F) << 4);
        fat_buffer[offset + 1] = (value >> 4) & 0xFF;
    }

    /* An entry can straddle two sectors; flush both if so. */
    fat_flush_sector_of(offset);
    if ((offset + 1) / ATA_SECTOR_SIZE != offset / ATA_SECTOR_SIZE) {
        fat_flush_sector_of(offset + 1);
    }
}

static bool cluster_in_chain(uint32_t cluster) {
    return cluster >= FAT12_FIRST_CLUSTER && cluster < FAT12_CHAIN_END_MIN;
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start + (cluster - FAT12_FIRST_CLUSTER) * sectors_per_cluster;
}

/* Take one free cluster and mark it end-of-chain. Returns 0 if full. */
static uint32_t alloc_cluster(void) {
    for (uint32_t i = FAT12_FIRST_CLUSTER; i < total_clusters + FAT12_FIRST_CLUSTER; i++) {
        if (fat12_get(i) == FAT12_FREE) {
            fat12_set(i, FAT12_END_OF_CHAIN);
            return i;
        }
    }
    return 0;
}

static void free_chain(uint32_t cluster) {
    while (cluster_in_chain(cluster)) {
        uint32_t next = fat12_get(cluster);
        fat12_set(cluster, FAT12_FREE);
        cluster = next;
    }
}

/* Allocate a linked chain of 'count' clusters. Returns its first cluster,
   or 0 (with nothing left allocated) if the disk is full. */
static uint32_t alloc_chain(uint32_t count) {
    uint32_t first = 0;
    uint32_t prev = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t c = alloc_cluster();
        if (!c) {
            free_chain(first);
            return 0;
        }
        if (prev) {
            fat12_set(prev, c);
        } else {
            first = c;
        }
        prev = c;
    }
    return first;
}

/* ---- file data ---------------------------------------------------- */

static void read_chain(uint32_t cluster, uint32_t size, uint8_t* buffer) {
    uint8_t* tmp = kmalloc(cluster_size);
    uint32_t done = 0;

    /* Read whole clusters into tmp and copy only what is left of the file,
       so the last cluster can't overflow the caller's buffer. */
    while (cluster_in_chain(cluster) && done < size) {
        uint32_t chunk = size - done < cluster_size ? size - done : cluster_size;

        ata_read(cluster_to_lba(cluster), sectors_per_cluster, (uint16_t*)tmp);
        memcpy(buffer + done, tmp, chunk);
        done += chunk;
        cluster = fat12_get(cluster);
    }
    kfree(tmp);
}

static void write_chain(uint32_t cluster, const uint8_t* data, uint32_t size) {
    uint8_t* tmp = kmalloc(cluster_size);
    uint32_t done = 0;

    while (cluster_in_chain(cluster)) {
        uint32_t chunk = size - done < cluster_size ? size - done : cluster_size;

        memset(tmp, 0, cluster_size);
        memcpy(tmp, data + done, chunk);
        ata_write(cluster_to_lba(cluster), sectors_per_cluster, (uint16_t*)tmp);
        done += chunk;
        cluster = fat12_get(cluster);
    }
    kfree(tmp);
}

/* ---- root directory ----------------------------------------------- */

static fat12_entry_t* root_dir_load(void) {
    uint8_t* dir = kmalloc(root_sectors * ATA_SECTOR_SIZE);

    for (uint32_t i = 0; i < root_sectors; i++) {
        ata_read(root_start + i, 1, (uint16_t*)(dir + i * ATA_SECTOR_SIZE));
    }
    return (fat12_entry_t*)dir;
}

static void root_dir_save_sector(fat12_entry_t* dir, uint32_t sector) {
    uint8_t* bytes = (uint8_t*)dir;
    ata_write(root_start + sector, 1, (uint16_t*)(bytes + sector * ATA_SECTOR_SIZE));
}

static void root_dir_save(fat12_entry_t* dir) {
    for (uint32_t i = 0; i < root_sectors; i++) {
        root_dir_save_sector(dir, i);
    }
}

/* Index of the entry named 'name_83', or -1. */
static int root_dir_find(fat12_entry_t* dir, const char* name_83) {
    for (uint32_t i = 0; i < root_entries; i++) {
        if (dir[i].name[0] == ENTRY_END_OF_DIR) {
            break;
        }
        if (entry_is_file(&dir[i]) && entry_matches(&dir[i], name_83)) {
            return (int)i;
        }
    }
    return -1;
}

/* Index of the first deleted or never-used entry, or -1 if full. */
static int root_dir_free_slot(fat12_entry_t* dir) {
    for (uint32_t i = 0; i < root_entries; i++) {
        if (dir[i].name[0] == ENTRY_END_OF_DIR || dir[i].name[0] == ENTRY_DELETED) {
            return (int)i;
        }
    }
    return -1;
}

/* ---- public API --------------------------------------------------- */

void fat12_init(void) {
    uint16_t sector[ATA_SECTOR_SIZE / 2];
    ata_read(0, 1, sector);
    fat12_boot_t* b = (fat12_boot_t*)sector;

    if (b->bytes_per_sector != ATA_SECTOR_SIZE || b->sectors_per_cluster == 0) {
        serial_printf("fat12: sector 0 is not a FAT12 boot sector\n");
        return;
    }
    fat_sectors         = b->sectors_per_fat;
    fat_start           = b->reserved_sectors;
    root_start          = fat_start + b->fat_count * fat_sectors;
    root_entries        = b->root_entry_count;
    root_sectors        = (root_entries * sizeof(fat12_entry_t) + ATA_SECTOR_SIZE - 1) / ATA_SECTOR_SIZE;
    data_start          = root_start + root_sectors;
    sectors_per_cluster = b->sectors_per_cluster;
    cluster_size        = sectors_per_cluster * ATA_SECTOR_SIZE;

    uint32_t total_sectors = b->total_sectors ? b->total_sectors : b->total_sectors_32;
    total_clusters = (total_sectors - data_start) / sectors_per_cluster;

    fat_buffer = kmalloc(fat_sectors * ATA_SECTOR_SIZE);
    for (uint32_t i = 0; i < fat_sectors; i++) {
        ata_read(fat_start + i, 1, (uint16_t*)(fat_buffer + i * ATA_SECTOR_SIZE));
    }

    ready = true;
    serial_printf("fat12: %u clusters of %u bytes\n", total_clusters, cluster_size);
}

int fat12_list(fat12_file_t* files, int max) {
    if (!ready) {
        return 0;
    }
    fat12_entry_t* dir = root_dir_load();
    int count = 0;

    for (uint32_t i = 0; i < root_entries && dir[i].name[0] != ENTRY_END_OF_DIR; i++) {
        if (entry_is_file(&dir[i]) && count < max) {
            entry_display_name(&dir[i], files[count].name);
            files[count].size = dir[i].file_size;
            count++;
        }
    }
    kfree(dir);
    return count;
}

/* Look up 'name' and return its first cluster and size. */
static fat12_status fat12_find(const char* name, uint32_t* cluster, uint32_t* size) {
    if (!ready) {
        return FAT12_NO_DISK;
    }
    char name_83[NAME_83_LEN];
    name_to_83(name, name_83);

    fat12_entry_t* dir = root_dir_load();
    int index = root_dir_find(dir, name_83);
    if (index >= 0) {
        *cluster = dir[index].start_cluster;
        *size = dir[index].file_size;
    }
    kfree(dir);
    return index >= 0 ? FAT12_OK : FAT12_NOT_FOUND;
}

fat12_status fat12_read(const char* name, uint8_t* buffer, uint32_t capacity, uint32_t* bytes_read) {
    uint32_t cluster;
    uint32_t size;
    fat12_status status = fat12_find(name, &cluster, &size);

    *bytes_read = 0;
    if (status != FAT12_OK) {
        return status;
    }
    *bytes_read = size < capacity ? size : capacity;
    read_chain(cluster, *bytes_read, buffer);
    return FAT12_OK;
}

/* Find the entry to (over)write 'name_83' into, freeing any old data.
   Returns the entry index or -1 if the directory is full. */
static int fat12_claim_entry(fat12_entry_t* dir, const char* name_83) {
    int slot = root_dir_find(dir, name_83);
    if (slot >= 0) {
        free_chain(dir[slot].start_cluster);    /* overwrite: drop old data */
        return slot;
    }
    return root_dir_free_slot(dir);
}

fat12_status fat12_write(const char* name, const uint8_t* data, uint32_t size) {
    if (!ready) {
        return FAT12_NO_DISK;
    }
    char name_83[NAME_83_LEN];
    name_to_83(name, name_83);

    fat12_entry_t* dir = root_dir_load();
    int slot = fat12_claim_entry(dir, name_83);
    uint32_t clusters = size ? (size + cluster_size - 1) / cluster_size : 1;
    uint32_t first = slot >= 0 ? alloc_chain(clusters) : 0;
    if (!first) {
        kfree(dir);
        return slot < 0 ? FAT12_DIR_FULL : FAT12_DISK_FULL;
    }
    write_chain(first, data, size);

    memcpy(dir[slot].name, name_83, NAME_LEN);
    memcpy(dir[slot].ext, name_83 + NAME_LEN, EXT_LEN);
    dir[slot].attributes    = ATTR_ARCHIVE;
    dir[slot].start_cluster = first;
    dir[slot].file_size     = size;
    root_dir_save(dir);
    kfree(dir);
    return FAT12_OK;
}

fat12_status fat12_delete(const char* name) {
    if (!ready) {
        return FAT12_NO_DISK;
    }
    char name_83[NAME_83_LEN];
    name_to_83(name, name_83);

    fat12_entry_t* dir = root_dir_load();
    int index = root_dir_find(dir, name_83);
    if (index >= 0) {
        free_chain(dir[index].start_cluster);
        dir[index].name[0] = ENTRY_DELETED;
        root_dir_save_sector(dir, index * sizeof(fat12_entry_t) / ATA_SECTOR_SIZE);
    }
    kfree(dir);
    return index >= 0 ? FAT12_OK : FAT12_NOT_FOUND;
}

const char* fat12_status_message(fat12_status status) {
    switch (status) {
        case FAT12_OK:        return "ok";
        case FAT12_NOT_FOUND: return "file not found";
        case FAT12_DIR_FULL:  return "root directory full";
        case FAT12_DISK_FULL: return "disk full";
        case FAT12_NO_DISK:   return "no disk";
    }
    return "unknown error";
}
