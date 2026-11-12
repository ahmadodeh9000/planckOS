# planckOS build
#
# Compiles every .c and .s file under src/ (plus boot/boot.s) into build/,
# links the kernel with link.ld and wraps it in a GRUB rescue ISO.
#
#   make              build build/planckOS.iso (does not start QEMU)
#   make iso          same as make
#   make run          boot the ISO in QEMU with disk.img attached
#   make run-headless boot without a window; serial output goes to stdout
#   make disk         create a blank FAT12 disk.img if it does not exist
#   make programs     build programs/*.c and copy them, plus the programs/*.asm
#                     examples, onto disk.img (mtools)
#   make asm-test     check the built-in assembler's bytes against NASM
#   make clean        remove build/

# ---- toolchain ---------------------------------------------------------
# Prefer an i686-elf cross compiler (Homebrew and most Linux distros ship
# one). Fall back to the host gcc in 32-bit freestanding mode.
ifneq ($(shell command -v i686-elf-gcc 2>/dev/null),)
    CC   := i686-elf-gcc
    GRUB := i686-elf-grub-mkrescue
else
    CC   := gcc
    GRUB := grub-mkrescue
    HOST_CFLAGS  := -m32 -fno-pic -fno-pie
    HOST_LDFLAGS := -m32 -no-pie -Wl,-m,elf_i386
endif

AS   := nasm
QEMU := qemu-system-i386

CFLAGS  := -std=gnu99 -ffreestanding -fno-stack-protector -fno-builtin \
           -nostdlib -Wall -Wextra -O2 -g -Isrc $(HOST_CFLAGS)
ASFLAGS := -f elf32 -g
LDFLAGS := -T link.ld -nostdlib -ffreestanding $(HOST_LDFLAGS)

# ---- files -------------------------------------------------------------
BUILD  := build
KERNEL := $(BUILD)/kernel.elf
ISO    := $(BUILD)/planckOS.iso
DISK   ?= disk.img

DISK_SIZE_MB             := 64
DISK_SECTORS_PER_CLUSTER := 64

C_SRCS   := $(shell find src -name '*.c')
ASM_SRCS := boot/boot.s $(shell find src -name '*.s')
OBJS     := $(patsubst %,$(BUILD)/%.o,$(ASM_SRCS) $(C_SRCS))
DEPS     := $(OBJS:.o=.d)

# Programs for 'exec': each programs/<name>.c becomes build/programs/<name>.elf,
# copied to the disk as <NAME>.ELF (so <name> must be at most 8 characters).
# The assembly examples programs/*.asm are copied as they are, as <NAME>.ASM,
# for the built-in 'asm' command.
PROG_SRCS    := $(wildcard programs/*.c)
PROG_ASM     := $(wildcard programs/*.asm)
PROGRAMS     := $(patsubst programs/%.c,$(BUILD)/programs/%.elf,$(PROG_SRCS))
PROG_CFLAGS  := -std=gnu99 -ffreestanding -fno-stack-protector -fno-builtin \
                -nostdlib -fno-pic -Wall -Wextra -O2 $(HOST_CFLAGS)
PROG_LDFLAGS := -T programs/program.ld -Wl,-z,max-page-size=0x1000 $(HOST_LDFLAGS)

# -rtc base=localtime: the guest's CMOS clock shows the host's local time
# instead of UTC.
QEMU_FLAGS := -boot d -cdrom $(ISO) -m 128M -serial stdio -rtc base=localtime \
              -drive file=$(DISK),format=raw,index=1,media=disk

# ---- targets -----------------------------------------------------------
.PHONY: all iso run run-headless disk programs asm-test clean

all: iso

iso: $(ISO)

$(ISO): $(KERNEL) boot/grub.cfg
	@mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/kernel
	cp boot/grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	$(GRUB) -o $@ $(BUILD)/iso 2>/dev/null

$(KERNEL): $(OBJS) link.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lgcc

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

run: $(ISO) $(DISK)
	$(QEMU) $(QEMU_FLAGS)

run-headless: $(ISO) $(DISK)
	$(QEMU) $(QEMU_FLAGS) -display none

# Only creates the disk when it is missing, so an existing disk.img (and the
# files on it) is never overwritten.
disk: $(DISK)

$(DISK):
	dd if=/dev/zero of=$@ bs=1048576 count=$(DISK_SIZE_MB) 2>/dev/null
ifeq ($(shell uname),Darwin)
	@# newfs_msdos only formats devices, so attach the file as one first.
	dev=$$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount $@ | awk 'NR==1{print $$1}'); \
	newfs_msdos -F 12 -c $(DISK_SECTORS_PER_CLUSTER) -v PLANCK $$dev; \
	hdiutil detach $$dev >/dev/null
else
	mkfs.fat -F 12 -s $(DISK_SECTORS_PER_CLUSTER) -n PLANCK $@
endif

# MTOOLS_SKIP_CHECK: mtools otherwise rejects disks whose size isn't a
# multiple of the classic floppy geometry, which ours needn't be.
programs: $(PROGRAMS) $(DISK)
	@for file in $(PROGRAMS) $(PROG_ASM); do \
	    name=$$(basename $$file | tr a-z A-Z); \
	    echo "mcopy $$file -> $(DISK):$$name"; \
	    MTOOLS_SKIP_CHECK=1 mcopy -o -i $(DISK) $$file ::$$name || exit 1; \
	done

# Builds the assembler in src/asm/ for this computer (HOST_CC), assembles
# tests/asm/encoding.asm with it and with 'nasm -O0', and compares the bytes.
# tests/asm/host/ supplies lib/string.h from the host's C library.
HOST_CC  ?= cc
ASM_TEST := $(BUILD)/asm-test

asm-test:
	@mkdir -p $(ASM_TEST)
	$(HOST_CC) -std=c99 -Wall -Wextra -Itests/asm/host -Isrc \
	    -o $(ASM_TEST)/asm_host tests/asm/asm_host.c $(wildcard src/asm/*.c)
	$(ASM_TEST)/asm_host tests/asm/encoding.asm $(ASM_TEST)/planck.bin
	nasm -O0 -f bin -I tests/asm/ -o $(ASM_TEST)/nasm.bin tests/asm/nasm.asm
	cmp $(ASM_TEST)/planck.bin $(ASM_TEST)/nasm.bin
	@echo "asm-test: identical to NASM ($$(wc -c < $(ASM_TEST)/nasm.bin | tr -d ' ') bytes)"

$(BUILD)/programs/%.elf: programs/%.c programs/planck.h programs/program.ld
	@mkdir -p $(dir $@)
	$(CC) $(PROG_CFLAGS) $(PROG_LDFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD)

-include $(DEPS)
