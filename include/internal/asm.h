#ifndef INTERNAL_ASM_H
#define INTERNAL_ASM_H

#include <stdint.h>

// General asm routines
#define asm_halt() ({           \
    __asm__ volatile ("cli");   \
    for (;;) {                  \
      __asm__ volatile ("hlt"); \
    }                           \
})

// GDT/IDT asm routines
extern void* asm_dispatch_table[256];
extern void asm_load_gdt(struct table_ptr* g);
#define asm_load_idt(ptr) __asm__ volatile ("lidt %0" :: "m" (ptr))

#endif // INTERNAL_ASM_H

