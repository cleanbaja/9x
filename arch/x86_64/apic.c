#include <arch/cpuid.h>
#include <arch/irqchip.h>
#include <arch/smp.h>
#include <arch/tables.h>
#include <arch/timer.h>
#include <lib/kcon.h>
#include <ninex/acpi.h>
#include <ninex/irq.h>
#include <vm/virt.h>
#include <vm/vm.h>

#define LAPIC_SPURIOUS 0x0f0
#define LAPIC_EOI 0x0b0
#define LAPIC_ID 0x020
#define LAPIC_ICR0     0x300
#define LAPIC_ICR1     0x310
#define LAPIC_SELF_IPI 0x3F0

#define LAPIC_TIMER_LVT  0x320
#define LAPIC_TIMER_CNT  0x390
#define LAPIC_TIMER_INIT 0x380
#define LAPIC_TIMER_DIV  0x3E0

#define IOAPIC_REG_VER 0x01

//////////////////////////////
//    Core APIC Interface
//////////////////////////////
vec_t(madt_lapic_t*) madt_lapics;
vec_t(madt_ioapic_t*) madt_ioapics;
vec_t(madt_iso_t*) madt_isos;
vec_t(madt_nmi_t*) madt_nmis;

static bool use_x2apic = false;
static uintptr_t xapic_base = 0x0;
static uint8_t translation_table[256];
CREATE_STAGE_SMP_NODEP(apic_ready, ic_enable)
CREATE_STAGE(scan_madt_target, madt_init, {apic_ready});

static void xapic_write(uint32_t reg, uint64_t val) {
  if (use_x2apic) {
    asm_wrmsr(IA32_x2APIC_BASE + (reg >> 4), val);
  } else {
    *((volatile uint32_t*)((xapic_base + VM_MEM_OFFSET) + reg)) = val;
  }
}

static uint64_t xapic_read(uint32_t reg) {
  if (use_x2apic) {
    return asm_rdmsr(IA32_x2APIC_BASE + (reg >> 4));
  } else {
    return *((volatile uint32_t*)((xapic_base + VM_MEM_OFFSET) + reg));
  }
}

uint32_t get_lapic_id() {
  if (use_x2apic) {
    return xapic_read(LAPIC_ID);
  } else {
    return (xapic_read(LAPIC_ID) >> 24);
  }
}

void ic_eoi() {
  xapic_write(LAPIC_EOI, 0);
}

void ic_send_ipi(uint8_t vec, uint32_t cpu, enum ipi_mode mode) {
  uint32_t icr_low = 0;

  // Use the x2APIC self IPI MSR, if possible
  if (mode == IPI_SELF && use_x2apic)
    xapic_write(LAPIC_SELF_IPI, vec);

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
    xapic_write(LAPIC_ICR0, ((uint64_t)cpu << 32) | icr_low);
  } else {
    xapic_write(LAPIC_ICR1, ((uint32_t)cpu << 24));
    xapic_write(LAPIC_ICR0, icr_low);

    while (xapic_read(LAPIC_ICR0) & (1 << 12))
      ;
  }
}

void ic_perform_startup(uint32_t apic_id) {
  if (use_x2apic) {
    // Do the IPI in two 64-bit writes
    xapic_write(LAPIC_ICR0, 0x4500 | ((uint64_t)apic_id << 32));
    xapic_write(LAPIC_ICR0,
                0x4600 | (0x80000 / 4096) | ((uint64_t)apic_id << 32));
  } else {
    // Send the INIT ipi
    xapic_write(LAPIC_ICR1, (apic_id << 24));
    xapic_write(LAPIC_ICR0, 0x4500);

    // Then send the startup address of the AP (0x80 for 0x80000)
    xapic_write(LAPIC_ICR1, (apic_id << 24));
    xapic_write(LAPIC_ICR0, 0x4600 | 0x80);

    // Wait for the second IPI to complete...
    while (xapic_read(LAPIC_ICR0) & (1 << 12))
      ;
  }
}

static void ic_enable() {
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

  // NOTE: We no longer map the APIC into memory, since
  // we assume the host loader/firmware has done so already
  if (!use_x2apic) {
    xapic_base = asm_rdmsr(IA32_APIC) & 0xfffff000;
  }

  // Enable the APIC (software level) and interrupts
  xapic_write(LAPIC_SPURIOUS, xapic_read(LAPIC_SPURIOUS) | (1 << 8) | 0xFF);
  asm_write_cr8(0);

  if (apic_msr & (1 << 8))
    klog("apic: enabled in %s mode!", use_x2apic ? "x2APIC" : "xAPIC",
         xapic_base);
}

//////////////////////////////
//        APIC Timer
//////////////////////////////
void ic_timer_calibrate() {
  // Setup the calibration, with divisor set to 16 (0x3)
  xapic_write(LAPIC_TIMER_DIV, 0x3);
  xapic_write(LAPIC_TIMER_LVT, 0xFF | (1 << 16));
  xapic_write(LAPIC_TIMER_INIT, (uint32_t)-1);

  // Sleep for 10ms
  timer_msleep(10);

  // Set the frequency, then disable the timer once more
  this_cpu->lapic_freq = (((uint32_t)-1) - xapic_read(LAPIC_TIMER_CNT)) / 10ull;
  xapic_write(LAPIC_TIMER_INIT, 0);
  xapic_write(LAPIC_TIMER_LVT, (1 << 16));
}

void ic_timer_oneshot(uint8_t vec, uint64_t ms) {
  if (CPU_CHECK(CPU_FEAT_INVARIANT) && CPU_CHECK(CPU_FEAT_DEADLINE)) {
    xapic_write(LAPIC_TIMER_LVT, (0b10 << 17) | vec);
    uint64_t goal = asm_rdtsc() + (ms * this_cpu->tsc_freq);

    asm_wrmsr(IA32_TSC_DEADLINE, goal);
  } else {
    // Stop the LAPIC timer
    xapic_write(LAPIC_TIMER_INIT, 0x0);
    xapic_write(LAPIC_TIMER_LVT, (1 << 16));

    // Calculate the total ticks we need
    uint64_t ticks = ms * this_cpu->lapic_freq;

    // Setup the registers
    xapic_write(LAPIC_TIMER_LVT, (xapic_read(LAPIC_TIMER_LVT) & ~(0b11 << 17)));
    xapic_write(LAPIC_TIMER_LVT,
                (xapic_read(LAPIC_TIMER_LVT) & 0xFFFFFF00) | vec);
    xapic_write(LAPIC_TIMER_DIV, 0x3);
    xapic_write(LAPIC_TIMER_INIT, this_cpu->lapic_freq * ms);

    // Clear the mask, and off we go!
    xapic_write(LAPIC_TIMER_LVT, xapic_read(LAPIC_TIMER_LVT) & ~(1 << 16));
  }
}

void ic_timer_stop() {
  xapic_write(LAPIC_TIMER_INIT, 0);
  xapic_write(LAPIC_TIMER_LVT, xapic_read(LAPIC_TIMER_LVT) | (1 << 16));
}

///////////////////////////////
//     IO-APIC Controller
//////////////////////////////
#define max_redir(id) ((ioapic_read(id, IOAPIC_REG_VER) >> 16) & 0x1F)
#define nth_ioapic(n) (madt_ioapics.data[n])

static uint32_t ioapic_read(size_t ioapic_num, uint32_t reg) {
  volatile uint32_t* base =
      (volatile uint32_t*)((size_t)nth_ioapic(ioapic_num)->addr +
                           VM_MEM_OFFSET);
  *base = reg;
  return *(base + 4);
}

static void ioapic_write(size_t ioapic_num, uint32_t reg, uint32_t data) {
  volatile uint32_t* base =
      (volatile uint32_t*)((size_t)nth_ioapic(ioapic_num)->addr +
                           VM_MEM_OFFSET);
  *base = reg;
  *(base + 4) = data;
}

static uint32_t get_ioapic_for_gsi(uint32_t gsi) {
  for (int i = 0; i < madt_ioapics.length; i++) {
    if (nth_ioapic(i)->gsib <= gsi &&
        nth_ioapic(i)->gsib + max_redir(i) > gsi) {
      return i;
    }
  }

  return (uint32_t)-1;
}

void ic_mask_irq(uint16_t slot, bool status) {
  uint32_t gsi = translation_table[slot];
  if (gsi == 0) {
    klog("apic: can't mask IO-APIC IRQ from vector, if GSI is unknown!");
    return;
  }

  // Check to see if the IRQ is a legacy one...
  for (size_t i = 0; i < madt_isos.length; i++) {
    if (madt_isos.data[i]->irq_source == gsi) {
      gsi = madt_isos.data[i]->gsi;
    }
  }

  size_t cur_apic = get_ioapic_for_gsi(gsi);
  uint32_t ioredtbl = (gsi - nth_ioapic(cur_apic)->gsib) * 2 + 16;
  uint32_t old_val  = ioapic_read(cur_apic, ioredtbl);

  if (status)
    ioapic_write(cur_apic, ioredtbl, old_val | (1 << 16));
  else
    ioapic_write(cur_apic, ioredtbl, old_val & ~(1 << 16));
}

void ic_create_redirect(uint8_t lapic_id,
                        uint8_t vec,
                        uint16_t slot,
                        int flags,
                        bool legacy) {
  size_t cur_apic = get_ioapic_for_gsi(slot);
  uint64_t redir_entry = vec | flags;
  uint32_t real_gsi = slot;

  if (legacy) {
    for (size_t i = 0; i < madt_isos.length; i++) {
      if (madt_isos.data[i]->irq_source == slot) {
	real_gsi = madt_isos.data[i]->gsi;

        if (madt_isos.data[i]->flags & 2)
          redir_entry |= (1 << 13);    // Active low
        if (madt_isos.data[i]->flags & 8)
          redir_entry |= (1 << 15);    // Level triggered
      }
    }
  }

  // Allow for tracebacks of the GSI, by wiring it into the table
  translation_table[vec] = real_gsi;
  get_irq_handler(vec)->eoi_strategy =
      (redir_entry & (1 << 15)) ? EOI_MODE_LEVEL : EOI_MODE_EDGE;

  // Set the target CPU, then complete the redirection of the IRQ
  redir_entry |= ((uint64_t)lapic_id) << 56;
  uint32_t ioredtbl = (real_gsi - nth_ioapic(cur_apic)->gsib) * 2 + 16;
  ioapic_write(cur_apic, ioredtbl, (uint32_t)redir_entry);
  ioapic_write(cur_apic, ioredtbl + 1, (uint32_t)(redir_entry >> 32));
}

//////////////////////////////
//        MADT Parser
//////////////////////////////
static void madt_init() {
  acpi_madt_t* madt = (acpi_madt_t*)acpi_query("APIC", 0);
  if (madt == NULL)
    PANIC(NULL, "Unable to find ACPI MADT table");

  // Parse the actual entries
  for (uint8_t* madt_ptr = (uint8_t*)madt->entries;
       (uintptr_t)madt_ptr < (uintptr_t)madt + madt->header.length;
       madt_ptr += *(madt_ptr + 1)) {
    switch (*(madt_ptr)) {
      case 0:  // Processor Local APIC
        vec_push(&madt_lapics, (madt_lapic_t*)madt_ptr);
        break;
      case 1: {  // I/O APIC
        madt_ioapic_t* ap = (madt_ioapic_t*)madt_ptr;
        vec_push(&madt_ioapics, ap);
        klog("madt: IOAPIC[%d] maps to GSIs %d-%d!", ap->id, ap->gsib,
             ap->gsib + max_redir(madt_ioapics.length - 1));
        break;
      }
      case 2:  // Interrupt Source Override
        vec_push(&madt_isos, (madt_iso_t*)madt_ptr);
        break;
      case 4:  // Non-Maskable Interrupt
        vec_push(&madt_nmis, (madt_nmi_t*)madt_ptr);
        break;
    }
  }

  klog("madt: Detected %d ISOs and %d NMIs", madt_isos.length,
       madt_nmis.length);
}
