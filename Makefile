UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	CC = i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c
	LD = i686-elf-ld
	GRUB = i686-elf-grub-mkrescue
else ifeq($(UNAME),LINUX)
	CC = gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c
	LD = ld
	GRUB = grub-mkrescue
endif

all:
	nasm -f elf32 boot/boot.s -o boot.o
	nasm -f elf32 src/gdt/gdt.s -o gdts.o
	nasm -f elf32 src/idt/idt.s -o idts.o
	nasm -f elf32 src/test.s -o test.o

	

	$(CC) src/kernel.c -o kernel.o
	$(CC) src/vga.c -o vga.o
	$(CC) src/gdt/gdt.c -o gdt.o
	$(CC) src/idt/idt.c -o idt.o
	$(CC) src/util.c -o util.o
	$(CC) src/strings.c -o strings.o
	$(CC) src/timer.c -o timer.o
	$(CC) src/keyboard.c -o keyboard.o
	$(CC) src/shell.c -o shell.o
	$(CC) src/pmm.c -o pmm.o
	$(CC) src/paging.c -o paging.o
	$(CC) src/kmalloc.c -o kmalloc.o
	$(CC) src/syscall.c -o syscall.o
	
	$(LD) -T link.ld -m elf_i386 -o kernel *.o
	
	mv kernel planck/boot/kernel
	$(GRUB) -o planckOS.iso planck
	
	rm *.o
	
	qemu-system-i386 planckOS.iso
	
