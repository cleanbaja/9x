#ifndef ARCH_ARCH_H
#define ARCH_ARCH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

void arch_early_init();
void arch_init();

// Memory Guard API (protect kernel from reading user memory)
void mg_enable();
void mg_disable();
bool mg_validate(uintptr_t ptr, size_t len);
bool mg_copy_to_user(void* usrptr, void* srcptr, size_t len);
bool mg_copy_from_user(void* kernptr, void* srcptr, size_t len);

#endif  // ARCH_ARCH_H
