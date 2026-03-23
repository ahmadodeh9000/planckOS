#include "fat12.h"
#include "ata.h"
#include "kmalloc.h"
#include "util.h"

/* ───────────────────────────────────────────────
   All layout values are derived from the BPB
   at init time and stored here.
─────────────────────────────────────────────── */
static uint32_t fat_start;          /* LBA of first FAT sector          */
static uint32_t root_start;         /* LBA of first root-dir sector     */
static uint32_t root_size;          /* root dir size in sectors         */
static uint32_t data_start;         /* LBA of cluster 2                 */
static uint8_t  sectors_per_cluster;
static uint32_t cluster_size;       /* bytes per cluster                */
static uint32_t total_clusters;     /* usable data clusters (2..N)      */

static uint8_t *fat_buffer;         /* in-RAM copy of the entire FAT    */
static uint32_t fat_sectors;        /* how many sectors the FAT occupies*/

/* ───────────────────────────────────────────────
   Helpers
─────────────────────────────────────────────── */

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

/* Convert a user-visible name ("FOO.TXT") to an 11-byte 8.3 field.
   The output is space-padded, not NUL-terminated. */
static void to_83(const char *in, char *out) {
    int i;
    for (i = 0; i < 11; i++) out[i] = ' ';

    int dot = -1;
    for (i = 0; in[i]; i++)
        if (in[i] == '.') { dot = i; break; }

    if (dot == -1) {
        /* no extension */
        for (i = 0; in[i] && i < 8; i++)
            out[i] = to_upper(in[i]);
    } else {
        for (i = 0; i < dot && i < 8; i++)
            out[i] = to_upper(in[i]);
        for (i = 0; in[dot + 1 + i] && i < 3; i++)
            out[8 + i] = to_upper(in[dot + 1 + i]);
    }
}

/* ───────────────────────────────────────────────
   FAT12 packed 12-bit entry access.

   For cluster N the 12-bit value sits at byte
   offset  N + (N/2)  in the FAT.

   Even cluster → low 12 bits of the 16-bit word.
   Odd  cluster → high 12 bits of the 16-bit word.
─────────────────────────────────────────────── */

static uint16_t fat12_get(uint32_t cluster) {
    uint32_t  offset = cluster + (cluster / 2);
    uint16_t  val    = (uint16_t)(fat_buffer[offset]) |
                       ((uint16_t)(fat_buffer[offset + 1]) << 8);
    return (cluster % 2 == 0) ? (val & 0x0FFF) : (val >> 4);
}

static void fat12_set(uint32_t cluster, uint16_t value) {
    uint32_t offset = cluster + (cluster / 2);

    if (cluster % 2 == 0) {
        /* low byte  = low 8 bits of value
           high byte = keep high nibble, replace low nibble with bits 11-8 */
        fat_buffer[offset]     = (uint8_t)(value & 0xFF);
        fat_buffer[offset + 1] = (uint8_t)((fat_buffer[offset + 1] & 0xF0) |
                                            ((value >> 8) & 0x0F));
    } else {
        /* low byte  = keep low nibble, replace high nibble with bits 3-0
           high byte = bits 11-4 of value */
        fat_buffer[offset]     = (uint8_t)((fat_buffer[offset] & 0x0F) |
                                            ((value & 0x0F) << 4));
        fat_buffer[offset + 1] = (uint8_t)((value >> 4) & 0xFF);
    }

    /* Write back the (at most two) affected FAT sectors immediately. */
    uint32_t sec = offset / 512;
    ata_write(fat_start + sec, 1,
              (uint16_t *)(fat_buffer + sec * 512));

    /* If the entry straddles a sector boundary, flush the next sector too. */
    if ((offset + 1) / 512 != sec) {
        uint32_t sec2 = sec + 1;
        if (sec2 < fat_sectors)
            ata_write(fat_start + sec2, 1,
                      (uint16_t *)(fat_buffer + sec2 * 512));
    }
}

/* ───────────────────────────────────────────────
   Cluster allocator / deallocator
─────────────────────────────────────────────── */

/* Allocate one free cluster and mark it EOF.
   Returns the cluster number, or 0 on failure. */
static uint32_t alloc_cluster(void) {
    for (uint32_t i = 2; i < total_clusters + 2; i++) {
        if (fat12_get(i) == 0x000) {
            fat12_set(i, 0xFFF);   /* mark EOF */
            return i;
        }
    }
    return 0;   /* disk full */
}

/* Free an entire cluster chain starting at 'cluster'. */
static void free_chain(uint32_t cluster) {
    while (cluster >= 2 && cluster < 0xFF8) {
        uint32_t next = fat12_get(cluster);
        fat12_set(cluster, 0x000);
        cluster = next;
    }
}

/* ───────────────────────────────────────────────
   Initialisation – must be called once at boot.
─────────────────────────────────────────────── */

void fat12_init(void) {
    uint16_t sector[256];
    ata_read(0, 1, sector);

    fat12_boot_t *b = (fat12_boot_t *)sector;

    fat_sectors        = b->sectors_per_fat;
    fat_start          = b->reserved_sectors;
    root_start         = fat_start + (b->fat_count * fat_sectors);
    root_size          = ((b->root_entry_count * 32) + 511) / 512;
    data_start         = root_start + root_size;
    sectors_per_cluster = b->sectors_per_cluster;
    cluster_size       = sectors_per_cluster * 512;

    /* FIX: derive total_clusters from the actual volume geometry,
       not from a FAT-buffer size approximation.                   */
    uint32_t total_sectors = b->total_sectors
                           ? b->total_sectors
                           : b->total_sectors_32;
    total_clusters = (total_sectors - data_start) / sectors_per_cluster;

    /* Load the entire FAT into RAM. */
    fat_buffer = (uint8_t *)kmalloc(fat_sectors * 512);
    for (uint32_t i = 0; i < fat_sectors; i++)
        ata_read(fat_start + i, 1,
                 (uint16_t *)(fat_buffer + i * 512));

    printf("FAT12 ready - %d clusters\n", total_clusters);
}

/* ───────────────────────────────────────────────
   Read a file from the root directory.

   'buffer' must be at least 'fat12_read' bytes.
   Caller should call fat12_size() first, or
   size the buffer conservatively.

   Returns the file's byte size, or 0 if not found.
─────────────────────────────────────────────── */

uint32_t fat12_read(const char *name, uint8_t *buffer) {
    char n83[11];
    to_83(name, n83);

    /* Load root directory. */
    uint8_t *dir = (uint8_t *)kmalloc(root_size * 512);
    for (uint32_t i = 0; i < root_size; i++)
        ata_read(root_start + i, 1,
                 (uint16_t *)(dir + i * 512));

    fat12_entry_t *e       = (fat12_entry_t *)dir;
    uint32_t       n_entries = root_size * 512 / sizeof(fat12_entry_t);
    uint32_t       cluster  = 0;
    uint32_t       size     = 0;

    for (uint32_t i = 0; i < n_entries; i++) {
        if (e[i].name[0] == 0x00) break;          /* no more entries */
        if ((uint8_t)e[i].name[0] == 0xE5) continue; /* deleted        */
        if (e[i].attributes == 0x0F) continue;    /* LFN              */

        int match = 1;
        for (int j = 0; j < 8; j++)
            if (e[i].name[j] != n83[j]) { match = 0; break; }
        if (match)
            for (int j = 0; j < 3; j++)
                if (e[i].ext[j] != n83[8 + j]) { match = 0; break; }

        if (match) {
            cluster = e[i].start_cluster;
            size    = e[i].file_size;
            break;
        }
    }

    kfree(dir);
    if (!cluster) return 0;

    /* Read cluster chain into buffer, stopping at file_size bytes. */
    uint32_t remaining = size;

    while (cluster >= 2 && cluster < 0xFF8) {
        uint32_t lba   = data_start + (cluster - 2) * sectors_per_cluster;
        uint32_t chunk = (remaining < cluster_size) ? remaining : cluster_size;

        /* Read one full cluster into a temp buffer, copy only 'chunk' bytes.
           This prevents overflowing the caller's buffer on the last cluster. */
        uint8_t *tmp = (uint8_t *)kmalloc(cluster_size);
        ata_read(lba, sectors_per_cluster, (uint16_t *)tmp);

        for (uint32_t i = 0; i < chunk; i++)
            buffer[(size - remaining) + i] = tmp[i];

        kfree(tmp);
        remaining -= chunk;
        cluster    = fat12_get(cluster);
    }

    return size;
}

/* ───────────────────────────────────────────────
   Write (create or overwrite) a file.

   Returns 1 on success, 0 on failure.
─────────────────────────────────────────────── */

int fat12_write(const char *name, uint8_t *data, uint32_t size) {
    char n83[11];
    to_83(name, n83);

    /* Load root directory. */
    uint8_t *dir = (uint8_t *)kmalloc(root_size * 512);
    for (uint32_t i = 0; i < root_size; i++)
        ata_read(root_start + i, 1,
                 (uint16_t *)(dir + i * 512));

    fat12_entry_t *e        = (fat12_entry_t *)dir;
    uint32_t       n_entries = root_size * 512 / sizeof(fat12_entry_t);

    int      slot        = -1;
    uint32_t old_cluster = 0;

    for (uint32_t i = 0; i < n_entries; i++) {
        if (e[i].name[0] == 0x00) {
            if (slot == -1) slot = (int)i;
            break;   /* no entry beyond here can match */
        }

        if ((uint8_t)e[i].name[0] == 0xE5) {
            if (slot == -1) slot = (int)i;
            continue;
        }

        if (e[i].attributes == 0x0F) continue;  /* LFN */

        int match = 1;
        for (int j = 0; j < 8; j++)
            if (e[i].name[j] != n83[j]) { match = 0; break; }
        if (match)
            for (int j = 0; j < 3; j++)
                if (e[i].ext[j] != n83[8 + j]) { match = 0; break; }

        if (match) {
            /* Overwriting an existing file. */
            old_cluster = e[i].start_cluster;
            slot        = (int)i;
            break;
        }
    }

    if (slot == -1) {
        printf("root directory full\n");
        kfree(dir);   /* FIX: was missing in original */
        return 0;
    }

    /* Free old cluster chain before allocating the new one. */
    if (old_cluster)
        free_chain(old_cluster);

    /* Allocate the needed clusters. */
    uint32_t needed = size ? (size + cluster_size - 1) / cluster_size : 1;
    uint32_t first  = 0;
    uint32_t prev   = 0;

    for (uint32_t i = 0; i < needed; i++) {
        uint32_t c = alloc_cluster();
        if (!c) {
            printf("disk full\n");
            /* Free whatever we already allocated this round. */
            if (first) free_chain(first);
            kfree(dir);   /* FIX: was missing in original */
            return 0;
        }
        if (!first) first = c;
        if (prev)   fat12_set(prev, (uint16_t)c);
        prev = c;
    }
    /* prev is already marked 0xFFF by alloc_cluster(). */

    /* Write data cluster by cluster. */
    uint32_t written = 0;
    uint32_t cur     = first;

    while (cur >= 2 && cur < 0xFF8) {
        uint32_t lba   = data_start + (cur - 2) * sectors_per_cluster;
        uint32_t chunk = cluster_size;
        if (written + chunk > size)
            chunk = size - written;

        /* Zero-fill a temp buffer, copy data into it, write the sector. */
        uint8_t *tmp = (uint8_t *)kmalloc(cluster_size);
        for (uint32_t i = 0; i < cluster_size; i++) tmp[i] = 0;
        for (uint32_t i = 0; i < chunk; i++)
            tmp[i] = data[written + i];

        ata_write(lba, sectors_per_cluster, (uint16_t *)tmp);
        kfree(tmp);

        written += chunk;
        cur      = fat12_get(cur);
    }

    /* Fill the directory entry. */
    for (int i = 0; i < 8; i++)
        e[slot].name[i] = n83[i];
    for (int i = 0; i < 3; i++)
        e[slot].ext[i]  = n83[8 + i];

    e[slot].attributes    = 0x20;          
    e[slot].start_cluster = (uint16_t)first;
    e[slot].file_size     = size;


    for (uint32_t i = 0; i < root_size; i++)
        ata_write(root_start + i, 1,(uint16_t *)(dir + i * 512));
    
    kfree(dir);


    printf("Wrote %s (%d bytes)\n", name, size);
    return 1;
}


void fat12_list(void) {
    uint8_t *buffer = (uint8_t *)kmalloc(root_size * 512);
    for (uint32_t i = 0; i < root_size; i++)
        ata_read(root_start + i, 1,
                 (uint16_t *)(buffer + i * 512));

    fat12_entry_t *entries   = (fat12_entry_t *)buffer;
    uint32_t       n_entries = root_size * 512 / sizeof(fat12_entry_t);

    for (uint32_t i = 0; i < n_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        if ((uint8_t)entries[i].name[0] == 0xE5) continue;
        if (entries[i].attributes == 0x0F) continue;

        /* Build a printable "BASENAME.EXT" string. */
        char display[13];
        int  n = 0;

        for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++)
            display[n++] = entries[i].name[j];

        if (entries[i].ext[0] != ' ') {
            display[n++] = '.';
            for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++)
                display[n++] = entries[i].ext[j];
        }

        display[n] = '\0';
        printf("    %s  %d bytes\n", display, entries[i].file_size);
    }

    kfree(buffer);
}

int fat12_delete(const char *name) {
    char n83[11];
    to_83(name, n83);

    uint8_t *dir = (uint8_t *)kmalloc(root_size * 512);
    for (uint32_t i = 0; i < root_size; i++)
        ata_read(root_start + i, 1, (uint16_t *)(dir + i * 512));

    fat12_entry_t *e     = (fat12_entry_t *)dir;
    uint32_t       n_ent = root_size * 512 / sizeof(fat12_entry_t);

    for (uint32_t i = 0; i < n_ent; i++) {
        if (e[i].name[0] == 0x00) break;
        if ((uint8_t)e[i].name[0] == 0xE5) continue;
        if (e[i].attributes == 0x0F) continue;

        int match = 1;
        for (int j = 0; j < 8; j++)
            if (e[i].name[j] != n83[j]) { match = 0; break; }
        if (match)
            for (int j = 0; j < 3; j++)
                if (e[i].ext[j] != n83[8 + j]) { match = 0; break; }

        if (match) {
            free_chain(e[i].start_cluster);  // free clusters in FAT
            e[i].name[0] = 0xE5;             // mark entry as deleted

            // flush only the sector containing this entry
            uint32_t sec = i / (512 / sizeof(fat12_entry_t));
            ata_write(root_start + sec, 1,
                      (uint16_t *)(dir + sec * 512));

            kfree(dir);
            printf("FAT12: deleted %s\n", name);
            return 1;
        }
    }

    kfree(dir);
    printf("FAT12: file not found\n");
    return 0;
}