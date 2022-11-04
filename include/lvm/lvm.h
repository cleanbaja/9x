#pragma once

#include <arch/pmap.h>
#include <stddef.h>

#define LVM_HEAP_START 0xffffea0000000000
#define LVM_HEAP_SIZE  0x0000000100000000ULL // 4GB
#define LVM_HEAP_END   (LVM_HEAP_START + LVM_HEAP_SIZE)

#ifdef KASAN
void kasan_poison_shadow(uintptr_t addr, size_t len, uint8_t code);
void kasan_unpoison_shadow(uintptr_t addr, size_t len);
void lvm_setup_kasan();
#endif

void* kmalloc(size_t len);
void  kfree(void *ptr);
void* krealloc(void *ptr, size_t size);

void lvm_init();


