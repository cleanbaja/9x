#include <9x/vm.h>
#include <lib/builtin.h>

#define NUM_PAGES(sz) (((sz) + 4096 - 1) / 4096)

struct metadata {
    size_t numpages;
    size_t size;
};

void* kmalloc(size_t size) {
    struct metadata* alloc = (struct metadata*)(vm_phys_alloc(NUM_PAGES(size) + 1) + VM_MEM_OFFSET);
    alloc->numpages = NUM_PAGES(size);
    alloc->size = size;
    return ((uint8_t*)alloc) + 4096;
}

void kfree(void* addr) {
    struct metadata* d = (struct metadata*)((uint8_t*)addr - 4096);
    vm_phys_free((void*)((uintptr_t)d - VM_MEM_OFFSET), d->numpages + 1);
}

void* krealloc(void* addr, size_t newsize) {
    if (!addr)
        return kmalloc(newsize);

    struct metadata* d = (struct metadata*)((uint8_t*)addr - 4096);
    if (NUM_PAGES(d->size) == NUM_PAGES(newsize)) {
        d->size = newsize;
        d->numpages = NUM_PAGES(newsize);
        return addr;
    }

    void* new = kmalloc(newsize);
    if (d->size > newsize)
        memcpy(addr, new, newsize);
    else
        memcpy(addr, new, d->size);

    kfree(addr);
    return new;
}


