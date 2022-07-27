#include <arch/cpuid.h>
#include <arch/irqchip.h>
#include <arch/smp.h>
#include <arch/tables.h>
#include <arch/arch.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <ninex/proc.h>
#include <vm/phys.h>
#include <vm/vm.h>

// XSAVE features not supported by 9x...
//  - Intel(R) MPX: Intel themselves deprecated it (also removed in GCC).
//  - Intel(R) HDC: We don't support APERF/MPERF MSR based power controls.
//  - Intel(R) PT: It is on the wishlist, but not supported at the moment.
#define XSAVE_UNSUPPORTED_MASK ((3 << 3) | (1 << 13) | (1 << 8))

// Store the kernel stack at 0x70000000000
#define THREAD_STACK_BASE 0x70000000000

enum {
  FPU_INSTR_FX,  // FXSAVE/FXRSTOR
  FPU_INSTR_X,   // XSAVE/XRSTOR
  FPU_INSTR_XC,  // XSAVEC/XRSTOR
  FPU_INSTR_XS,  // XSAVES/XRSTORS
} fpu_mode = FPU_INSTR_FX;

uint64_t cpu_features = 0;
uint64_t fpu_save_size = 0;
extern void asm_syscall_entry();
extern void sched_spinup(cpu_ctx_t* context);

static char* mode_to_str(int mode) {
  switch (mode) {
    case FPU_INSTR_FX:
      return "FXSAVE";

    case FPU_INSTR_X:
      return "XSAVE";

    case FPU_INSTR_XC:
      return "XSAVEC";

    case FPU_INSTR_XS:
      return "XSAVES";
  }

  return NULL;  // ???
}

void fpu_save(uint8_t* zone) {
  // Check alignment...
  switch (fpu_mode) {
    case FPU_INSTR_FX:
      if (((uintptr_t)zone % 16) != 0)
        goto misalign;
      break;

    case FPU_INSTR_X:
    case FPU_INSTR_XC:
    case FPU_INSTR_XS:
      if (((uintptr_t)zone % 64) != 0)
        goto misalign;
      break;
  }

  // Then save the context
  switch (fpu_mode) {
    case FPU_INSTR_FX:
      asm volatile("fxsaveq %[zone]" ::[zone] "m"(*zone) : "memory");
      break;

    case FPU_INSTR_X:
      asm volatile("xsaveq %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF),
                   "d"(0xFFFFFFFF)
                   : "memory");
      break;

    case FPU_INSTR_XC:
      asm volatile("xsavec %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF),
                   "d"(0xFFFFFFFF)
                   : "memory");
      break;

    case FPU_INSTR_XS:
      asm volatile("xsaves %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF),
                   "d"(0xFFFFFFFF)
                   : "memory");
      break;
  }
  return;

misalign:
  klog("fpu: save area is not aligned!");
  return;
}

void fpu_restore(uint8_t* zone) {
  // Check alignment...
  switch (fpu_mode) {
    case FPU_INSTR_FX:
      if (((uintptr_t)zone % 16) != 0)
        goto misalign;
      break;

    case FPU_INSTR_X:
    case FPU_INSTR_XC:
    case FPU_INSTR_XS:
      if (((uintptr_t)zone % 64) != 0)
        goto misalign;
      break;
  }

  // Then save the context
  switch (fpu_mode) {
    case FPU_INSTR_FX:
      asm volatile("fxrstorq %[zone]" ::[zone] "m"(*zone) : "memory");
      break;

    case FPU_INSTR_X:
    case FPU_INSTR_XC:
      asm volatile("xrstorq %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF),
                   "d"(0xFFFFFFFF)
                   : "memory");
      break;

    case FPU_INSTR_XS:
      asm volatile("xrstors %[zone]" ::[zone] "m"(*zone), "a"(0xFFFFFFFF),
                   "d"(0xFFFFFFFF)
                   : "memory");
      break;
  }
  return;

misalign:
  klog("fpu: restore area is not aligned!");
  return;
}

static void fpu_init() {
  // Use the XSAVE family of instructions when possible...
  if (CPU_CHECK(CPU_FEAT_XSAVE)) {
    uint32_t a, b, c, d;
    asm_write_cr4(asm_read_cr4() | (1 << 18));
    fpu_mode = FPU_INSTR_X;

    // Check for XSAVEC/XSAVES
    cpuid_subleaf(0xD, 1, &a, &b, &c, &d);
    if (a & CPUID_EAX_XSAVEC) {
      fpu_mode = FPU_INSTR_XC;

      cpuid_subleaf(0xD, 0, &a, &b, &c, &d);
      fpu_save_size = b;
    }
    if (a & CPUID_EAX_XCR0_BNDREGS) {
      fpu_mode = FPU_INSTR_XS;

      // TODO: We currently don't take advantage
      // of supervisor saving/restoring, so clear
      // the IA32_XSS MSR
      asm_wrmsr(0x0DA0, 0);
      fpu_save_size = b;
    }

    // Setup xcr0 to save relevant features
    cpuid_subleaf(0xD, 0, &a, &b, &c, &d);
    uint64_t xcr0 = (a & ~XSAVE_UNSUPPORTED_MASK);
    asm_wrxcr(0, xcr0);

    // Dump supported features
    if (xcr0 & 3) {
      klog("fpu: saving x87 & SSE state with %s", mode_to_str(fpu_mode));
    }
    if (xcr0 & 4) {
      klog("fpu: saving AVX state with %s", mode_to_str(fpu_mode));
    }
    if (xcr0 & 224) {
      klog("fpu: saving AVX-512 state with %s", mode_to_str(fpu_mode));
    }

    // Find the save size for XSAVE, and print to the user
    if (fpu_save_size == 0)
      fpu_save_size = (uint64_t)c;

    klog("fpu: using extended FPU save/restore with context size of %d!",
         fpu_save_size);
  } else {
    fpu_save_size = 512;
    asm_write_cr4(asm_read_cr4() | (1 << 9));
    klog("fpu: using legacy FXSAVE/FXRSTOR");
  }
}

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
  if (ecx & CPUID_ECX_RDPID) {
    cpu_features |= CPU_FEAT_RDPID;
  }
  if (ebx & CPUID_EBX_SMAP) {
    cpu_features |= CPU_FEAT_SMAP;
  }
  if (ebx & CPUID_EBX_INVPCID) {
    cpu_features |= CPU_FEAT_INVPCID;
  }

  cpuid_subleaf(0x80000007, 0x0, &eax, &ebx, &ecx, &edx);
  if (edx & CPUID_EDX_INVARIANT) {
    cpu_features |= CPU_FEAT_INVARIANT;
  }

  cpuid_subleaf(0x80000001, 0x0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_TCE) {
    cpu_features |= CPU_FEAT_TCE;
    klog("cpu: using translation cache extension on AMD!");
  }

  // Set the last bit so that we don't run this function more than once
  cpu_features |= (1ull << 63ull);
}

void cpu_early_init() {
  // Find features if we haven't done that already
  if (cpu_features == 0)
    detect_cpu_features();
  if (fpu_save_size == 0)
    fpu_init();

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
  cr4 |= (1 << 7)  |  // Enables Global Pages
         (1 << 9)  |  // Enables SSE and FXSAVE/FXRSTOR
         (1 << 10) |  // Allows for unmasked SSE exceptions
         (1 << 20);   // Enables Supervisor Mode Execution Prevention
  asm_write_cr4(cr4);

  uint64_t efer = asm_rdmsr(IA32_EFER);
  efer |= (1 << 11) |  // Enable No-Execute Pages
          (1 << 0);    // Enable the syscall/sysret mechanism

  // Enable Translation Cache Extension, a AMD only feature.
  if (CPU_CHECK(CPU_FEAT_TCE))
    efer |= (1 << 15);
  asm_wrmsr(IA32_EFER, efer);

  // Enable PCID
  cr4 = asm_read_cr4();
  if (CPU_CHECK(CPU_FEAT_PCID))
    cr4 |= (1 << 17);

  // Enable SMAP
  if (CPU_CHECK(CPU_FEAT_SMAP))
    cr4 |= (1 << 21);

  // Enable X{SAVE,RSTOR} instructions
  if (CPU_CHECK(CPU_FEAT_XSAVE))
    cr4 |= (1 << 18);
  asm_write_cr4(cr4);

  // Setup the syscall instruction
  asm_wrmsr(IA32_STAR, ((uint64_t)(GDT_KERNEL_DATA | 3)) << 48 | ((uint64_t)GDT_KERNEL_CODE) << 32);
  asm_wrmsr(IA32_LSTAR, (uintptr_t)asm_syscall_entry);
  asm_wrmsr(IA32_SFMASK, ~(uint32_t)2);

  // Finally, disable floating point emulation (for SSE)
  asm_write_cr0((asm_read_cr0() & ~(1 << 2)) | (1 << 1));
}

void cpu_save_thread(cpu_ctx_t* context) {
  this_cpu->cur_thread->context = *context;

  // Only save FPU/percpu stuff on usermode threads
  if (this_cpu->cur_thread->fpu_save_area) {
    this_cpu->cur_thread->client_gs = asm_rdmsr(IA32_GS_BASE);
    this_cpu->cur_thread->client_fs = asm_rdmsr(IA32_FS_BASE);
    fpu_save(this_cpu->cur_thread->fpu_save_area);
  }
}

void cpu_restore_thread(cpu_ctx_t* context) {
  thread_t* thrd = this_cpu->cur_thread;

  // If the FPU context is present, then the
  // segment registers need to be attended to as well
  if (thrd->fpu_save_area) {
    asm_wrmsr(IA32_GS_BASE, thrd->client_gs);
    asm_wrmsr(IA32_FS_BASE, thrd->client_fs);
    fpu_restore(thrd->fpu_save_area);
  }

  if (this_cpu->cur_spc != thrd->parent->space) {
    vm_space_load(thrd->parent->space);
    this_cpu->cur_spc = thrd->parent->space;
  }

  this_cpu->kernel_stack = thrd->syscall_stack;
  cpu_ctx_t* new_context = &this_cpu->cur_thread->context;

  if (new_context->cs & 3)
    asm_swapgs();
  sched_spinup(new_context);
}

void cpu_create_kctx(thread_t* thrd, uintptr_t entry, uint64_t arg1) {
  cpu_ctx_t* context = &thrd->context;

  // Create a 64KB stack...
  uintptr_t stack = (uintptr_t)vm_phys_alloc(16, VM_ALLOC_ZERO);
  stack += VM_MEM_OFFSET + (16 * VM_PAGE_SIZE);

  context->rdi = arg1;
  context->cs = GDT_KERNEL_CODE;
  context->ss = GDT_KERNEL_DATA;
  context->rflags = 0x202;
  context->rsp = stack;
  context->rip = entry;
}

// Helper macros for the stack tricks we do later...
#define push(ptr, value) *(--ptr) = value
#define auxval(ptr, tag, value) ({ \
  push(ptr, value);                \
  push(ptr, tag);                  \
});

void cpu_create_uctx(thread_t* thrd, struct exec_args args, bool elf) {
  cpu_ctx_t* context = &thrd->context;

  // Create a 32KB user stack (mapped at 0x70000000000)...
  uintptr_t stack_base = 0x0;
  if (elf) {
    stack_base = (uintptr_t)vm_phys_alloc(8, VM_ALLOC_ZERO);
    vm_map_range(thrd->parent->space, stack_base, (uintptr_t)THREAD_STACK_BASE,
                 8 * VM_PAGE_SIZE, VM_PERM_READ | VM_PERM_WRITE | VM_PERM_USER);
  }

  // And a 16KB kernel stack
  thrd->syscall_stack = (uintptr_t)vm_phys_alloc(16, VM_ALLOC_ZERO);
  thrd->syscall_stack += VM_MEM_OFFSET + (16 * VM_PAGE_SIZE);

  // Fill in the initial values of the context
  context->cs = GDT_USER_CODE | 3;
  context->ss = GDT_USER_DATA | 3;
  context->rflags = 0x202;
  context->rsp = THREAD_STACK_BASE + (8 * VM_PAGE_SIZE);
  context->rip = args.entry;

  if (!elf)
    return;

  // Push the SYSV mandated elements to the stack, starting with
  // all the raw strings
  size_t narg = 0, nenv = 0;
  mg_disable();
  char* sptr = (char*)(stack_base + (8 * VM_PAGE_SIZE) + VM_MEM_OFFSET);
  uint64_t* stack;

  // Push all raw strings to the stack...
  for (const char** env = args.envp; *env; env++, nenv++) {
    sptr -= (strlen(*env) + 1);
    strcpy(sptr, *env);
  }
  for (const char** arg = args.argp; *arg; arg++, narg++) {
    sptr -= (strlen(*arg) + 1);
    strcpy(sptr, *arg);
  }

  // Align the stack before we push anything else...
  stack = (size_t*)(sptr - ((uintptr_t)sptr & 0xf));
  if ((narg + nenv + 3) % 2)
    push(stack, 0);

  // Push all auxvals
  auxval(stack, AT_NULL,   0x0);
  auxval(stack, AT_ENTRY,  args.vec.at_entry);
  auxval(stack, AT_PHDR,   args.vec.at_phdr);
  auxval(stack, AT_PHENT,  args.vec.at_phent);
  auxval(stack, AT_PHNUM,  args.vec.at_phnum);
  auxval(stack, AT_CPUCAP, cpu_features);
  push(stack, 0);

  // Push pointers to the env strings
  stack -= nenv;
  sptr = (char*)(THREAD_STACK_BASE + (8 * VM_PAGE_SIZE));
  for (int i = 0; i < nenv; i++) {
    sptr -= strlen(args.envp[i]) + 1;
    stack[i] = (uint64_t)sptr;
  }

  // Push pointers to the argv strings
  push(stack, 0);
  stack -= narg;
  for (int i = 0; i < narg; i++) {
    sptr -= strlen(args.argp[i]) + 1;
    stack[i] = (uint64_t)sptr;
  }
  push(stack, narg);

  // Finalize RSP and return
  context->rsp = ((uintptr_t)stack - VM_MEM_OFFSET - stack_base) + THREAD_STACK_BASE;
  mg_enable();
}

