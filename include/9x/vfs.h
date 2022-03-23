#ifndef VFS_H
#define VFS_H

#include <fs/backing.h>
#include <lib/vec.h>
#include <internal/stivale2.h>

#define MAX_NAME_LEN 256

// A collection of API calls and decls that make up a filesystem
struct filesystem
{
  const char* name;
  bool needs_backing;

  struct vfs_node* (*populate)(struct vfs_node* node);
  struct backing* (*open)(struct vfs_node* node, bool new_node, mode_t mode);
  struct backing* (*mkdir)(struct vfs_node* node, mode_t mode);
  struct vfs_node* (*mount)(struct vfs_node*, const char*, struct vfs_node*);
};

// Repersents a virtual filesystem node
struct vfs_node
{
  char name[MAX_NAME_LEN];
  int64_t refcount;
  struct backing* backing;
  struct filesystem* fs;

  struct vfs_node* parent;
  struct vfs_node* mountpoint;
  struct vfs_node* redir;

  vec_t(struct vfs_node*) children;
};

void
vfs_register_filesystem(struct filesystem* fs);
void
vfs_init(struct stivale2_struct_tag_modules* md);
struct vfs_node*
create_node(const char* name,
            struct filesystem* filesystem,
            struct vfs_node* parent);
void
vfs_mkdir(struct vfs_node* parent, char* path, mode_t mode);
struct backing*
vfs_open(struct vfs_node* parent,
         const char* path,
         bool create,
         mode_t creat_mode);

// VFS root node, aka the parent of all nodes
extern struct vfs_node* root_node;

// Kernel provided filesystems
extern struct filesystem tmpfs;

#endif // VFS_H
