/* Target the x86_64 platform */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

/* kern_entry - src/generic/kern.c */
ENTRY(kern_entry)

SECTIONS
{
 . = 0xffffffff80200000;

 .stivale2hdr : ALIGN(4K) {
  KEEP(*(.stivale2hdr))
 } 

 .text : ALIGN(4K) {
  *(.text*)
 }

 .rodata : ALIGN(4K) {
  *(.rodata*)
 }

 .data : ALIGN(4K) {
  *(.data*)
 }

 .bss : ALIGN(4K) {
  *(.bss*)
  *(COMMON)

  . = ALIGN(4K);
  kern_top = .;
 }
}

