#include <ninex/syscall.h>
#include <lib/builtin.h>
#include <lib/errno.h>
#include <lib/kcon.h>
#include <arch/hat.h>
#include <arch/smp.h>
#include <fs/handle.h>
#include <vm/seg.h>

static void stubcall(cpu_ctx_t* context) {
  PANIC(NULL, "syscall: call %d is a stub!\n", CALLNUM(context));
}

static void sys_debug_log(cpu_ctx_t* context) {
  // To make the output cleaner, use a 'mlibc' prefix unless the message
  // is from the rtdl, which has its own prefix
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

static void sys_open(cpu_ctx_t* context) {
  const char* path = (const char*)ARG0(context);
  int flags = ARG1(context);
  mode_t mode = ARG2(context);
  int* fd = (int*)ARG3(context);

  struct handle* hnd = handle_open(path, flags, mode);
  if (hnd == NULL) {
    *fd = -1;
    return;
  }

  *fd = this_cpu->cur_thread->parent->fd_counter++;
  htab_insert(&this_cpu->cur_thread->parent->handles, fd, sizeof(int), hnd);
  return;
}

// Small macro to help with fd to handle conversion
#define fd_to_handle(fd, fail_label) ({                                                   \
  struct handle* result =                                                                 \
    (struct handle*)htab_find(&this_cpu->cur_thread->parent->handles, &fd, sizeof(int));  \
  if (result == NULL) {                                                                   \
    set_errno(EBADF);                                                                     \
    goto fail_label;                                                                      \
  }                                                                                       \
  result;                                                                                 \
})

static void sys_read(cpu_ctx_t* context) {
  struct handle* result = fd_to_handle(ARG0(context), failed);
  if (result->flags & O_WRONLY) {
    set_errno(EINVAL);
    goto failed;
  }

  *((uint64_t*)ARG3(context)) = result->data.res->read(result->data.res, (void*)ARG1(context), result->offset, ARG2(context));
  result->offset += *((uint64_t*)ARG3(context));

failed:
  return;
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
static void sys_seek(cpu_ctx_t* context) {
  struct handle* result = fd_to_handle(ARG0(context), failed);

  switch(ARG2(context)) {
    case SEEK_SET:
      result->offset = ARG1(context);
      break;
    case SEEK_CUR:
      result->offset += ARG1(context);
      break;
    case SEEK_END:
      result->offset = ARG1(context) + result->data.res->st.st_size;
      break;
    default:
      set_errno(EINVAL);
      goto failed;
  }

  *((uint64_t*)ARG3(context)) = result->offset;

failed:
  return;
}

static void sys_close(cpu_ctx_t* context) {
  struct handle* result = fd_to_handle(ARG0(context), exit);
  result->data.res->close(result->data.res);
  result->refcount--;

  if (result->refcount == 0)
    kfree(result);

exit:
  return;
}

uintptr_t syscall_table[] = {
  [SYS_DEBUG_LOG] = (uintptr_t)sys_debug_log,
  [SYS_OPEN]      = (uintptr_t)sys_open,
  [SYS_VM_MAP]    = (uintptr_t)sys_vm_map,
  [SYS_VM_UNMAP]  = (uintptr_t)sys_vm_unmap,
  [SYS_EXIT]      = (uintptr_t)stubcall,
  [SYS_READ]      = (uintptr_t)sys_read,
  [SYS_WRITE]     = (uintptr_t)stubcall,
  [SYS_SEEK]      = (uintptr_t)sys_seek,
  [SYS_CLOSE]     = (uintptr_t)sys_close,
  [SYS_ARCHCTL]   = (uintptr_t)syscall_archctl
};
uintptr_t nr_syscalls = ARRAY_LEN(syscall_table);

