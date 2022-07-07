#include <fs/devtmpfs.h>
#include <fs/vfs.h>
#include <lib/builtin.h>
#include <vm/vm.h>

/* A complete copy of tmpfs, except we create some files on mount */
struct devtmpfs_vnode {
  struct vnode;
  size_t capacity;
  char* data;
};

static struct vfs_ent* root_mount = NULL;
static int dev_counter = 1;

// devtmpfs starts out empty, so no need to populate
static struct vfs_ent* devtmpfs_populate(struct vfs_ent* node) {
  (void)node;
  return NULL;
}

static ssize_t devtmpfs_read(struct vnode* bck,
                             void* buf,
                             off_t offset,
                             size_t count) {
  struct devtmpfs_vnode* b = (struct devtmpfs_vnode*)bck;
  spinlock(&b->lock);

  // Truncate the read if the size is too big!
  if (offset + count > b->st.st_size)
    count -= (offset + count) - b->st.st_size;

  // Read the file into the buffer
  memcpy(buf, b->data + offset, count);
  spinrelease(&b->lock);
  return count;
}

static ssize_t devtmpfs_write(struct vnode* bck,
                              const void* buf,
                              off_t offset,
                              size_t count) {
  struct devtmpfs_vnode* b = (struct devtmpfs_vnode*)bck;
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

static ssize_t devtmpfs_resize(struct vnode* bck, off_t new_size) {
  struct devtmpfs_vnode* b = (struct devtmpfs_vnode*)bck;
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

static void devtmpfs_close(struct vnode* bck) {
  spinlock(&bck->lock);
  bck->refcount--;
  spinrelease(&bck->lock);
}

static struct vnode* devtmpfs_open(struct vfs_ent* node,
                                     bool new_node,
                                     mode_t mode) {
  // We should only get called when creating a new resource/node
  if (!new_node)
    return NULL;

  struct devtmpfs_vnode* bck =
      (struct devtmpfs_vnode*)create_resource(sizeof(struct devtmpfs_vnode));

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

  return (struct vnode*)bck;
}

static struct vnode* devtmpfs_mkdir(struct vfs_ent* node, mode_t mode) {
  struct devtmpfs_vnode* bck =
      (struct devtmpfs_vnode*)create_resource(sizeof(struct devtmpfs_vnode));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFDIR;
  bck->st.st_nlink = 1;

  return (struct vnode*)bck;
}

static struct vnode* devtmpfs_link(struct vfs_ent* node, mode_t mode) {
  struct devtmpfs_vnode* bck =
      (struct devtmpfs_vnode*)create_resource(sizeof(struct devtmpfs_vnode));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFLNK;
  bck->st.st_nlink = 1;

  return (struct vnode*)bck;
}

static struct vfs_ent* devtmpfs_mount(const char* base,
                                       struct vfs_ent* parent) {
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

struct vnode* devtmpfs_create_device(char* path, int size) {
  struct vfs_resolved_node res =
      vfs_resolve(root_mount, path, RESOLVE_CREATE_SHALLOW);
  if (!res.success)
    return NULL;

  kfree(res.raw_string);
  res.target->backing = create_resource(size);
  return res.target->backing;
}

struct filesystem devtmpfs = {.name = "devtmpfs",
                              .needs_backing = false,
                              .populate = devtmpfs_populate,
                              .open = devtmpfs_open,
                              .mkdir = devtmpfs_mkdir,
                              .link = devtmpfs_link,
                              .mount = devtmpfs_mount};
