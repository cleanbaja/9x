#ifdef KASAN

#include <lvm/lvm.h>
#include <lvm/lvm_space.h>
#include <lvm/lvm_page.h>
#include <lib/tlsf.h>
#include <lib/libc.h>
#include <lib/panic.h>

#define CHECK_ADDR(addr)    (addr >= LVM_HEAP_START && addr < LVM_HEAP_END)
#define KASAN_BASE          0xdfffe00000000000
#define INLINE_FUNC         __attribute__((always_inline)) static inline
#define EXPECT_FALSE(expr)  __builtin_expect((expr),0)
#define EXPECT_TRUE(expr)   __builtin_expect((expr),1)

#define KASAN_FUNC(name, size, store)             \
  void __asan_##name##_noabort(uintptr_t addr) {  \
    kasan_report(addr, size, store);              \
  }

#define DEFINE_KASAN_FUNCS(size)             \
  KASAN_FUNC(load##size, size, false)        \
  KASAN_FUNC(report_load##size, size, false) \
  KASAN_FUNC(store##size, size, true)        \
  KASAN_FUNC(report_store##size, size, true)

INLINE_FUNC int8_t *get_shadow(uintptr_t ptr) {
  return (int8_t*)(KASAN_BASE + (ptr >> 3));
}

ASAN_NO_SANITIZE_ADDRESS
void kasan_poison_shadow(uintptr_t addr, size_t len, uint8_t code) {
  if (!CHECK_ADDR(addr))
    return; // Not in KASAN range

  void *shadow_start, *shadow_end;
  shadow_start = get_shadow(addr);
  shadow_end = get_shadow(addr + len);

  memset(shadow_start, code, shadow_end - shadow_start);
}

ASAN_NO_SANITIZE_ADDRESS
void kasan_unpoison_shadow(uintptr_t addr, size_t len) {
  if (!CHECK_ADDR(addr))
    return; // Not in KASAN range

  kasan_poison_shadow(addr, len, 0);

  if (len & 0b111) {
    uint8_t *shadow = (uint8_t *)get_shadow(addr + len);
    *shadow = len & 0b111;
  }
}

INLINE_FUNC bool kasan_check_1(uintptr_t ptr) {
  int8_t shadow = *get_shadow(ptr);

  if (EXPECT_FALSE(shadow)) {
    int8_t last_byte = ptr & 0b111;
    return EXPECT_FALSE(last_byte >= shadow);
  }

  return false;
}

INLINE_FUNC bool kasan_check_2_4_8(uintptr_t ptr, size_t len) {
  uint8_t* shadow = (uint8_t*)get_shadow(ptr);
  if (EXPECT_FALSE(((ptr + len - 1) & 0b111) < len - 1))
    return *shadow || kasan_check_1(ptr + len - 1);

  return kasan_check_1(ptr + len - 1);
}

INLINE_FUNC uint64_t scan_bytes(uint8_t* bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (EXPECT_FALSE(bytes[i]))
      return (uint64_t)&bytes[i];
  }

  return 0;
}

INLINE_FUNC uint64_t mem_is_zero(uintptr_t begin, uintptr_t end) {
  uint32_t words;
  uint64_t ret;
  uint32_t prefix = (uint64_t)begin % 8;

  if (end - begin <= 16)
    return scan_bytes((uint8_t*)begin, end - begin);

  if (prefix) {
    prefix = 8 - prefix;
    ret = scan_bytes((uint8_t*)begin, prefix);
    if (EXPECT_FALSE(ret))
      return ret;
    begin += prefix;
  }

  words = (end - begin) / 8;
  while (words) {
    if (EXPECT_FALSE(*(uint64_t *)begin))
      return scan_bytes((uint8_t*)begin, 8);
    begin += 8;
    words--;
  }

  return scan_bytes((uint8_t*)begin, (end - begin) % 8);
}

INLINE_FUNC bool kasan_check_N(uintptr_t ptr, size_t len) {
  uint64_t ret = mem_is_zero((uintptr_t)get_shadow(ptr), (uintptr_t)(get_shadow(ptr + len - 1) + 1));

  if (EXPECT_FALSE(ret)) {
    uint64_t last_byte = ptr + len - 1;
    int8_t *last_shadow = (int8_t*)get_shadow(last_byte);

    if (EXPECT_FALSE(ret != (uint64_t)last_shadow || ((int64_t)(last_byte & 0b111) >= *last_shadow))) {
      return true;
    }
  }

  return false;
}

INLINE_FUNC bool kasan_check(uintptr_t addr, size_t len) {
  if (__builtin_constant_p(len)) {
    switch (len) {
      case 1:
        return kasan_check_1(addr);
      case 2:
      case 4:
      case 8:
        return kasan_check_2_4_8(addr, len);
    }
  }

  return kasan_check_N(addr, len);
}

ASAN_NO_SANITIZE_ADDRESS
static void kasan_report(uintptr_t addr, size_t size, bool is_store) {
  if (EXPECT_TRUE(!CHECK_ADDR(addr)))
    return; // Not in KASAN range

  if (EXPECT_FALSE(size == 0))
    return;

  if (EXPECT_TRUE(!kasan_check(addr, size)))
    return;

failed:
  panic(NULL, "kasan: faulty %u-byte %s to 0x%lx\n", size, is_store ? "write" : "read", addr);
}

DEFINE_KASAN_FUNCS(1);
DEFINE_KASAN_FUNCS(2);
DEFINE_KASAN_FUNCS(4);
DEFINE_KASAN_FUNCS(8);
DEFINE_KASAN_FUNCS(16);

void __asan_loadN_noabort(uintptr_t addr, size_t size) {
    kasan_report(addr, size, false);
}
void __asan_storeN_noabort(uintptr_t addr, size_t size) {
    kasan_report(addr, size, true);
}
void __asan_report_load_n_noabort(uintptr_t addr, size_t size) {
    kasan_report(addr, size, true);
}
void __asan_report_store_n_noabort(uintptr_t addr, size_t size) {
    kasan_report(addr, size, true);
}

// Stub no return, 'cause linux does it
void __asan_handle_no_return() {}

// Don't support alloca stuff, since it's not used in the kernel
void __asan_alloca_poison(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
}
void __asan_allocas_unpoison(void *stack_top, void *stack_bottom) {
  (void)stack_top;
  (void)stack_bottom;
}

void lvm_setup_kasan() {
  int perms = LVM_PERM_READ | LVM_PERM_WRITE | LVM_TYPE_GLOBAL;
  uintptr_t begin = KASAN_BASE + LVM_HEAP_START / 8;
  uintptr_t end = KASAN_BASE + LVM_HEAP_END / 8;

  for (uintptr_t i = 0; i < end - begin; i += LVM_PAGE_SIZE) {
    uintptr_t page = LVM_ALLOC_PAGE(false, LVM_PAGE_SYSTEM);
    memset((void*)(page + LVM_HIGHER_HALF), 0xFE, LVM_PAGE_SIZE);
    lvm_map_page(&kspace, i + begin, page, LVM_PAGE_SIZE, perms);
  }
}

#endif
