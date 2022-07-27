#include <fs/handle.h>
#include <fs/vfs.h>
#include <lib/builtin.h>
#include <vm/vm.h>

struct tmpfs_vnode {
  struct vnode;
  size_t capacity;
  char *data;
};

static ssize_t tmpfs_read(struct vnode *bck,
                          void *buf,
                          off_t offset,
                          size_t count) {
  struct tmpfs_vnode *b = (struct tmpfs_vnode *)bck;
  spinlock(&b->lock);

  // Truncate the read if the size is too big!
  if (offset + count > b->st.st_size) count -= (offset + count) - b->st.st_size;

  // Read the file into the buffer
  memcpy(buf, b->data + offset, count);
  spinrelease(&b->lock);
  return count;
}

static ssize_t tmpfs_write(struct vnode *bck,
                           const void *buf,
                           off_t offset,
                           size_t count) {
  struct tmpfs_vnode *b = (struct tmpfs_vnode *)bck;
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

static ssize_t tmpfs_resize(struct vnode *bck, off_t new_size) {
  struct tmpfs_vnode *b = (struct tmpfs_vnode *)bck;
  spinlock(&b->lock);

  // Prevent downsizing...
  if (new_size <= b->capacity) return -1;

  // Grow the file
  while (new_size > b->capacity)
    b->capacity <<= 1;
  b->data = krealloc(b->data, b->capacity);

  // Update data structures
  b->st.st_size = new_size;
  spinrelease(&b->lock);
  return new_size;
}

static void tmpfs_close(struct vnode *bck) {
  spinlock(&bck->lock);
  bck->refcount--;
  spinrelease(&bck->lock);
}

static struct vnode *tmpfs_open(struct vfs_ent *node,
                                bool new_node,
                                mode_t mode) {
  // We should only get called when creating a new resource/node
  if (!new_node) return NULL;

  struct tmpfs_vnode *bck =
      (struct tmpfs_vnode *)create_resource(sizeof(struct tmpfs_vnode));

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

  return (struct vnode *)bck;
}

static struct vnode *tmpfs_mkdir(struct vfs_ent *node, mode_t mode) {
  struct tmpfs_vnode *bck =
      (struct tmpfs_vnode *)create_resource(sizeof(struct tmpfs_vnode));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFDIR;
  bck->st.st_nlink = 1;

  return (struct vnode *)bck;
}

static struct vnode *tmpfs_link(struct vfs_ent *node, mode_t mode) {
  struct tmpfs_vnode *bck =
      (struct tmpfs_vnode *)create_resource(sizeof(struct tmpfs_vnode));

  bck->st.st_dev = 1;
  bck->st.st_size = 0;
  bck->st.st_blocks = 0;
  bck->st.st_blksize = 512;
  bck->st.st_ino = 1;
  bck->st.st_mode = (mode & ~S_IFMT) | S_IFLNK;
  bck->st.st_nlink = 1;

  return (struct vnode *)bck;
}

static struct vfs_ent *tmpfs_mount(const char *basename,
                                   mode_t mount_perms,
                                   struct vfs_ent *parent,
                                   struct vfs_ent *source) {
  (void)source;
  struct vfs_ent *newent = vfs_create_node(basename, parent);
  newent->backing = tmpfs_mkdir(newent, mount_perms);
  newent->fs = &tmpfs;
  return newent;
}

struct filesystem tmpfs = {.name = "tmpfs",
                           .needs_backing = false,
                           .open = tmpfs_open,
                           .mkdir = tmpfs_mkdir,
                           .link = tmpfs_link,
                           .mount = tmpfs_mount};
