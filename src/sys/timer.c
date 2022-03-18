#include <9x/acpi.h>
#include <internal/asm.h>
#include <lib/builtin.h>
#include <lib/lock.h>
#include <lib/log.h>
#include <sys/apic.h>
#include <sys/timer.h>
#include <vm/vm.h>

static void* hpet_base;
static uint64_t hpet_period;
static CREATE_SPINLOCK(timer_lock);

#define FEMTO_PER_MS     (1000000000000)
#define TSC_CALI_CYCLES 2

static uint64_t hpet_read(uint16_t offset)
{
    return *((volatile uint64_t*)(hpet_base + offset));
}

static void hpet_write(uint16_t offset, uint64_t val)
{
    *((volatile uint64_t*)(hpet_base + offset)) = val;
}

static void
hpet_sleep(uint64_t ms)
{
  uint64_t goal =
    hpet_read(HPET_MAIN_COUNTER_REG) + (ms * (FEMTO_PER_MS / hpet_period));

  while (hpet_read(HPET_MAIN_COUNTER_REG) < goal)
    __asm__ volatile("pause");
}

void
calibrate_tsc()
{
  // Determine the TSC frequency
  for (int i = 0; i < TSC_CALI_CYCLES; i++) {
    uint64_t start_reading = asm_rdtsc();
    hpet_sleep(10);
    uint64_t final_reading = asm_rdtsc();

    per_cpu(tsc_freq) += ((final_reading - start_reading) / 10);
  }

   // Average out readings
   per_cpu(tsc_freq) /= TSC_CALI_CYCLES;
   if (cpunum() == 0) {
     uint64_t n = per_cpu(tsc_freq) / 1000;
     int d4 = (n % 10);
     int d3 = (n / 10) % 10;
     int d2 = (n / 100) % 10;
     int d1 = (n / 1000);
     log("timer/tsc: CPU frequency is locked in at %d.%d%d%d GHz",
         d1,
         d2,
         d3,
         d4);
     timer_sleep(5000);
   }
}

void timer_init() {
  // First, check if we're the BSP, or APs
  if (!(cpunum() == 0))
    goto common;

  // Find the HPET table
  acpi_hpet_t* hp = acpi_query("HPET", 0);
  if (hp == NULL)
    PANIC(NULL, "HPET not present on this system\n");

  // Map the HPET into memory
  hpet_base = (void*)((uintptr_t)hp->base.base + VM_MEM_OFFSET);
  vm_virt_map(per_cpu(cur_space), hp->base.base, (uintptr_t)hpet_base, VM_PERM_READ | VM_PERM_WRITE | VM_CACHE_FLAG_UNCACHED);
  
  // Set the period, clear the main counter, and enable the HPET
  hpet_period = (hpet_read(HPET_CAP_REG) >> 32);
  hpet_write(HPET_MAIN_COUNTER_REG, 0);
  hpet_write(HPET_CONF_REG, hpet_read(HPET_CONF_REG) | (1 << 0));

common:
  // Calibrate the TSC, only if it's usable
  if (CPU_CHECK(CPU_FEAT_INVARIANT))
    calibrate_tsc();

  // Calibrate the APIC, only if it's needed as a time source
  if (!CPU_CHECK(CPU_FEAT_DEADLINE))
    calibrate_apic();
}

void timer_sleep(uint64_t ms) {
  // Always use the TSC when possible, its much better
  if (!CPU_CHECK(CPU_FEAT_INVARIANT)) {
    uint64_t goal = hpet_read(HPET_MAIN_COUNTER_REG) + (ms * (FEMTO_PER_MS / hpet_period));

    while(hpet_read(HPET_MAIN_COUNTER_REG) < goal)
        __asm__ volatile("pause");
  } else {
    uint64_t goal = asm_rdtsc() + (ms * per_cpu(tsc_freq));

    while (asm_rdtsc() < goal)
        __asm__ volatile("pause");
  }
}

