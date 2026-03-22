#include "fat32.h"
#include "ata.h"
#include "util.h"
#include "kmalloc.h"

// We store these after reading the boot sector
static uint32_t fat_start;      // first sector of the FAT table
static uint32_t data_start;     // first sector of the data region
static uint32_t root_cluster;   // cluster number of root directory
static uint8_t  sectors_per_cluster;
static uint32_t bytes_per_cluster;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Convert a cluster number to the LBA sector it starts at
static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start + (cluster - 2) * sectors_per_cluster;
}

// Read the FAT table entry for a given cluster
// This tells us: "what is the next cluster in this file's chain?"
static uint32_t fat_next_cluster(uint32_t cluster) {
    // Each FAT32 entry is 4 bytes
    // Figure out which sector of the FAT table contains this entry
    uint32_t fat_offset  = cluster * 4;
    uint32_t fat_sector  = fat_start + (fat_offset / 512);
    uint32_t fat_index   = (fat_offset % 512) / 4;

    // Read that sector
    uint16_t buffer[256];  // 512 bytes = 256 words
    ata_read(fat_sector, 1, buffer);

    // Cast to uint32_t array and read the entry
    uint32_t *fat = (uint32_t *)buffer;
    return fat[fat_index] & 0x0FFFFFFF;  // mask top 4 bits, they're reserved
}

// ── Init ──────────────────────────────────────────────────────────────────────

int fat32_init() {
    // Read sector 0 — the boot sector
    uint16_t buffer[256];
    ata_read(0, 1, buffer);

    fat32_boot_t *boot = (fat32_boot_t *)buffer;

    // Verify it's actually FAT32
    if (boot->bytes_per_sector != 512) {
        printf("FAT32: bad sector size\n");
        return 0;
    }

    // Calculate where things are on disk
    fat_start           = boot->reserved_sectors;
    data_start          = fat_start + (boot->fat_count * boot->sectors_per_fat_32);
    root_cluster        = boot->root_cluster;
    sectors_per_cluster = boot->sectors_per_cluster;
    bytes_per_cluster   = sectors_per_cluster * 512;

    printf("FAT32: initialized\n");
    printf("  fat_start=%d data_start=%d root_cluster=%d\n",
        fat_start, data_start, root_cluster);

    return 1;
}

// ── List root directory ───────────────────────────────────────────────────────

void fat32_list_root() {
    // Allocate a buffer big enough for one cluster
    uint8_t *buffer = (uint8_t *)kmalloc(bytes_per_cluster);

    uint32_t cluster = root_cluster;

    // Walk the cluster chain of the root directory
    while (cluster < 0x0FFFFFF8) {  // 0x0FFFFFF8 = end of chain marker
        uint32_t lba = cluster_to_lba(cluster);
        ata_read(lba, sectors_per_cluster, (uint16_t *)buffer);

        // Each directory entry is 32 bytes
        fat32_entry_t *entries = (fat32_entry_t *)buffer;
        uint32_t entry_count   = bytes_per_cluster / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < entry_count; i++) {
            // Skip empty entries and long filename entries
            if (entries[i].name[0] == 0x00) goto done;  // no more entries
            if (entries[i].name[0] == 0xE5) continue;   // deleted file
            if (entries[i].attributes == 0x0F) continue; // long filename

            // Print name + extension
            char name[13];
            int n = 0;
            for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++)
                name[n++] = entries[i].name[j];
            if (entries[i].ext[0] != ' ') {
                name[n++] = '.';
                for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++)
                    name[n++] = entries[i].ext[j];
            }
            name[n] = '\0';

            if (entries[i].attributes & 0x10)
                printf("  [DIR]  %s\n", name);
            else
                printf("  [FILE] %s (%d bytes)\n", name, entries[i].file_size);
        }

        cluster = fat_next_cluster(cluster);
    }
done:
    kfree(buffer);
}

// ── Read file ─────────────────────────────────────────────────────────────────

uint32_t fat32_read_file(const char *filename, uint8_t *buffer) {
    // Parse filename into 8.3 format for comparison
    // e.g. "HELLO.TXT" → name="HELLO   " ext="TXT"
    char name83[11];
    for (int i = 0; i < 11; i++) name83[i] = ' ';

    int dot = -1;
    for (int i = 0; filename[i]; i++) {
        if (filename[i] == '.') { dot = i; break; }
    }

    if (dot == -1) {
        // No extension
        for (int i = 0; filename[i] && i < 8; i++)
            name83[i] = filename[i];
    } else {
        for (int i = 0; i < dot && i < 8; i++)
            name83[i] = filename[i];
        for (int i = 0; filename[dot+1+i] && i < 3; i++)
            name83[8+i] = filename[dot+1+i];
    }

    // Search root directory for this file
    uint8_t *dir_buf = (uint8_t *)kmalloc(bytes_per_cluster);
    uint32_t cluster = root_cluster;
    uint32_t file_cluster = 0;
    uint32_t file_size    = 0;

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        ata_read(lba, sectors_per_cluster, (uint16_t *)dir_buf);

        fat32_entry_t *entries = (fat32_entry_t *)dir_buf;
        uint32_t entry_count   = bytes_per_cluster / sizeof(fat32_entry_t);

        for (uint32_t i = 0; i < entry_count; i++) {
            if (entries[i].name[0] == 0x00) goto not_found;
            if (entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == 0x0F) continue;

            // Compare 8.3 name
            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (entries[i].name[j] != name83[j]) { match = 0; break; }
            }

            if (match) {
                file_cluster = ((uint32_t)entries[i].cluster_high << 16)
                             | entries[i].cluster_low;
                file_size    = entries[i].file_size;
                goto found;
            }
        }
        cluster = fat_next_cluster(cluster);
    }

not_found:
    kfree(dir_buf);
    printf("FAT32: file not found\n");
    return 0;

found:
    kfree(dir_buf);

    // Read the file cluster by cluster into buffer
    uint32_t bytes_read = 0;
    cluster = file_cluster;

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        ata_read(lba, sectors_per_cluster, (uint16_t *)(buffer + bytes_read));
        bytes_read += bytes_per_cluster;
        cluster = fat_next_cluster(cluster);
    }

    return file_size;
}