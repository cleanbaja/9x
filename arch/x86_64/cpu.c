#include <arch/cpuid.h>
#include <arch/ic.h>
#include <arch/irq.h>
#include <arch/cpu.h>
#include <arch/tables.h>
#include <lib/kcon.h>
#include <ninex/proc.h>
#include <ninex/smp.h>
#include <vm/phys.h>
#include <vm/vm.h>

uint64_t cpu_features = 0;
uint64_t fpu_save_size = 0;
uint64_t fpu_save_align = 0;
extern void asm_syscall_entry();

static void detect_cpu_features() {
  uint32_t eax, ebx, ecx, edx;

  cpuid_subleaf(1, 0x0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_DEADLINE) {
    cpu_features |= CPU_FEAT_DEADLINE;
  }
  if (ecx & CPUID_ECX_PCID) {
    cpu_features |= CPU_FEAT_PCID;
  }
  if (ecx & CPUID_ECX_XSAVE) {
    cpu_features |= CPU_FEAT_XSAVE;
  }
  
  cpuid_subleaf(0x7, 0x0, &eax, &ebx, &ecx, &edx);
  if (ebx & CPUID_EBX_FSGSBASE) {
    cpu_features |= CPU_FEAT_FSGSBASE;
  }
  if (ebx & CPUID_EBX_SMAP) {
    cpu_features |= CPU_FEAT_SMAP;
  }
  if (ebx & CPUID_EBX_INVPCID) {
    cpu_features |= CPU_FEAT_INVPCID;
  }
  if (ecx & (1 << 16)) {
    kernel_vma = 0xFF00000000000000;
    extern enum { VM_5LV_PAGING, VM_4LV_PAGING } paging_mode;
    paging_mode  = VM_5LV_PAGING;

    klog("cpu: la57 detected!");
  }
 
  cpuid_subleaf(0x80000007, 0x0, &eax, &ebx, &ecx, &edx);
  if (edx & CPUID_EDX_INVARIANT) {
    cpu_features |= CPU_FEAT_INVARIANT;
  }

  cpuid_subleaf(0x80000001, 0x0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_TCE) {
    cpu_features |= CPU_FEAT_TCE;
  }
 
  // Set the last bit so that we don't run this function more than once
  cpu_features |= (1ull << 63ull);
}

void fpu_save(uint8_t* zone) {
  // Check if the pointer is aligned!
  if (((uintptr_t)zone % fpu_save_align) != 0) {
    klog("fpu: save area isn't aligned!!!");
  }

  // Then save the context...
  if (CPU_CHECK(CPU_FEAT_XSAVE)) {
    asm volatile(
      "xsaveq %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
      : "memory");
  } else {
    asm volatile("fxsaveq %[zone]" ::[zone] "m"(*zone) : "memory");
  }
}

void fpu_restore(uint8_t* zone) {
  // Check if the pointer is aligned!
  if (((uintptr_t)zone % fpu_save_align) != 0) {
    klog("fpu: save area isn't aligned!!!");
  }

  // Then restore the context...
  if (CPU_CHECK(CPU_FEAT_XSAVE)) {
    asm volatile(
      "xrstorq %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF), "d"(0xFFFFFFFF)
      : "memory");
  } else {
    asm volatile("fxrstorq %[zone]" ::[zone] "m"(*zone) : "memory");
  }
}

static void fpu_init() {
  uint32_t a, b, c, d;
  cpuid_subleaf(1, 0, &a, &b, &c, &d);
  if (CPU_CHECK(CPU_FEAT_XSAVE)) {
    // Enable XSAVE and XRSTOR, along with xgetbv/xsetbv
    asm_write_cr4(asm_read_cr4() | (1 << 18));  
    klog("fpu: using extended FPU save/restore");

    uint64_t xcr0 = 0;
    xcr0 |= (1 << 0) | // Save x87 state with x{save,rstor}, required
            (1 << 1);  // Save SSE state with x{save,rstor}

    if (c & CPUID_ECX_AVX) {
      xcr0 |= (1 << 2);
      klog("fpu: saving AVX state with XSAVE");
    }

    cpuid_subleaf(0x7, 0, &a, &b, &c, &d);
    if (b & CPUID_EBX_AVX512) {
      xcr0 |= (1 << 5) | // Enable AVX-512 foundation
              (1 << 6) | // Enable the lower 15 ZMM registers
              (1 << 7);  // Enable the higher 15 ZMM registers
      klog("fpu: saving AVX-512 state with XSAVE");
    }
    
    if (c & CPUID_ECX_PKE) {
      xcr0 |= (1 << 9); // Enable management of the PKRU register
      klog("fpu: saving PKU (Protection Keys for Userspace) state with XSAVE");
    }

    asm_wrxcr(0, xcr0);
    cpuid_subleaf(0xD, 0, &a, &b, &c, &d);
    fpu_save_size = (uint64_t)c;
    fpu_save_align = 64;
  } else {
    fpu_save_size = 512;
    fpu_save_align = 16;
    klog("fpu: using legacy FXSAVE/FXRSTOR");
  }
}

void cpu_early_init() {
  // Find features if we haven't done that already
  if (cpu_features == 0) detect_cpu_features();
  if (fpu_save_size == 0) fpu_init();

  // Assert that certain CPU features are present...
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(CPUID_EXTEND_FUNCTION_1, 0x0, &eax, &ebx, &ecx, &edx);

  if (!(edx & CPUID_EDX_XD_BIT_AVIL))
    PANIC(NULL, "NX not supported!\n");

  cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx);
  if (!(edx & CPUID_EDX_PGE))
    PANIC(NULL, "Global Pages not supported!\n");

  cpuid_subleaf(0x7, 0, &eax, &ebx, &ecx, &edx);
  if (!(ebx & CPUID_EBX_SMEP))
    PANIC(NULL, "SMEP not supported!\n");
  
  // Then enable all possible features
  uint64_t cr4 = asm_read_cr4();
  cr4 |= (1 << 2)  | // Stop userspace from reading the TSC
	       (1 << 7)  | // Enables Global Pages
         (1 << 9)  | // Allows for fxsave/fxrstor, along with SSE
	       (1 << 10) | // Allows for unmasked SSE exceptions
	       (1 << 20);  // Enables Supervisor Mode Execution Prevention
  asm_write_cr4(cr4);

  uint64_t efer = asm_rdmsr(IA32_EFER);
  efer |= (1 << 11) | // Enable No-Execute
          (1 << 0);   // Enable the syscall/sysret mechanism
  asm_wrmsr(IA32_EFER, efer);

  /* Enable optional features, if supported! */
  cr4 = asm_read_cr4();
  
  // Enable Supervisor Mode Access Prevention
  if (CPU_CHECK(CPU_FEAT_SMAP))
    cr4 |= (1 << 21);
  
  // Enable PCID
  if (CPU_CHECK(CPU_FEAT_PCID))
    cr4 |= (1 << 17);
  
  // Enable FSGSBASE instructions
  if (CPU_CHECK(CPU_FEAT_FSGSBASE))
    cr4 |= (1 << 16);

  // Enable X{SAVE,RSTOR} instructions
  if (CPU_CHECK(CPU_FEAT_XSAVE))
    cr4 |= (1 << 18);

  asm_write_cr4(cr4);

  // Enable Translation Cache Extension, a AMD only feature.
  efer = asm_rdmsr(IA32_EFER);
  if (CPU_CHECK(CPU_FEAT_TCE))
    efer |= (1 << 15);
  asm_wrmsr(IA32_EFER, efer);

  // Setup syscall instruction
  asm_wrmsr(IA32_STAR, 0x13ull << 48 | ((uint64_t)GDT_KERNEL_CODE) << 32);
  asm_wrmsr(IA32_LSTAR, (uintptr_t)asm_syscall_entry);
  asm_wrmsr(IA32_SFMASK, ~(uint32_t)2);
}


void cpu_create_context(void* thr, uintptr_t stack, uintptr_t entry, int user) {
  if (user)
    klog("cpu: (TODO) support the creation of usermode contexts");
  
  thread_t* thread   = (thread_t*)thr;
  cpu_ctx_t* context = &thread->context;

  context->rip    = entry;
  context->rsp    = stack;
  context->cs     = GDT_KERNEL_CODE;
  context->ss     = GDT_KERNEL_DATA;
  context->rflags = 0x202;
  context->rip    = entry;

  if (fpu_save_size > 4096) {
    // TODO: Fix memory leak introduced here!
    thread->fpu_save_area = vm_phys_alloc(2, VM_ALLOC_ZERO) + VM_MEM_OFFSET;
  } else {
    thread->fpu_save_area = vm_phys_alloc(1, VM_ALLOC_ZERO) + VM_MEM_OFFSET;
  }
}

#define ARCHCTL_WRITE_FS  0xB0
#define ARCHCTL_READ_MSR  0xB1
#define ARCHCTL_WRITE_MSR 0xb2

void syscall_archctl(cpu_ctx_t* context) {
  switch (context->rdi) {
  case ARCHCTL_WRITE_FS:
    asm_wrmsr(IA32_FS_BASE, context->rsi);
    per_cpu(cur_thread)->percpu_base = context->rsi;
    break;
  case ARCHCTL_READ_MSR:
    context->rax = asm_rdmsr(context->rsi);
    break;
  case ARCHCTL_WRITE_MSR:
    asm_wrmsr(context->rsi, context->rdx);
    break;
  } 
}

