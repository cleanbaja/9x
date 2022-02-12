#include <9x/vm.h>
#include <internal/asm.h>
#include <internal/cpuid.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/tables.h>

#define LAPIC_SPURIOUS 0x0f0
#define LAPIC_ICR0     0x300
#define LAPIC_ICR1     0x310
#define LAPIC_EOI      0x0b0
#define LAPIC_ESR      0x280
#define LAPIC_ERR_CTL  0x370

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
  uint32_t icr_low  = 0;
  uint32_t icr_high = 0;

  // Check for self IPIs, since they're handled diffrently
  if (mode == IPI_SELF) {
    if (use_x2apic) {
      asm_wrmsr(0x83F, vec); 
    } else {
      apic_write(LAPIC_ICR1, 0);
      apic_write(LAPIC_ICR0, vec | (1 << 14));
    }

    return;
  }

  // Generate the flags for the specific IPI mode
  icr_low = generate_flags(mode);
  icr_low |= vec;

  // Encode and send the IPI...
  if (use_x2apic) {
    apic_write(LAPIC_ICR0, ((uint64_t)cpu << 32) | icr_low);
  } else {
    apic_write(LAPIC_ICR1, ((uint32_t)cpu << 24));
    apic_write(LAPIC_ICR0, icr_low);

    while (apic_read(LAPIC_ICR0) & (1 << 12));
  }
}

void
activate_apic()
{
  uint32_t eax, ebx, ecx, edx;

  if (!use_x2apic) {
    cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx); 
    
    if (ecx & CPUID_ECX_x2APIC) {
      use_x2apic = true;
    }
  }

  uint64_t apic_msr = asm_rdmsr(IA32_APIC);
  apic_msr |= (use_x2apic << 10); // Set x2apic (if available)    
  apic_msr |= (1 << 11);          // Enable the APIC
  asm_wrmsr(IA32_APIC, apic_msr);

  if (!use_x2apic && apic_msr & (1 << 8)) {
    xapic_base = asm_rdmsr(IA32_APIC) & 0xfffff000;
    vm_virt_unmap(&kernel_space, xapic_base + VM_MEM_OFFSET);
    vm_virt_map(&kernel_space,
                xapic_base,
                xapic_base + VM_MEM_OFFSET,
                VM_PERM_READ | VM_PERM_WRITE | VM_CACHE_FLAG_UNCACHED);
  }

  // Commence the reciving of interrupts...
  apic_write(LAPIC_SPURIOUS, apic_read(LAPIC_SPURIOUS) | (1 << 8) | 0xFF);
  asm_write_cr8(0);

  if (apic_msr & (1 << 8))
    log("apic: enabled in %s mode (base: 0x%lx)",
        use_x2apic ? "x2APIC" : "xAPIC",
        xapic_base);
}

