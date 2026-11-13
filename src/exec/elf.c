/*
 * elf.c - loads 32-bit ELF programs from a file buffer into the
 * program area and returns their entry point.
 *
 * Every number in the file comes from the disk and may be wrong or
 * hostile, so each one is checked before it is used. The size checks are
 * written as "a <= limit - b" instead of "a + b <= limit": the sum could
 * wrap around 4 GB and pass, the subtraction can't (once b <= limit).
 */
#include "exec/elf.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "lib/string.h"

static bool program_area_ready;

/* Keep the PMM from handing out the program area. */
void elf_init(void) {
    uint32_t total_pages, used_pages;

    pmm_stats(&total_pages, &used_pages);
    if ((uint64_t)total_pages * PAGE_SIZE < PROGRAM_END) {
        serial_printf("elf: not enough RAM for the program area, exec disabled\n");
        return;
    }
    pmm_reserve_region(PROGRAM_BASE, PROGRAM_END - PROGRAM_BASE);
    program_area_ready = true;
    serial_printf("elf: program area %x - %x\n", PROGRAM_BASE, PROGRAM_END);
}

/* True if [start, start + length) lies inside [0, limit). */
static bool range_fits(uint32_t start, uint32_t length, uint32_t limit) {
    return start <= limit && length <= limit - start;
}

static bool elf_check_ident(const elf32_header_t* h) {
    uint32_t magic;

    memcpy(&magic, h->ident, sizeof(magic));
    if (magic != ELF_MAGIC) {
        serial_printf("elf: bad magic, not an ELF file\n");
        return false;
    }
    if (h->ident[ELF_IDENT_CLASS] != ELF_CLASS_32) {
        serial_printf("elf: not a 32-bit ELF\n");
        return false;
    }
    if (h->ident[ELF_IDENT_DATA] != ELF_DATA_LSB) {
        serial_printf("elf: not little-endian\n");
        return false;
    }
    return true;
}

/* Check the header and that the program header table is inside the file. */
static bool elf_check(const uint8_t* data, uint32_t size) {
    const elf32_header_t* h = (const elf32_header_t*)data;

    if (size < sizeof(elf32_header_t)) {
        serial_printf("elf: file too small (%u bytes)\n", size);
        return false;
    }
    if (!elf_check_ident(h)) {
        return false;
    }
    if (h->type != ELF_TYPE_EXEC || h->machine != ELF_MACHINE_386) {
        serial_printf("elf: not an i386 executable (type %u, machine %u)\n", h->type, h->machine);
        return false;
    }
    if (h->phentsize != sizeof(elf32_phdr_t)) {
        serial_printf("elf: unexpected program header size %u\n", h->phentsize);
        return false;
    }
    /* phnum and phentsize are 16-bit, so their product can't overflow. */
    if (!range_fits(h->phoff, (uint32_t)h->phnum * h->phentsize, size)) {
        serial_printf("elf: program headers lie outside the file\n");
        return false;
    }
    return true;
}

/* Check one PT_LOAD segment, then copy it and zero its .bss part. */
static bool elf_load_segment(const uint8_t* data, uint32_t size, const elf32_phdr_t* ph) {
    if (ph->vaddr < PROGRAM_BASE ||
        !range_fits(ph->vaddr - PROGRAM_BASE, ph->memsz, PROGRAM_END - PROGRAM_BASE)) {
        serial_printf("elf: segment %x (+%u) is outside the program area\n", ph->vaddr, ph->memsz);
        return false;
    }
    if (ph->filesz > ph->memsz) {
        serial_printf("elf: segment has more file bytes than memory bytes\n");
        return false;
    }
    if (!range_fits(ph->offset, ph->filesz, size)) {
        serial_printf("elf: segment data lies outside the file\n");
        return false;
    }

    uint8_t* dest = (uint8_t*)ph->vaddr;
    memcpy(dest, data + ph->offset, ph->filesz);
    memset(dest + ph->filesz, 0, ph->memsz - ph->filesz);
    return true;
}

uint32_t elf_load(const uint8_t* data, uint32_t size) {
    if (!program_area_ready || !elf_check(data, size)) {
        return 0;
    }
    const elf32_header_t* h = (const elf32_header_t*)data;
    const elf32_phdr_t* phdrs = (const elf32_phdr_t*)(data + h->phoff);

    for (uint32_t i = 0; i < h->phnum; i++) {
        if (phdrs[i].type == PT_LOAD && !elf_load_segment(data, size, &phdrs[i])) {
            return 0;
        }
    }
    if (h->entry < PROGRAM_BASE || h->entry >= PROGRAM_END) {
        serial_printf("elf: entry point %x is outside the program area\n", h->entry);
        return 0;
    }
    return h->entry;
}

bool elf_in_program_area(uint32_t address) {
    return address >= PROGRAM_BASE && address < PROGRAM_END;
}
