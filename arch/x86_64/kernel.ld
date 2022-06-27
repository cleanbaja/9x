/* kern_entry - src/generic/kern.c */
ENTRY(kern_entry)

PHDRS {
    null    PT_NULL    FLAGS(0) ;                   /* Null segment */
    text    PT_LOAD    FLAGS(5) ;                   /* Execute + Read */
    rodata  PT_LOAD    FLAGS(4) ;                   /* Read only */
    data    PT_LOAD    FLAGS(6) ;                   /* Write + Read */
    dynamic PT_DYNAMIC FLAGS(6) ;                   /* Dynamic segment needed for PIE */
}

SECTIONS {
    . = 0xFFFFFFFF80200000;

    .text : {
        *(.text .text.*)
    } :text

    . += CONSTANT(MAXPAGESIZE);

    /* Keep this section, since the spec requires it! */
    .stivale2hdr : {
        KEEP(*(.stivale2hdr))
    } :rodata

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . += CONSTANT(MAXPAGESIZE);

    .data : {
        *(.data .data.*)
    } :data

	/* PIE related memes */
    .dynamic : {
        *(.dynamic)
    } :data :dynamic

    .rela : {
        *(.rela .rela.*)
    } :data

    .got : {
        *(.got .got.*)
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)

        /* Reserve a 16 KB stack for kernel startup */
        . = ALIGN(16);
        . += CONSTANT(MAXPAGESIZE) * 16;
        __kern_stack_top = .;
    } :data

    /* Discard unused sections (fixes issues with certain linkers) */
    /DISCARD/ : {
        *.o(*)
        *.a:*(*)
    }
}
