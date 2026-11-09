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

QEMU_FLAGS := -boot d -cdrom $(ISO) -m 128M -serial stdio \
              -drive file=$(DISK),format=raw,index=1,media=disk

# ---- targets -----------------------------------------------------------
.PHONY: all iso run run-headless disk clean

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

clean:
	rm -rf $(BUILD)

-include $(DEPS)
