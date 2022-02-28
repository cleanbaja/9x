#include <sys/timer.h>
#include <lib/log.h>
#include <9x/acpi.h>
#include <lib/lock.h>
#include <internal/asm.h>

static void* hpet_base;
static uint64_t hpet_period;
static CREATE_SPINLOCK(timer_lock);

#define FEMTO_PER_MS     (1000000000000)
#define TSC_CALI_CYCLES  5

static uint64_t hpet_read(uint16_t offset)
{
    return *((volatile uint64_t*)(hpet_base + offset));
}

static void hpet_write(uint16_t offset, uint64_t val)
{
    *((volatile uint64_t*)(hpet_base + offset)) = val;
}

void calibrate_tsc() {
   for (int i = 0; i < TSC_CALI_CYCLES; i++) {
     uint64_t start_reading = asm_rdtsc();
     timer_sleep(10);
     uint64_t delta = (asm_rdtsc() - start_reading) / 10;

     per_cpu(tsc_freq) += delta;
   } 

   // Average out readings
   per_cpu(tsc_freq) /= TSC_CALI_CYCLES;
   
   if (per_cpu(cpu_num) == 0)
     log("timer/tsc: CPU has a CPU frequency of %u MHz", per_cpu(cpu_num), per_cpu(tsc_freq) / 1000);
}

void timer_init() {
  // First, check if we're the BSP, or APs
  if (!(per_cpu(cpu_num) == 0))
    goto common;

  // Find the HPET table
  acpi_hpet_t* hp = acpi_query("HPET", 0);
  if (hp == NULL)
    PANIC(NULL, "HPET not present on this system\n");

  // Map the HPET into memory
  hpet_base = (void*)((uintptr_t)hp->base.base + VM_MEM_OFFSET);
  vm_virt_map(per_cpu(cur_space), hp->base.base, (uintptr_t)hpet_base, VM_PERM_READ | VM_PERM_WRITE | VM_CACHE_FLAG_UNCACHED);
  log("timer/hpet: cap reg -> (period: %u, counters: %u)", (hpet_read(HPET_CAP_REG) >> 32), ((hpet_read(HPET_CAP_REG) >> 8) & 0x1F) + 1); 
  
  // Set the period, clear the main counter, and enable the HPET
  hpet_period = (hpet_read(HPET_CAP_REG) >> 32);
  hpet_write(HPET_MAIN_COUNTER_REG, 0);
  hpet_write(HPET_CONF_REG, hpet_read(HPET_CONF_REG) | (1 << 0));
  
common:
  // Calibrate the TSC, only if its usable
  if (CPU_CHECK(CPU_FEAT_INVARIANT))
    calibrate_tsc();

  // TODO: Setup LAPIC timer (if needed?)
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

