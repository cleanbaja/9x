#include <internal/cpuid.h>
#include <lib/log.h>
#include <vm.h>

uint64_t mmu_features = 0;

static void setup_feature_map() {
  // Set mmu_features with proper values
  uint32_t eax, ebx, ecx, edx;
  cpuid_subleaf(CPUID_EXTEND_FUNCTION_1, 0x0, &eax, &ebx, &ecx, &edx);

  if (edx & CPUID_EDX_XD_BIT_AVIL) {
    mmu_features |= (1 << 0);
  }
  if (edx & CPUID_EDX_PAGE1GB) {
    mmu_features |= (1 << 5); 
  }

  cpuid_subleaf(0x1, 0, &eax, &ebx, &ecx, &edx);
  if (ecx & CPUID_ECX_PCID) {
    mmu_features |= (1 << 4);
  }
  if (edx & CPUID_EDX_PGE) {
    mmu_features |= (1 << 1);
  }

  cpuid_subleaf(0x7, 0, &eax, &ebx, &ecx, &edx);
  if (ebx & CPUID_EBX_SMEP) {
    mmu_features |= (1 << 2);
  } 
  if (ebx & CPUID_EBX_SMAP) {
    mmu_features |= (1 << 3);
  }
  if (!(ebx & CPUID_EBX_INVPCID)) {
    // Don't say PCID is supported if there is no invpcid...
    mmu_features &= ~(1 << 4);
  }
}

void vm_init_virt() {
  setup_feature_map();

  char buf[250];
  snprintf(buf, 250, "virt: Features -> (%s %s %s %s %s %s)",
		     MMU_CHECK(MM_FEAT_NX)    ? "NX"     : "-NX",
		     MMU_CHECK(MM_FEAT_GLOBL) ? "GLOBAL" : "-GLOBAL",
		     MMU_CHECK(MM_FEAT_SMEP)  ? "SMEP"   : "-SMEP",
		     MMU_CHECK(MM_FEAT_SMAP)  ? "SMAP"   : "-SMAP",
		     MMU_CHECK(MM_FEAT_PCID)  ? "PCID"   : "-PCID",
		     MMU_CHECK(MM_FEAT_1GB)   ? "1GB"    : "-1GB");
  log(buf);
}

