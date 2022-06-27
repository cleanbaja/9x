#include <fs/backing.h>
#include <fs/vfs.h>
#include <lib/builtin.h>
#include <vm/vm.h>

struct tmpfs_backing {
  struct backing;
  size_t capacity;
  char* data;
};

// tmpfs starts out empty, so no need to populate
static struct vfs_node* tmpfs_populate(struct vfs_node* node) {
  (void)node;
  return NULL;
}

static ssize_t tmpfs_read(struct backing* bck,
                          void* buf,
                          off_t offset,
                          size_t count) {
  struct tmpfs_backing* b = (struct tmpfs_backing*)bck;
  spinlock(&b->lock);

  // Truncate the read if the size is too big!
  if (offset + count > b->st.st_size)
    count -= (offset + count) - b->st.st_size;

  // Read the file into the buffer
  memcpy(buf, b->data + offset, count);
  spinrelease(&b->lock);
  return count;
}

static ssize_t tmpfs_write(struct backing* bck,
                           const void* buf,
                           off_t offset,
                           size_t count) {
  struct tmpfs_backing* b = (struct tmpfs_backing*)bck;
  spinlock(&b->lock);

  // Grow the file if needed!
  if (offset + count > b->capacity) {
    while (offset + count > b->capacity)
      b->capacity <<= 1;

    b->data = krealloc(b->data, b->capacity);
  }

  // Update stuff...
  memcpy(b->data + offset, buf, count);
  b->st.st_size += count;
  spinrelease(&b->lock);

  return count;
}

static ssize_t tmpfs_resize(struct backing* bck, off_t new_size) {
  struct tmpfs_backing* b = (struct tmpfs_backing*)bck;
  spinlock(&b->lock);

  // Prevent downsizing...
  if (new_size <= b->capacity)
    return -1;

  // Grow the file
  while (new_size > b->capacity)
    b->capacity <<= 1;
  b->data = krealloc(b->data, b->capacity);

  // Update data structures
  b->st.st_size = new_size;
  spinrelease(&b->lock);
  return new_size;
}

static void tmpfs_close(struct backing* bck) {
  spinlock(&bck->lock);
  bck->refcount--;
  spinrelease(&bck->lock);
}

static struct backing* tmpfs_open(struct vfs_node* node,
                                  bool new_node,
                                  mode_t mode) {
  // We should only get called when creating a new resource/node
  if (!new_node)
    return NULL;

  struct tmpfs_backing* bck =
      (struct tmpfs_backing*)create_backing(sizeof(struct tmpfs_backing));

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
  bck->read = tmpfs_read;
  bck->write = tmpfs_write;
  bck->resize = tmpfs_resize;
  bck->close = tmpfs_close;

  return (struct backing*)bck;
}

static struct backing* tmpfs_mkdir(struct vfs_node* node, mode_t mode) {
  struct tmpfs_backing* bck =
      (struct tmpfs_backing*)create_backing(sizeof(struct tmpfs_backing));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFDIR;
  bck->st.st_nlink = 1;

  return (struct backing*)bck;
}

static struct backing* tmpfs_link(struct vfs_node* node, mode_t mode) {
  struct tmpfs_backing* bck =
      (struct tmpfs_backing*)create_backing(sizeof(struct tmpfs_backing));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFLNK;
  bck->st.st_nlink = 1;

  return (struct backing*)bck;
}

static struct vfs_node* tmpfs_mount(const char* base, struct vfs_node* parent) {
  parent->fs = &tmpfs;
  return vfs_create_node(base, parent);
}

struct filesystem tmpfs = {.name = "tmpfs",
                           .needs_backing = false,
                           .populate = tmpfs_populate,
                           .open = tmpfs_open,
                           .mkdir = tmpfs_mkdir,
                           .link = tmpfs_link,
                           .mount = tmpfs_mount};
