/* kern_entry - src/kern/init.c */
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

    . = ALIGN(64K);
    . += 64K;

    /* Keep this section, since the spec requires it! */
    .stivale2hdr : {
        KEEP(*(.stivale2hdr))
    } :rodata

    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(64K);
    . += 64K;

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
        . += 4K * 16;
        __kern_stack_top = .;
    } :data

    /* DWARF section definitions from GNU binutils */
    /* Copyright (C) 2014-2022 Free Software Foundation, Inc.
       Copying and distribution of this script, with or without modification,
       are permitted in any medium without royalty provided the copyright
       notice and this notice are preserved.  */

    /* DWARF 1.  */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }

    /* GNU DWARF 1 extensions.  */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }

    /* DWARF 1.1 and DWARF 2.  */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }

    /* DWARF 2.  */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }

    /* SGI/MIPS DWARF 2 extensions.  */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }

    /* DWARF 3.  */
    .debug_pubtypes 0 : { *(.debug_pubtypes) }
    .debug_ranges   0 : { *(.debug_ranges) }

    /* DWARF 5.  */
    .debug_addr     0 : { *(.debug_addr) }
    .debug_line_str 0 : { *(.debug_line_str) }
    .debug_loclists 0 : { *(.debug_loclists) }
    .debug_macro    0 : { *(.debug_macro) }
    .debug_names    0 : { *(.debug_names) }
    .debug_rnglists 0 : { *(.debug_rnglists) }
    .debug_str_offsets 0 : { *(.debug_str_offsets) }
    .debug_sup      0 : { *(.debug_sup) }
    .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }

    /* Discard unused sections (fixes issues with certain linkers) */
    /DISCARD/ : {
        *.o(*)
        *.a:*(*)
    }
}

