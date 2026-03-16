all:
	nasm -f elf32 boot/boot.s -o boot.o
	nasm -f elf32 src/gdt.s -o gdts.o
	nasm -f elf32 src/idt.s -o idts.o

	i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c src/kernel.c -o kernel.o
	i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c src/vga.c -o vga.o
	i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c src/gdt.c -o gdt.o
	i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c src/idt.c -o idt.o
	i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c src/util.c -o util.o
	i686-elf-gcc -m32 -ffreestanding -fno-stack-protector -fno-builtin -c src/timer.c -o timer.o
	
	i686-elf-ld -T link.ld -m elf_i386 -o kernel boot.o kernel.o vga.o gdt.o gdts.o idt.o idts.o util.o timer.o
	
	mv kernel planck/boot/kernel
	i686-elf-grub-mkrescue -o planckOS.iso planck
	
	rm *.o
	
	qemu-system-i386 planckOS.iso
	
