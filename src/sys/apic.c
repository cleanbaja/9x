#include <9x/vm.h>
#include <internal/asm.h>
#include <internal/cpuid.h>
#include <sys/apic.h>

#define LAPIC_SPURIOUS 0x0f0
#define LAPIC_ICR0 0x300
#define LAPIC_ICR1 0x310
#define LAPIC_EOI 0x0b0

static bool use_x2apic = false;
static uintptr_t xapic_base = 0x0;

static void
apic_write(uint32_t reg, uint64_t val)
{
  if (use_x2apic) {
    asm_wrmsr(IA32_x2APIC_BASE + (reg >> 4), val);
  } else {
    *((volatile uint32_t*)((xapic_base + VM_MEM_OFFSET) + reg)) = val;
  }
}

static uint64_t
apic_read(uint32_t reg)
{
  if (use_x2apic) {
    return asm_rdmsr(IA32_x2APIC_BASE + (reg >> 4));
  } else {
    return *((volatile uint32_t*)((xapic_base + VM_MEM_OFFSET) + reg));
  }
}

void
apic_eoi()
{
  apic_write(LAPIC_EOI, 0);
}

static uint32_t
generate_flags(enum ipi_mode md)
{
  uint32_t to_return = 0;

  switch (md) {
    case IPI_OTHERS:
      to_return |=
        (1 << 18) | (1 << 19) | (1 << 14); // One time only, All excluding self
      break;
    case IPI_SPECIFIC:
      to_return |= (1 << 14);              // One time only, Use CPU num in destination field
      break;
    case IPI_EVERYONE:
      to_return |= (1 << 19) | (1 << 14);  // One time only, Send to all including self
      break;
    default:
      log("apic: Unknown IPI mode %d", md);
      break;
  }

  return to_return;
}

void
send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode)
{
  uint32_t icr_low = 0;
  uint32_t icr_high = 0;

  // Use the self IPI register (if possible)
  if (use_x2apic && mode == IPI_SELF)
    asm_wrmsr(0x83F, vec);

  icr_low = generate_flags(mode);
  icr_low |= vec;

  if (use_x2apic) {
    icr_high = cpu;
    apic_write(LAPIC_ICR0, ((uint64_t)icr_high << 32) | icr_low);
  } else {
    icr_high = ((uint8_t)cpu << 24);
    apic_write(LAPIC_ICR0, icr_low);
    apic_write(LAPIC_ICR1, icr_high);
  }
}

void
activate_apic()
{
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx);

  if (ecx & CPUID_ECX_x2APIC) {
    use_x2apic =
      true; // We always use x2apic if its available, since its quite better
  }

  uint64_t apic_msr = asm_rdmsr(IA32_APIC);
  apic_msr |= (1 << 11);          // Enable the APIC
  apic_msr |= (use_x2apic << 10); // Set to x2apic mode (if possible)
  asm_wrmsr(IA32_APIC, apic_msr);

  if (!use_x2apic && apic_msr & (1 << 8)) {
    xapic_base = asm_rdmsr(IA32_APIC) & 0xfffff000;
    vm_virt_unmap(&kernel_space, xapic_base + VM_MEM_OFFSET);
    vm_virt_map(&kernel_space,
                xapic_base,
                xapic_base + VM_MEM_OFFSET,
                VM_PERM_READ | VM_PERM_WRITE);
  }

  // Commence the reciving of interrupts...
  apic_write(LAPIC_SPURIOUS, apic_read(LAPIC_SPURIOUS) | (1 << 8) | 0xFF);
  apic_write(0x80, 0);

  if (apic_msr & (1 << 8))
    log("apic: enabled in %s mode (base: 0x%lx)",
        use_x2apic ? "x2APIC" : "xAPIC",
        xapic_base);
}
