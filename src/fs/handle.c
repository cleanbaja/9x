#include <lib/errno.h>
#include <fs/handle.h>
#include <fs/vfs.h>
#include <arch/smp.h>
#include <vm/vm.h>

ssize_t
default_read(struct vnode* b, void* buf, off_t loc, size_t count)
{
  (void)b;
  (void)buf;
  (void)loc;
  (void)count;

  return -1;
}

ssize_t
default_write(struct vnode* b, const void* buf, off_t loc, size_t count)
{
  (void)b;
  (void)buf;
  (void)loc;
  (void)count;

  return -1;
}

ssize_t
default_ioctl(struct vnode* b, int64_t req, void* argp)
{
  (void)b;
  (void)req;
  (void)argp;

  return -1;
}

ssize_t
default_resize(struct vnode* b, off_t new_size)
{
  (void)b;
  (void)new_size;

  return -1;
}

struct vnode*
create_resource(size_t extra_bytes) {
  struct vnode* bk = NULL;
  if (extra_bytes == 0) {
    bk = (struct vnode*)kmalloc(sizeof(struct vnode));
  } else if (extra_bytes >= sizeof(struct vnode)) {
    bk = (struct vnode*)kmalloc(extra_bytes);
  } else {
    return NULL;
  }

  bk->read = default_read;
  bk->write = default_write;
  bk->ioctl = default_ioctl;
  bk->resize = default_resize;
  bk->refcount = 1;

  return bk;
}

struct handle* handle_open(const char* path, int flags, int creat_mode) {
  bool create = flags & O_CREAT;
  struct vfs_resolved_node res = vfs_resolve(this_cpu->cur_thread->parent->cwd, (char*)path, ((create) ? RESOLVE_CREATE_SHALLOW : 0));
  if (!res.success) {
    set_errno(ENOENT);
    goto failed;
  } else if (res.target->backing != NULL && S_ISDIR(res.target->backing->st.st_mode)) {
    set_errno(EISDIR);
    goto failed;
  }

  res.target->refcount++;
  if (res.target->backing == NULL && create)
    res.target->backing = res.parent->fs->open(res.target, true, creat_mode);

  struct handle* hnd = kmalloc(sizeof(struct handle));
  hnd->data.res = res.target->backing;
  hnd->flags = flags;
  hnd->refcount = 1;
  return hnd;

failed:
  return NULL;
}
