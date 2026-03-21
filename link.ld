OUTPUT_FORMAT(elf32-i386)

ENTRY(start)
SECTIONS
{
	. 	= 1M;

	kernel_start = .;

	.text	: 
	{
		*(.text)
	}
	.data 	: 
	{
		*(.data)
	} 
	.bss 	: 
	{
		*(.bss)
	}

	kernel_end = .;



}
