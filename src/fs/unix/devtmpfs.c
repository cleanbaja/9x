#include <fs/devtmpfs.h>
#include <fs/vfs.h>
#include <lib/builtin.h>
#include <vm/vm.h>

/* A complete copy of tmpfs, except we create some files on mount */
struct devtmpfs_backing {
  struct backing;
  size_t capacity;
  char* data;
};

static struct vfs_node* root_mount = NULL;
static int dev_counter = 1;

// devtmpfs starts out empty, so no need to populate
static struct vfs_node* devtmpfs_populate(struct vfs_node* node) {
  (void)node;
  return NULL;
}

static ssize_t devtmpfs_read(struct backing* bck,
                             void* buf,
                             off_t offset,
                             size_t count) {
  struct devtmpfs_backing* b = (struct devtmpfs_backing*)bck;
  spinlock_acquire(&b->lock);

  // Truncate the read if the size is too big!
  if (offset + count > b->st.st_size)
    count -= (offset + count) - b->st.st_size;

  // Read the file into the buffer
  memcpy(buf, b->data + offset, count);
  spinlock_release(&b->lock);
  return count;
}

static ssize_t devtmpfs_write(struct backing* bck,
                              const void* buf,
                              off_t offset,
                              size_t count) {
  struct devtmpfs_backing* b = (struct devtmpfs_backing*)bck;
  spinlock_acquire(&b->lock);

  // Grow the file if needed!
  if (offset + count > b->capacity) {
    while (offset + count > b->capacity)
      b->capacity <<= 1;

    b->data = krealloc(b->data, b->capacity);
  }

  // Update stuff...
  memcpy(b->data + offset, buf, count);
  b->st.st_size += count;
  spinlock_release(&b->lock);

  return count;
}

static ssize_t devtmpfs_resize(struct backing* bck, off_t new_size) {
  struct devtmpfs_backing* b = (struct devtmpfs_backing*)bck;
  spinlock_acquire(&b->lock);

  // Prevent downsizing...
  if (new_size <= b->capacity)
    return -1;

  // Grow the file
  while (new_size > b->capacity)
    b->capacity <<= 1;
  b->data = krealloc(b->data, b->capacity);

  // Update data structures
  b->st.st_size = new_size;
  spinlock_release(&b->lock);
  return new_size;
}

static void devtmpfs_close(struct backing* bck) {
  spinlock_acquire(&bck->lock);
  bck->refcount--;
  spinlock_release(&bck->lock);
}

static struct backing* devtmpfs_open(struct vfs_node* node,
                                     bool new_node,
                                     mode_t mode) {
  // We should only get called when creating a new resource/node
  if (!new_node)
    return NULL;

  struct devtmpfs_backing* bck =
      (struct devtmpfs_backing*)create_backing(sizeof(struct devtmpfs_backing));

  // Fill in the backing with proper values
  bck->capacity = 4096;
  bck->data = kmalloc(bck->capacity);
  bck->st.st_dev = 1;  // TODO: Respect Device IDs
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;  // TODO: Respect Inodes
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFREG;
  bck->st.st_nlink = 1;
  bck->refcount = 1;
  bck->read = devtmpfs_read;
  bck->write = devtmpfs_write;
  bck->resize = devtmpfs_resize;
  bck->close = devtmpfs_close;

  return (struct backing*)bck;
}

static struct backing* devtmpfs_mkdir(struct vfs_node* node, mode_t mode) {
  struct devtmpfs_backing* bck =
      (struct devtmpfs_backing*)create_backing(sizeof(struct devtmpfs_backing));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFDIR;
  bck->st.st_nlink = 1;

  return (struct backing*)bck;
}

static struct backing* devtmpfs_link(struct vfs_node* node, mode_t mode) {
  struct devtmpfs_backing* bck =
      (struct devtmpfs_backing*)create_backing(sizeof(struct devtmpfs_backing));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFLNK;
  bck->st.st_nlink = 1;

  return (struct backing*)bck;
}

static struct vfs_node* devtmpfs_mount(const char* base,
                                       struct vfs_node* parent) {
  // TODO: Support multiple devtmpfs mounts
  if (root_mount)
    return NULL;

  parent->fs = &devtmpfs;
  root_mount = vfs_create_node(base, parent);
  return root_mount;
}

size_t devtmpfs_create_id(int subclass) {
  return MKDEV(dev_counter++, subclass);
}

struct backing* devtmpfs_create_device(char* path, int size) {
  struct vfs_resolved_node res =
      vfs_resolve(root_mount, path, RESOLVE_CREATE_SHALLOW);
  if (!res.success)
    return NULL;

  kfree(res.raw_string);
  res.target->backing = create_backing(size);
  return res.target->backing;
}

struct filesystem devtmpfs = {.name = "devtmpfs",
                              .needs_backing = false,
                              .populate = devtmpfs_populate,
                              .open = devtmpfs_open,
                              .mkdir = devtmpfs_mkdir,
                              .link = devtmpfs_link,
                              .mount = devtmpfs_mount};
