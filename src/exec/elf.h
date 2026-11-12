/*
 * elf.h - loading 32-bit ELF executables into the program area.
 *
 * A program is linked to run at a fixed address inside the program area
 * (PROGRAM_BASE..PROGRAM_END, 32-48 MB). elf_load() checks the file, copies
 * its loadable segments there and returns the address to jump to.
 */
#ifndef elf_h__
#define elf_h__

#include "lib/common.h"

#define PROGRAM_BASE    0x2000000   /* 32 MB */
#define PROGRAM_END     0x3000000   /* 48 MB */

#define ELF_MAGIC       0x464C457F  /* 0x7F 'E' 'L' 'F' as a little-endian uint32 */
#define ELF_IDENT_SIZE  16
#define ELF_IDENT_CLASS 4           /* index of the class byte in ident[] */
#define ELF_IDENT_DATA  5           /* index of the byte order byte in ident[] */
#define ELF_CLASS_32    1           /* ident[4]: 32-bit */
#define ELF_DATA_LSB    1           /* ident[5]: little-endian */
#define ELF_TYPE_EXEC   2           /* an executable, not a library or object */
#define ELF_MACHINE_386 3           /* Intel 80386 */
#define PT_LOAD         1           /* program header type: load this segment */

typedef struct {
    uint8_t  ident[ELF_IDENT_SIZE];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;         /* address to jump to */
    uint32_t phoff;         /* file offset of the program headers */
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;     /* size of one program header */
    uint16_t phnum;         /* number of program headers */
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf32_header_t;

typedef struct {
    uint32_t type;          /* PT_LOAD = load this one */
    uint32_t offset;        /* where the bytes are in the file */
    uint32_t vaddr;         /* where they go in memory */
    uint32_t paddr;
    uint32_t filesz;        /* bytes to copy */
    uint32_t memsz;         /* total size; the rest gets zeroed (.bss) */
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) elf32_phdr_t;

/* Reserve the program area in the PMM. Call once, after heap_init. */
void elf_init(void);

/* Check the ELF file in data[0..size) and load it into the program area.
   Returns the entry point, or 0 if the file is invalid (the reason goes
   to the serial log). */
uint32_t elf_load(const uint8_t* data, uint32_t size);

#endif
