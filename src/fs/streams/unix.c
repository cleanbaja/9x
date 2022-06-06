#include <fs/devtmpfs.h>
#include <lib/builtin.h>

static ssize_t
zero_read(struct backing* bck, void* buf, off_t offset, size_t count)
{
  (void)offset;
  (void)bck;
  
  // Use 64-bit memcpy when possible
  if (count % 8) {
    memset64(buf, 0, count);
  } else {
    memset(buf, 0, count);
  }
  
  return count;
}

static ssize_t
null_read(struct backing* bck, void* buf, off_t offset, size_t count)
{
  (void)offset;
  (void)bck;
  (void)buf;
  (void)count;
  
  return 0;
}

static ssize_t
null_write(struct backing* bck, const void* buf, off_t offset, size_t count)
{
  (void)offset;
  (void)bck;
  (void)buf;
  (void)count;
  
  return count;
}

static ssize_t
null_resize(struct backing* bck, off_t new_size)
{
  // The null device can't be resized
  (void)bck;
  (void)new_size;
  return 0;
}

static void null_close(struct backing* bck) {
  spinlock_acquire(&bck->lock);
  bck->refcount--;
  spinlock_release(&bck->lock);
}

void setup_unix_streams()
{
  struct backing* zero_bck = devtmpfs_create_device("zero", 0);
  struct backing* null_bck = devtmpfs_create_device("null", 0);
      
  // Setup '/dev/zero'
  zero_bck->st.st_dev     = devtmpfs_create_id(0);
  zero_bck->st.st_mode    = 0666 | S_IFCHR;
  zero_bck->st.st_nlink   = 1;
  zero_bck->refcount      = 1;
  zero_bck->read   = zero_read;
  zero_bck->write  = null_write;
  zero_bck->resize = null_resize;
  zero_bck->close  = null_close;

  // Setup '/dev/null'
  null_bck->st.st_dev     = devtmpfs_create_id(0);
  null_bck->st.st_mode    = 0666 | S_IFCHR;
  null_bck->st.st_nlink   = 1;
  null_bck->refcount      = 1;
  null_bck->read   = null_read;
  null_bck->write  = null_write;
  null_bck->resize = null_resize;
  null_bck->close  = null_close;
}

