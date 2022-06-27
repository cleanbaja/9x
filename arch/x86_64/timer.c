#include <arch/cpuid.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <lib/kcon.h>
#include <lib/lock.h>
#include <ninex/acpi.h>
#include <vm/vm.h>

#define HPET_REG_CAP 0x0
#define HPET_REG_CONF 0x10
#define HPET_REG_COUNTER 0x0F0

static void* hpet_base = NULL;
static uint64_t hpet_period = 0;
static lock_t timer_lock;

static inline uint64_t hpet_read(uint16_t reg) {
  return *((volatile uint64_t*)(hpet_base + reg));
}

static inline void hpet_write(uint16_t reg, uint64_t val) {
  *((volatile uint64_t*)(hpet_base + reg)) = val;
}

void hpet_sleep(uint64_t ms) {
  if (hpet_base == NULL) {
    acpi_hpet_t* h = acpi_query("HPET", 0);
    if (h == NULL)
      PANIC(NULL, "Unable to find HPET on this system!\n");

    // NOTE: Once again, it is the responsibility of the
    // loader/firmware to map the HPET into memory, with
    // the proper permissions/attributes
    hpet_base = (void*)(h->base.base + VM_MEM_OFFSET);

    // Make sure the HPET isn't bogus
    uint32_t reg_count = ((hpet_read(HPET_REG_CAP) >> 8) & 0x1F) + 1;
    if ((reg_count < 2) || ((hpet_read(HPET_REG_CAP) >> 32) == 0))
      PANIC(NULL, "HPET does not support required capabilities!\n");

    // Set the period, and enable the HPET
    hpet_period = (hpet_read(HPET_REG_CAP) >> 32);
    hpet_write(HPET_REG_COUNTER, 0);
    hpet_write(HPET_REG_CONF, hpet_read(HPET_REG_CONF) | (1 << 0));
  }

  uint64_t goal =
      hpet_read(HPET_REG_COUNTER) + (ms * (1000000000000 / hpet_period));
  while (hpet_read(HPET_REG_COUNTER) < goal)
    asm("pause");
}

static bool cpuid_calibrate_tsc() {
  // Perform a quick mitigation for QEMU/KVM, since it supports 15h, but gives
  // bad values...
  uint32_t max_cpuid_level, unused0, unused1, unused2;
  cpuid_subleaf(0x0U, 0x0U, &max_cpuid_level, &unused0, &unused1, &unused2);

  if (max_cpuid_level >= 0x15U) {
    uint32_t eax_denominator, ebx_numerator, ecx_hz, reserved;

    cpuid_subleaf(0x15U, 0x0U, &eax_denominator, &ebx_numerator, &ecx_hz,
                  &reserved);

    if ((eax_denominator != 0U) && (ebx_numerator != 0U)) {
      this_cpu->tsc_freq =
          (((uint64_t)ecx_hz * ebx_numerator) / eax_denominator) / 1000U;
    }
  }

  if ((this_cpu->tsc_freq == 0UL) && (max_cpuid_level >= 0x16U)) {
    uint32_t eax_base_mhz, ebx_max_mhz, ecx_bus_mhz, edx;

    cpuid_subleaf(0x16U, 0x0U, &eax_base_mhz, &ebx_max_mhz, &ecx_bus_mhz, &edx);
    this_cpu->tsc_freq = (uint64_t)eax_base_mhz * 1000U;
  }

  return (this_cpu->tsc_freq != 0);
}

void timer_cali() {
  // Before anything, try to calibrate the APIC Timer, then grab the spinlock
  ic_timer_cali();
  spinlock(&timer_lock);

  // Make sure that we can proceed with the callibration
  if (!CPU_CHECK(CPU_FEAT_INVARIANT)) {
    if (is_bsp())
      klog("tsc: Invariant TSC is not supported, falling back to HPET!");

    spinrelease(&timer_lock);
    return;
  }

  // Try using CPUID to calibrate the TSC...
  if (cpuid_calibrate_tsc())
    goto check_cpu_speed;

  // Settle for regular calibration with the HPET
  uint64_t start = 0, delta = 0;
  this_cpu->tsc_freq = 0x0;

  // Run the actual calibration
  for (int i = 0; i < TSC_CALI_CYCLES; i++) {
    start = asm_rdtsc();
    hpet_sleep(10);
    delta = (asm_rdtsc() - start) / 10;

    this_cpu->tsc_freq += delta;
  }

  // Even out the readings, for the final result
  this_cpu->tsc_freq /= TSC_CALI_CYCLES;

check_cpu_speed:
  if ((this_cpu->tsc_freq / 1000) == 0) {
    PANIC(NULL, "CPU frequency is too slow, get a faster computer!");
  }

  if (is_bsp()) {
    uint64_t n = this_cpu->tsc_freq / 1000;
    int d4 = (n % 10);
    int d3 = (n / 10) % 10;
    int d2 = (n / 100) % 10;
    int d1 = (n / 1000);
    klog("tsc: CPU frequency is locked at %d.%d%d%d GHz",
        d1,
        d2,
        d3,
        d4);
  }

  spinrelease(&timer_lock);
}

void timer_usleep(uint64_t us) {
  if (CPU_CHECK(CPU_FEAT_INVARIANT) && this_cpu->tsc_freq) {
    uint64_t goal = asm_rdtsc() + (us * (this_cpu->tsc_freq / 1000));

    while (asm_rdtsc() < goal)
      asm("pause");
  } else {
    // Boy, you sure wish you had that Invariant TSC, don't you...
    hpet_sleep(((us / 1000) == 0) ? 1 : (us / 1000));
  }
}

void timer_msleep(uint64_t ms) {
  if (CPU_CHECK(CPU_FEAT_INVARIANT) && this_cpu->tsc_freq) {
    uint64_t goal = asm_rdtsc() + (ms * this_cpu->tsc_freq);

    while (asm_rdtsc() < goal)
      asm("pause");
  } else {
    hpet_sleep(ms);
  }
}

