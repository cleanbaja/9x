#include <ninex/syscall.h>
#include <ninex/sched.h>
#include <lib/builtin.h>
#include <lib/errno.h>
#include <lib/kcon.h>
#include <arch/hat.h>
#include <arch/smp.h>
#include <arch/arch.h>
#include <fs/vfs.h>
#include <vm/seg.h>

////////////////////////
//   Syscall Helpers
////////////////////////
static void stubcall(cpu_ctx_t* context) {
  PANIC(NULL, "syscall: call %d is a stub!\n", CALLNUM(context));
}

// This function is implmented really badly, so I need to
// find a better way to get the job done
static char* user_strdup(uintptr_t str) {
  mg_disable();
  int str_length = strlen((char*)str);
  mg_enable();

  char* result = kmalloc(str_length);
  mg_copy_from_user(result, (void*)str, str_length);
  return result;
}

// Small macro for writing to a usermode pointer
#define sc_write(pointer, data, type) ({                 \
  if (!mg_validate((uintptr_t)pointer, sizeof(type))) {  \
    set_errno(EFAULT);                                   \
    return;                                              \
  }                                                      \
  mg_disable();                                          \
  *((type*)pointer) = (type)data;                        \
  mg_enable();                                           \
})

// Another small macro to help with fd to handle conversion
#define openfd(fd) ({                                                               \
  struct handle* result =                                                                 \
    (struct handle*)htab_find(&this_cpu->cur_thread->parent->handles, &fd, sizeof(int));  \
  if (result == NULL) {                                                                   \
    set_errno(EBADF);                                                                     \
    return;                                                                               \
  }                                                                                       \
  result;                                                                                 \
})

/////////////////////////////
//  Syscall Implmentations
/////////////////////////////
static void sys_debug_log(cpu_ctx_t* context) {
  char* message = user_strdup(ARG0(context));
  if (message == NULL)
    return;

  // To make the output cleaner, use a 'mlibc' prefix unless the message
  // is from the rtdl, which has its own prefix
  bool is_rtdl_message = false;
  if (memcmp("ldso:", message, 5) == 0)
    is_rtdl_message = true;

  if (is_rtdl_message) {
    klog("%s", message);
  } else {
    klog("mlibc: %s", message);
  }

  kfree(message);
}

// Macros to help with extracting prot/flags from the context
#define GET_FLAGS(var) (int)(var & 0xFFFFFFFFLL)
#define GET_PROT(var)  (int)((var & 0xFFFFFFFF00000000LL) >> 32)

static void sys_vm_map(cpu_ctx_t* context) {
  void** window = (void**)ARG0(context);
  uint64_t flg = ARG1(context);
  size_t size = ARG2(context);
  uintptr_t hint = ARG5(context);

  if (!(GET_FLAGS(flg) & MAP_ANON)) {
    klog("syscall: vnode mappings aren't supported!");
    set_errno(ENOTSUP);
    sc_write(window, ((uintptr_t)-1), uintptr_t);
    return;
  }

  struct vm_seg* result = vm_create_seg(GET_FLAGS(flg), GET_PROT(flg), size, hint);
  if (result)
    sc_write(window, result->base, void*);
  else
    sc_write(window, ((uintptr_t)-1), uintptr_t);
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
  int    flags = ARG1(context);
  mode_t mode  = ARG2(context);
  char*  path  = user_strdup(ARG0(context));
  if (path == NULL) {
    set_errno(EINVAL);
    sc_write(ARG3(context), -1, int);
    return;
  }
  klog("opening path %s", path);

  // mlibc's rtdl opens with flags as 0, which is not allowed
  // therefore, assume O_RDWR when flags are 0
  if (flags == 0)
    flags = O_RDWR;

  struct handle* hnd = handle_open(this_cpu->cur_thread->parent, path, flags, mode);
  if (hnd == NULL) {
    sc_write(ARG3(context), -1, int);
    goto finished;
  }

  int fd = this_cpu->cur_thread->parent->fd_counter++;
  sc_write(ARG3(context), fd, int);
  htab_insert(&this_cpu->cur_thread->parent->handles, &fd, sizeof(int), hnd);

finished:
  kfree(path);
  return;
}

static void sys_read(cpu_ctx_t* context) {
  struct handle* result = openfd(ARG0(context));

  if (!CAN_READ(result->flags)) {
    set_errno(EINVAL);
    return;
  }

  // Validate the pointer to the buffer
  if (!mg_validate(ARG1(context), ARG2(context))) {
    set_errno(EINVAL);
    return;
  } else {
    mg_disable(); // Disable SMAP/PAN instead of using a scratch buffer
  }

  size_t bytes_read = result->node->read(result->node, (void*)ARG1(context), result->offset, ARG2(context));
  result->offset += bytes_read;
  sc_write(ARG3(context), bytes_read, size_t);
  mg_enable();
}

static void sys_write(cpu_ctx_t* context) {
  struct handle* result = openfd(ARG0(context));

  if (!CAN_WRITE(result->flags)) {
    set_errno(EINVAL);
    return;
  }

  // Validate the pointer to the buffer
  if (!mg_validate(ARG1(context), ARG2(context))) {
    set_errno(EINVAL);
    return;
  } else {
    mg_disable();
  }

  size_t bytes_written = result->node->write(result->node, (void*)ARG1(context), result->offset, ARG2(context));
  result->offset += bytes_written;
  sc_write(ARG3(context), bytes_written, size_t);
  mg_enable();
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
static void sys_seek(cpu_ctx_t* context) {
  struct handle* result = openfd(ARG0(context));

  switch(ARG2(context)) {
    case SEEK_SET:
      result->offset = ARG1(context);
      break;
    case SEEK_CUR:
      result->offset += ARG1(context);
      break;
    case SEEK_END:
      result->offset = ARG1(context) + result->node->st.st_size;
      break;
    default:
      set_errno(EINVAL);
      return;
  }

  sc_write(ARG3(context), result->offset, size_t);
}



static void sys_close(cpu_ctx_t* context) {
  struct handle* result = openfd(ARG0(context));
  result->node->close(result->node);
  result->refcount--;

  htab_delete(&this_cpu->cur_thread->parent->handles, &ARG0(context), sizeof(int));
  if (result->refcount == 0)
    kfree(result);
}

static void sys_get_pid(cpu_ctx_t* context) {
  sc_write(ARG0(context), this_cpu->cur_thread->parent->pid, pid_t);
}

static void sys_get_ppid(cpu_ctx_t* context) {
  sc_write(ARG0(context), this_cpu->cur_thread->parent->ppid, pid_t);
}

static void sys_exit(cpu_ctx_t* context) {
  int status = (int)ARG0(context);
  proc_t* process = this_cpu->cur_thread->parent;

  // Disable interrupts, so that the scheduler doesn't return us
  asm volatile ("cli");

  // Start by stopping all threads associated with this process
  for (int i = 0; i < process->threads.length; i++) {
    if (process->threads.data[i] == this_cpu->cur_thread)
      continue;

    sched_die(process->threads.data[i]);
  }

  // Next, close all file descriptors
  for (int i = 0; i < process->handles.capacity; i++) {
    struct handle* hl = process->handles.data[i];
    if (hl == NULL)
      continue;

    hl->node->close(hl->node);
    hl->refcount--;

    if (hl->refcount == 0)
      kfree(hl);
  }

  // Then switch to the kernel's space, and destroy the user's
  vm_space_t* proc_space = process->space;
  vm_space_load(&kernel_space);
  vm_space_destroy(proc_space);

  // Finally, kill the thread we're currently running on
  process->status = status | 0x200;
  sched_die(NULL);
}

static void sys_fcntl(cpu_ctx_t* context) {
  set_errno(ENOSYS);
}

static void sys_ioctl(cpu_ctx_t* context) {
  struct handle* hnd = openfd(ARG0(context));
  if (!mg_validate(ARG3(context), 16)) {
    set_errno(EFAULT);
    return;
  }

  int result = hnd->node->ioctl(hnd->node, ARG1(context), (void*)ARG2(context));
  sc_write(ARG3(context), result, int);
}

static void sys_getcwd(cpu_ctx_t* context) {
  if (!mg_validate(ARG0(context), ARG1(context))) {
    set_errno(EFAULT);
    return;
  }

  char* result = vfs_get_path(this_cpu->cur_thread->parent->cwd);
  if (strlen(result) > ARG1(context)) {
    set_errno(ERANGE);
  } else {
    mg_copy_to_user((void*)ARG0(context), result, strlen(result));
  }

  kfree(result);
}

uintptr_t syscall_table[] = {
  [SYS_DEBUG_LOG] = (uintptr_t)sys_debug_log,
  [SYS_OPEN]      = (uintptr_t)sys_open,
  [SYS_VM_MAP]    = (uintptr_t)sys_vm_map,
  [SYS_VM_UNMAP]  = (uintptr_t)sys_vm_unmap,
  [SYS_EXIT]      = (uintptr_t)sys_exit,
  [SYS_READ]      = (uintptr_t)sys_read,
  [SYS_WRITE]     = (uintptr_t)sys_write,
  [SYS_SEEK]      = (uintptr_t)sys_seek,
  [SYS_CLOSE]     = (uintptr_t)sys_close,
  [SYS_GETPID]    = (uintptr_t)sys_get_pid,
  [SYS_GETPPID]   = (uintptr_t)sys_get_ppid,
  [SYS_ARCHCTL]   = (uintptr_t)syscall_archctl,
  [SYS_FCNTL]     = (uintptr_t)sys_fcntl,
  [SYS_IOCTL]     = (uintptr_t)sys_ioctl,
  [SYS_GETCWD]    = (uintptr_t)sys_getcwd
};
uintptr_t nr_syscalls = ARRAY_LEN(syscall_table);

