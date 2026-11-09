# Memory management

The memory code lives in `src/mm/` and has three layers:

| Layer | File | Hands out |
|-------|------|-----------|
| Physical memory manager (PMM) | `mm/pmm.c` | 4 KB physical pages |
| Paging | `mm/paging.c` | virtual → physical mappings |
| Kernel heap | `mm/kmalloc.c` | byte-sized blocks (`kmalloc` / `kfree`) |

## Physical memory manager — `mm/pmm.c`

**What it does.** Tracks which 4 KB pages of physical RAM are free.

**How it works.** It keeps a bitmap with one bit per page (1 = used,
0 = free), sized for the full 4 GB address space (1M pages = 128 KB of
bitmap). `pmm_init()`:

1. Marks **every** page as used. This is the safe default: anything GRUB
   doesn't explicitly call free stays off-limits.
2. Walks the Multiboot memory map (`mmap_addr` / `mmap_length`, valid when
   flag bit 6 is set) and clears the bits for every page in a type-1
   ("available") region. Regions above 4 GB are skipped, since we don't use
   PAE.
3. Reserves the first 1 MB (BIOS data, VGA memory, ROMs) even if GRUB calls
   parts of it free.
4. Reserves the kernel image, `kernel_start` to `kernel_end` from `link.ld`.

Each mmap entry starts with a `size` field that does **not** count itself.
That's why the loop advances by `entry->size + sizeof(uint32_t)`.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `pmm_init(mbi)` | Build the bitmap from GRUB's memory map. |
| `pmm_alloc_page()` | Find the first free page, mark it used, return its physical address (0 on failure). |
| `pmm_free_page(addr)` | Mark a page free again. |
| `pmm_reserve_region(start, size)` | Mark every page overlapping a byte range as used. Use it for memory that something else owns. |

**Extending it.** A faster allocator could remember where the last search
stopped, or keep a free-page stack. Contiguous multi-page allocations would
need a "find N free bits in a row" search.

**Known limits.** First-fit linear scan. No reference counts. RAM size comes
from `mem_upper`, so memory after the first hole above 1 MB is ignored.

## Paging — `mm/paging.c`

**What it does.** Turns on the MMU so every memory access goes through page
tables. It also lets the kernel map extra physical memory, such as a
framebuffer, at chosen virtual addresses.

**How it works.** 32-bit x86 paging without PAE has two levels:

```
virtual address:  | dir index (10) | table index (10) | offset (12) |
CR3 -> page directory (1024 entries) -> page table (1024 entries) -> 4 KB page
```

Each directory entry covers 4 MB and each table entry covers one 4 KB page.
The low 12 bits of an entry are flags (`PAGE_PRESENT`, `PAGE_WRITABLE`,
`PAGE_USER`, ...) and the high 20 bits are the physical address.

`paging_init()` identity-maps the first `PAGING_IDENTITY_MB` (64 MB) with 16
statically allocated page tables in `.bss`, loads the directory into CR3 and
sets bit 31 of CR0. Because those tables are part of the kernel image, the
PMM already treats them as used.

`paging_map(virt, phys, flags)` writes one page-table entry. If the 4 MB
region has no table yet, it takes a page from the PMM, zeroes it and links
it into the directory. It then runs `invlpg` so the CPU drops any stale
translation from its TLB.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `paging_init()` | Identity-map 64 MB and enable paging. |
| `paging_map(virt, phys, flags)` | Map one 4 KB page. Returns `false` if no page table could be allocated. |

**Extending it.** Add `paging_unmap()`, which clears the entry and runs
`invlpg`. For user mode, give each process its own page directory and set
`PAGE_USER` on its pages.

**Known limits.** One address space for everything. New page tables must
come from identity-mapped memory (the PMM hands out low pages first, so this
holds unless RAM below 64 MB runs out). No unmapping yet.
