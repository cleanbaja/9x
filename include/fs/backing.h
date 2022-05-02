#ifndef FS_BACKING_H
#define FS_BACKING_H

#include <lib/posix.h>
#include <lib/lock.h>
#include <stddef.h>

struct backing {
  ssize_t (*read)(struct backing*, void*, off_t, size_t);
  ssize_t (*write)(struct backing*, const void*, off_t, size_t);
  ssize_t (*ioctl)(struct backing*, int64_t, void*);
  ssize_t (*resize)(struct backing*, off_t);
  void    (*close)(struct backing* bck);

  struct  stat st;
  struct  spinlock lock;
  int64_t refcount;
};

struct backing* create_backing(size_t extra_bytes);

#endif // FS_BACKING_H
