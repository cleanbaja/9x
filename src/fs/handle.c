#include <arch/smp.h>
#include <fs/handle.h>
#include <fs/vfs.h>
#include <lib/builtin.h>
#include <lib/errno.h>
#include <vm/vm.h>

ssize_t default_read(struct vnode *v, void *buf, off_t loc, size_t count) {
  (void)v;
  (void)buf;
  (void)loc;
  (void)count;

  return -1;
}

ssize_t default_write(struct vnode *v,
                      const void *buf,
                      off_t loc,
                      size_t count) {
  (void)v;
  (void)buf;
  (void)loc;
  (void)count;

  return -1;
}

ssize_t default_ioctl(struct vnode *v, int64_t req, void *argp) {
  (void)v;
  (void)req;
  (void)argp;

  set_errno(EINVAL);
  return -1;
}

ssize_t default_resize(struct vnode *v, off_t new_size) {
  (void)v;
  (void)new_size;

  return -1;
}

struct vnode *create_resource(size_t extra_bytes) {
  struct vnode *vk = NULL;
  if (extra_bytes == 0) {
    vk = (struct vnode *)kmalloc(sizeof(struct vnode));
  } else if (extra_bytes >= sizeof(struct vnode)) {
    vk = (struct vnode *)kmalloc(extra_bytes);
  } else {
    return NULL;
  }

  vk->read = default_read;
  vk->write = default_write;
  vk->ioctl = default_ioctl;
  vk->resize = default_resize;
  vk->refcount = 1;

  return vk;
}

struct handle *handle_open(struct process *proc,
                           const char *path,
                           int flags,
                           int creat_mode) {
  // TODO: Return EEXISTS if (creat_mode == (O_CREAT | O_EXCEL))
  struct vfs_resolved_node res =
      vfs_resolve(proc->cwd, (char *)path,
                  ((flags & O_CREAT) ? RESOLVE_CREATE_SHALLOW : 0));
  if (!res.success) {
    set_errno(ENOENT);
    goto failed;
  } else if (res.target->backing != NULL &&
             S_ISDIR(res.target->backing->st.st_mode)) {
    set_errno(EISDIR);
    goto failed;
  }

  res.target->backing->refcount++;
  kfree(res.raw_string);
  if (res.target->backing == NULL && (flags & O_CREAT))
    res.target->backing = res.parent->fs->open(res.target, true, creat_mode);

  struct handle *hnd = kmalloc(sizeof(struct handle));
  hnd->node = res.target->backing;
  hnd->file = res.target;
  hnd->flags = flags;
  hnd->refcount = 1;
  return hnd;

failed:
  kfree(res.raw_string);
  return NULL;
}

struct handle *handle_clone(struct handle *parent) {
  struct handle *hnd = kmalloc(sizeof(struct handle));
  memcpy(hnd, parent, sizeof(struct handle));
  hnd->refcount = 1;
  parent->refcount++;

  return hnd;
}
