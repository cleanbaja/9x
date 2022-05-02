#include <ninex/acpi.h>
#include <ninex/smp.h>
#include <arch/asm.h>
#include <arch/cpuid.h>
#include <lib/kcon.h>
#include <arch/ic.h>
#include <arch/tables.h>
#include <arch/timer.h>
#include <vm/virt.h>
#include <vm/vm.h>

#define LAPIC_SPURIOUS 0x0f0
#define LAPIC_ICR0     0x300
#define LAPIC_ICR1     0x310
#define LAPIC_EOI      0x0b0
#define LAPIC_ESR      0x280
#define LAPIC_ERR_CTL  0x370
#define LAPIC_SELF_IPI 0x3F0

#define LAPIC_TIMER_LVT  0x320
#define LAPIC_TIMER_CNT  0x390
#define LAPIC_TIMER_INIT 0x380
#define LAPIC_TIMER_DIV  0x3E0

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

static uint32_t
ioapic_read(size_t ioapic_num, uint32_t reg)
{
  volatile uint32_t* base =
    (volatile uint32_t*)((size_t)madt_ioapics.data[ioapic_num]->addr +
                         VM_MEM_OFFSET);
  *base = reg;
  return *(base + 4);
}

static void
ioapic_write(size_t ioapic_num, uint32_t reg, uint32_t data)
{
  volatile uint32_t* base =
    (volatile uint32_t*)((size_t)madt_ioapics.data[ioapic_num]->addr +
                         VM_MEM_OFFSET);
  *base = reg;
  *(base + 4) = data;
}

void ic_eoi()
{
  apic_write(LAPIC_EOI, 0);
}

void ic_send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode)
{
  uint32_t icr_low = 0;

  // Use the x2APIC self IPI MSR, if possible
  if (mode == IPI_SELF && use_x2apic)
    apic_write(LAPIC_SELF_IPI, vec);

  // Encode the vector and deal with the rest of the IPI modes
  icr_low |= vec;
  switch (mode) {
    case IPI_OTHERS:
      icr_low |= (1 << 18) | (1 << 19);
      break;
    case IPI_EVERYONE:
      icr_low |= (1 << 19);
      break;
    case IPI_SELF:
      cpu = 0;
  }

  // Encode and send the IPI...
  if (use_x2apic) {
    apic_write(LAPIC_ICR0, ((uint64_t)cpu << 32) | icr_low);
  } else {
    apic_write(LAPIC_ICR1, ((uint32_t)cpu << 24));
    apic_write(LAPIC_ICR0, icr_low);

    while (apic_read(LAPIC_ICR0) & (1 << 12));
  }
}

void ic_enable()
{
  // Check for the x2APIC
  if (!use_x2apic) {
    uint32_t eax, ebx, ecx, edx;
    cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx); 
    
    if (ecx & CPUID_ECX_x2APIC) {
      use_x2apic = true;
    }
  }

  // Enable the APIC (hardware level)
  uint64_t apic_msr = asm_rdmsr(IA32_APIC);
  apic_msr |= (use_x2apic << 10); // Set x2apic (if available)
  apic_msr |= (1 << 11);          // Enable the APIC
  asm_wrmsr(IA32_APIC, apic_msr);

  // Map the APIC into memory
  if (!use_x2apic && xapic_base == 0x0) {
    xapic_base = asm_rdmsr(IA32_APIC) & 0xfffff000;
    vm_virt_fragment(&kernel_space, xapic_base + VM_MEM_OFFSET, VM_PERM_READ | VM_PERM_WRITE);
    vm_virt_map(&kernel_space,
                xapic_base,
                xapic_base + VM_MEM_OFFSET,
                VM_PERM_READ | VM_PERM_WRITE | VM_CACHE_UNCACHED);
  }

  // Enable the APIC (software level) and interrupts
  apic_write(LAPIC_SPURIOUS, apic_read(LAPIC_SPURIOUS) | (1 << 8) | 0xFF);
  asm_write_cr8(0);

  if (apic_msr & (1 << 8))
    klog("apic: enabled in %s mode (base: 0x%lx)",
        use_x2apic ? "x2APIC" : "xAPIC",
        xapic_base);
}

static uint32_t get_max_redirects(size_t ioapic_num)
{
  return (ioapic_read(ioapic_num, 1) & 0xff0000) >> 16;
}

// Return the index of the I/O APIC that handles this redirect
static int64_t get_ioapic(uint32_t gsi) {
  for (size_t i = 0; i < madt_ioapics.length; i++) {
    if (madt_ioapics.data[i]->gsib <= gsi &&
        madt_ioapics.data[i]->gsib + get_max_redirects(i) > gsi)
      return i;
  }

  return -1;
}

void ic_mask_irq(uint16_t slot, bool status) {
  size_t   cur_apic = get_ioapic(slot);
  uint32_t ioredtbl = (slot - madt_ioapics.data[cur_apic]->gsib) * 2 + 16;
  uint32_t old_val  = ioapic_read(cur_apic, ioredtbl);

  if (status)
    ioapic_write(cur_apic, ioredtbl, old_val | (1 << 16));
  else
    ioapic_write(cur_apic, ioredtbl, old_val & (1 << 16));
}

void ic_create_redirect(uint8_t lapic_id, 
		        uint8_t vec, 
			uint16_t slot, 
			bool legacy)
{
  // We only support Active High and Edge Triggered interrupts
  // so don't set any mode-related bits...
  size_t cur_apic = get_ioapic(slot);
  uint64_t redir_entry = vec;
  uint32_t real_gsi = slot;

  if (legacy) {
    for (size_t i = 0; i < madt_isos.length; i++) {
      if (madt_isos.data[i]->irq_source == slot) {
        klog("apic: IRQ %u used by override.", slot);

	real_gsi = madt_isos.data[i]->gsi;
	if (madt_isos.data[i]->flags & 2)
          redir_entry |= (1 << 13);    // Active low
        if (madt_isos.data[i]->flags & 8)
          redir_entry |= (1 << 15);    // Level triggered
      }
    }
  }

  // Allow for tracebacks of the GSI, by wiring it into the handler
  get_handler(vec)->gsi = real_gsi;

  // Set the target CPU, then complete the redirection of the IRQ
  redir_entry |= ((uint64_t)lapic_id) << 56;
  uint32_t ioredtbl = (real_gsi - madt_ioapics.data[cur_apic]->gsib) * 2 + 16;
  ioapic_write(cur_apic, ioredtbl, (uint32_t)redir_entry);
  ioapic_write(cur_apic, ioredtbl + 1, (uint32_t)(redir_entry >> 32));
}

void ic_timer_calibrate() {
  // Setup the calibration, with divisor set to 16 (0x3)
  apic_write(LAPIC_TIMER_DIV, 0x3);
  apic_write(LAPIC_TIMER_LVT, 0xFF | (1 << 16));
  apic_write(LAPIC_TIMER_INIT, (uint32_t)-1);

  // Sleep for 10ms
  timer_msleep(10);

  // Set the frequency, then disable the timer once more
  per_cpu(lapic_freq) = (((uint32_t)-1) - apic_read(LAPIC_TIMER_CNT)) / 10ull;
  apic_write(LAPIC_TIMER_INIT, 0);
  apic_write(LAPIC_TIMER_LVT, (1 << 16));
}

void
ic_timer_oneshot(uint8_t vec, uint64_t ms)
{
  if (CPU_CHECK(CPU_FEAT_INVARIANT) && CPU_CHECK(CPU_FEAT_DEADLINE)) {
    apic_write(LAPIC_TIMER_LVT, (0b10 << 17) | vec);
    uint64_t goal = asm_rdtsc() + (ms * per_cpu(tsc_freq));

    asm_wrmsr(IA32_TSC_DEADLINE, goal);
  } else {
    // Stop the LAPIC timer
    apic_write(LAPIC_TIMER_INIT, 0x0);
    apic_write(LAPIC_TIMER_LVT, (1 << 16));

    // Calculate the total ticks we need
    uint64_t ticks = ms * per_cpu(lapic_freq);

    // Setup the registers
    apic_write(LAPIC_TIMER_LVT, (apic_read(LAPIC_TIMER_LVT) & ~(0b11 << 17)));
    apic_write(LAPIC_TIMER_LVT, (apic_read(LAPIC_TIMER_LVT) & 0xFFFFFF00) | vec);
    apic_write(LAPIC_TIMER_DIV, 0x3);
    apic_write(LAPIC_TIMER_INIT, per_cpu(lapic_freq) * ms);

    // Clear the mask, and off we go!
    apic_write(LAPIC_TIMER_LVT, apic_read(LAPIC_TIMER_LVT) & ~(1 << 16));
  }
}

void
ic_timer_stop() {
  apic_write(LAPIC_TIMER_INIT, 0);
  apic_write(LAPIC_TIMER_LVT, apic_read(LAPIC_TIMER_LVT) | (1 << 16));
}

