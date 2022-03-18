#include <fs/backing.h>
#include <vm/vm.h>

ssize_t
default_read(struct backing* b, void* buf, off_t loc, size_t count)
{
  (void)b;
  (void)buf;
  (void)loc;
  (void)count;

  return -1;
}

ssize_t
default_write(struct backing* b, const void* buf, off_t loc, size_t count)
{
  (void)b;
  (void)buf;
  (void)loc;
  (void)count;

  return -1;
}

ssize_t
default_ioctl(struct backing* b, int64_t req, void* argp)
{
  (void)b;
  (void)req;
  (void)argp;

  return -1;
}

ssize_t
default_resize(struct backing* b, off_t new_size)
{
  (void)b;
  (void)new_size;

  return -1;
}

struct backing*
create_backing(size_t extra_bytes)
{
  struct backing* bk = NULL;
  if (extra_bytes == 0) {
    bk = (struct backing*)kmalloc(sizeof(struct backing));
  } else if (extra_bytes >= sizeof(struct backing)) {
    bk = (struct backing*)kmalloc(extra_bytes);
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
