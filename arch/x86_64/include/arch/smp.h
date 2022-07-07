#ifndef ARCH_SMP_H
#define ARCH_SMP_H

#include <arch/cpu.h>
#include <arch/tables.h>
#include <ninex/proc.h>
#include <vm/virt.h>
#include <arch/asm.h>

struct percpu_info {
  uint32_t lapic_id;
  uint16_t proc_id;
  uint64_t tsc_freq, lapic_freq;
  uintptr_t kernel_stack, user_stack;
  thread_t* cur_thread;
  vm_space_t* cur_spc;
  uint32_t errno;
  struct tss tss;
  bool yielded;
} __attribute__((packed));

void smp_startup();

// Macro for accessing the percpu_info struct
#define this_cpu ((struct percpu_info*)asm_rdmsr(IA32_GS_BASE))
#define cpu_num                                                    \
  ({                                                               \
    int ret;                                                       \
    /* FIXME: Use the actual 'RDPID' instruction, which GCC has */ \
    if (CPU_CHECK(CPU_FEAT_RDPID)) {                               \
      asm(".byte 0xf3,0x0f,0xc7,0xf8" : "=a"(ret));                \
    } else {                                                       \
      asm volatile("rdtscp" : "=c"(ret)::"rax", "rdx");            \
    }                                                              \
    ret;                                                           \
  })

#endif  // ARCH_SMP_H
