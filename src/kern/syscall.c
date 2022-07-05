#include <ninex/syscall.h>
#include <lib/builtin.h>
#include <lib/errno.h>
#include <lib/kcon.h>
#include <arch/hat.h>
#include <vm/seg.h>

static void stubcall(cpu_ctx_t* context) {
  PANIC(NULL, "syscall: call %d is a stub!\n", CALLNUM(context));
}

static void sys_debug_log(cpu_ctx_t* context) {
  // To make the output cleaner, use a 'mlibc' prefix unless the message
  // is from the rtdl, which has its own message
  char* message = (char*)ARG0(context);
  bool is_rtdl_message = false;
  if (memcmp("rtdl:", message, 5) == 0)
    is_rtdl_message = true;

  if (is_rtdl_message) {
    klog("%s", message);
  } else {
    klog("mlibc: %s", message);
  }
}

// Macros to help with extracting prot/flags from the context
#define GET_FLAGS(var) (int)(var & 0xFFFFFFFFLL)
#define GET_PROT(var)  (int)((var & 0xFFFFFFFF00000000LL) >> 32)

static void sys_vm_map(cpu_ctx_t* context) {
  void** window = (void**)ARG0(context);
  uint64_t flg = ARG1(context);
  size_t size = ARG2(context);
  int fd = ARG3(context);
  size_t offset = ARG4(context);
  uintptr_t hint = ARG5(context);

  if (!(GET_FLAGS(flg) & MAP_ANON)) {
    klog("syscall: vnode mappings aren't supported!");
    set_errno(ENOTSUP);
    *window = (void*)((uintptr_t)-1);
    return;
  }

  struct vm_seg* result = vm_create_seg(GET_FLAGS(flg), GET_PROT(flg), size, hint);
  if (result)
    *window = (void*)result->base;
  else
    *window = (void*)((uintptr_t)-1);
}

static void sys_vm_unmap(cpu_ctx_t* context) {
  uintptr_t ptr = ARG0(context);
  size_t len = ARG1(context);

  if (len == 0 || (ptr % cur_config->page_size != 0)) {
    set_errno(EINVAL);
    return;
  }

  uint64_t offset;
  struct vm_seg* sg = vm_find_seg(ptr, &offset);
  if (sg == NULL) {
    set_errno(EINVAL);
    return;
  }

  if (!sg->ops.unmap(sg, ptr, len))
    set_errno(EINVAL);
}

uintptr_t syscall_table[] = {
  [SYS_DEBUG_LOG] = (uintptr_t)sys_debug_log,
  [SYS_OPEN]      = (uintptr_t)stubcall,
  [SYS_VM_MAP]    = (uintptr_t)sys_vm_map,
  [SYS_VM_UNMAP]  = (uintptr_t)sys_vm_unmap,
  [SYS_EXIT]      = (uintptr_t)stubcall,
  [SYS_READ]      = (uintptr_t)stubcall,
  [SYS_WRITE]     = (uintptr_t)stubcall,
  [SYS_CLOSE]     = (uintptr_t)stubcall,
  [SYS_ARCHCTL]   = (uintptr_t)syscall_archctl
};
uintptr_t nr_syscalls = ARRAY_LEN(syscall_table);

