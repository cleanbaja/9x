#include <generic/acpi.h>
#include <arch/timer.h>
#include <arch/cpu.h>
#include <lib/log.h>
#include <generic/smp.h>
#include <vm/virt.h>
#include <vm/vm.h>

#define FEMTO_PER_MS    (1000000000000)
#define FEMTO_PER_US    (1000000)
static void* hpet_base = NULL;
uint32_t hpet_period = 0x0;

static inline uint64_t hpet_read(uint16_t reg) {
  return *((volatile uint64_t*)(hpet_base + reg));
}

static inline void hpet_write(uint16_t reg, uint64_t val) {
  *((volatile uint64_t*)(hpet_base + reg)) = val;
}

static void hpet_init() {
  // Find the HPET table
  acpi_hpet_t* h = acpi_query("HPET", 0);
  if (h == NULL)
    PANIC(NULL, "Unable to find HPET on this system!\n");
 
  // Map the HPET into memory (as device mem)
  hpet_base = (void*)(h->base.base + VM_MEM_OFFSET);
  vm_virt_fragment(&kernel_space, (uintptr_t)hpet_base, VM_PERM_READ | VM_PERM_WRITE);
  vm_virt_map(&kernel_space, h->base.base, (uintptr_t)hpet_base, VM_PERM_READ | VM_PERM_WRITE | VM_CACHE_UNCACHED);
  
  // Make sure the HPET isn't bogus
  uint32_t reg_count = ((hpet_read(HPET_REG_CAP) >> 8) & 0x1F) + 1;
  if ((reg_count < 2) || ((hpet_read(HPET_REG_CAP) >> 32) == 0))
    PANIC(NULL, "HPET does not support required capabilities!\n");

  // Set the period, and enable the HPET
  hpet_period = (hpet_read(HPET_REG_CAP) >> 32);
  hpet_write(HPET_REG_COUNTER, 0);
  hpet_write(HPET_REG_CONF, hpet_read(HPET_REG_CONF) | (1 << 0));
}

void tsc_calibrate() {
  // Make sure that we can proceed with the callibration
  if (hpet_base == 0x0) hpet_init();
  if (!CPU_CHECK(CPU_FEAT_INVARIANT)) {
    if (is_bsp())
      log("tsc: Invariant TSC is not supported!");
 
    goto outro;
  }

  // TODO: Use other methods to get the CPU frequency, like CPUID

  // Setup the variables for calibration, and remove
  // the INVARIANT feat so that we may use the HPET
  cpu_features &= ~CPU_FEAT_INVARIANT;
  uint64_t start = 0, delta = 0;
  per_cpu(tsc_freq) = 0x0;

  // Run the actual calibration
  for (int i = 0; i < TSC_CALI_CYCLES; i++) {
    start = asm_rdtsc();  
    timer_msleep(10);
    delta = (asm_rdtsc() - start) / 10;
    
    per_cpu(tsc_freq) += delta;
  }

  // Restore the INVARIANT feat, and even out the readings
  per_cpu(tsc_freq) /= TSC_CALI_CYCLES;
  cpu_features |= CPU_FEAT_INVARIANT;

  if (is_bsp()) {
    uint64_t n = per_cpu(tsc_freq) / 1000;
    int d4 = (n % 10);
    int d3 = (n / 10) % 10;
    int d2 = (n / 100) % 10;
    int d1 = (n / 1000);
    log("tsc: CPU frequency is locked at %d.%d%d%d GHz",
        d1,
        d2,
        d3,
        d4);
  }

outro:
  if (!CPU_CHECK(CPU_FEAT_DEADLINE)) { apic_calibrate(); }
}


void timer_usleep(uint64_t us) {
  if (!CPU_CHECK(CPU_FEAT_INVARIANT)) {
    uint64_t goal = hpet_read(HPET_REG_COUNTER) + (us * (FEMTO_PER_US / hpet_period));

    while(hpet_read(HPET_REG_COUNTER) < goal)
        __asm__ volatile("pause");  
  }
}

void timer_msleep(uint64_t ms) {
  if (!CPU_CHECK(CPU_FEAT_INVARIANT)) {
    uint64_t goal = hpet_read(HPET_REG_COUNTER) + (ms * (FEMTO_PER_MS / hpet_period));

    while(hpet_read(HPET_REG_COUNTER) < goal)
        __asm__ volatile("pause");
  } else {
    uint64_t goal = asm_rdtsc() + (ms * per_cpu(tsc_freq));
   
    while (asm_rdtsc() < goal)
      __asm__ volatile ("pause");
  }
}

