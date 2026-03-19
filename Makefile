CC = i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c

all:
	nasm -f elf32 boot/boot.s -o boot.o
	nasm -f elf32 src/gdt/gdt.s -o gdts.o
	nasm -f elf32 src/idt/idt.s -o idts.o
	

	$(CC) src/kernel.c -o kernel.o
	$(CC) src/vga.c -o vga.o
	$(CC) src/gdt/gdt.c -o gdt.o
	$(CC) src/idt/idt.c -o idt.o
	$(CC) src/util.c -o util.o
	$(CC) src/strings.c -o strings.o
	$(CC) src/timer.c -o timer.o
	$(CC) src/keyboard.c -o keyboard.o
	$(CC) src/shell.c -o shell.o
	
	i686-elf-ld -T link.ld -m elf_i386 -o kernel *.o
	
	mv kernel planck/boot/kernel
	i686-elf-grub-mkrescue -o planckOS.iso planck
	
	rm *.o
	
	qemu-system-i386 planckOS.iso
	
