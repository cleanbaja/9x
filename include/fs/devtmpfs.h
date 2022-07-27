#ifndef FS_DEVTMPFS_H
#define FS_DEVTMPFS_H

#include <fs/handle.h>

/* dev_t is layed out like this...
 * |--------------------------------|
 * |  Minor Device (lower 32-bits)  |
 * |--------------------------------|
 * |  Major Device (higher 32-bits) |
 * |--------------------------------|
 */

#define EXTRACT_DEVICE_MAJOR(dev) ((uint32_t)((uint64_t)dev >> 32))
#define EXTRACT_DEVICE_MINOR(dev) ((uint32_t)dev)
#define MKDEV(maj, min) (min | ((uint64_t)maj << 32))

// Path has to be relative to /dev, like "dri/card0"
struct vnode *devtmpfs_create_device(char *path, int size);
size_t devtmpfs_create_id(int subclass);

void setup_unix_streams();
void setup_random_streams();

#endif  // FS_DEVTMPFS_H
