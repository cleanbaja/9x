#ifndef ARCH_SMP_H
#define ARCH_SMP_H

#include <arch/cpu.h>
#include <ninex/init.h>
#include <vm/virt.h>

#ifndef ARCH_INTERNAL
#define ARCH_INTERNAL
#endif  // ARCH_INTERNAL
#include <arch/asm.h>

struct percpu_info {
  uint32_t lapic_id;
  uint64_t tsc_freq, lapic_freq;
  uintptr_t kernel_stack;
  vm_space_t* cur_spc;
} __attribute__((packed));

EXPORT_STAGE(smp_stage);

// Macro for accessing the percpu_info struct
#define this_cpu ((struct percpu_info*)asm_rdmsr(IA32_KERNEL_GS_BASE))
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
