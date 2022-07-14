#ifndef FS_BACKING_H
#define FS_BACKING_H

#include <lib/types.h>
#include <lib/lock.h>
#include <lib/vec.h>
#include <stddef.h>

/*
 * Unix/Posix definitions...
 */
#define O_ACCMODE 0x0007
#define O_EXEC    0b1000
#define O_RDONLY  0b0100
#define O_RDWR    0b1100
#define O_SEARCH  0b0010
#define O_WRONLY  0b1010
#define CAN_READ(x)  ((x & O_RDONLY) || (x & O_RDWR))
#define CAN_WRITE(x)  ((x & O_WRONLY) || (x & O_RDWR))

#define O_APPEND 0x0008
#define O_CREAT 0x0010
#define O_DIRECTORY 0x0020
#define O_EXCL 0x0040
#define O_NOCTTY 0x0080
#define O_NOFOLLOW 0x0100
#define O_TRUNC 0x0200
#define O_NONBLOCK 0x0400
#define O_DSYNC 0x0800
#define O_RSYNC 0x1000
#define O_SYNC 0x2000
#define O_CLOEXEC 0x4000

#define S_IFMT 0x0F000
#define S_IFBLK 0x06000
#define S_IFCHR 0x02000
#define S_IFIFO 0x01000
#define S_IFREG 0x08000
#define S_IFDIR 0x04000
#define S_IFLNK 0x0A000
#define S_IFSOCK 0x0C000
#define S_IFPIPE 0x03000

#define S_ISBLK(m) (((m)&S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m)&S_IFMT) == S_IFCHR)
#define S_ISFIFO(m) (((m)&S_IFMT) == S_IFIFO)
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#define S_ISLNK(m) (((m)&S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m)&S_IFMT) == S_IFSOCK)

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14

struct stat
{
  dev_t st_dev;
  ino_t st_ino;
  mode_t st_mode;
  uint32_t st_nlink;
  uid_t st_uid;
  gid_t st_gid;
  dev_t st_rdev;
  off_t st_size;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  blksize_t st_blksize;
  blkcnt_t st_blocks;
};

struct dirent
{
  ino_t d_ino;
  off_t d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[1024];
};

/*
 * 9x internal definitions
 */
struct vnode {
  ssize_t (*read)(struct vnode*, void*, off_t, size_t);
  ssize_t (*write)(struct vnode*, const void*, off_t, size_t);
  ssize_t (*ioctl)(struct vnode*, int64_t, void*);
  ssize_t (*resize)(struct vnode*, off_t);
  void    (*close)(struct vnode* bck);

  struct  stat st;
  lock_t  lock;
  int64_t refcount;
};

struct handle {
  struct vfs_ent* file;
  union {
    vec_t(struct dirent*)* dirents;
    struct vnode* node;
  };

  uint32_t flags;
  lock_t lock;
  int64_t refcount, offset;
};

struct process;
struct vnode* create_resource(size_t extra_bytes);
struct handle* handle_open(struct process* proc, const char* path, int flags, int creat_mode);
struct handle* handle_clone(struct handle* parent);

#endif // FS_BACKING_H
