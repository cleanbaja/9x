#ifndef FS_VFS_H
#define FS_VFS_H

#include <fs/backing.h>
#include <lib/vec.h>
#include <lib/stivale2.h>

#define MAX_NAME_LEN 256

// A collection of API calls and decls that make up a filesystem
struct filesystem
{
  const char* name;
  bool needs_backing;

  struct vfs_node* (*populate)(struct vfs_node* node);
  struct backing*  (*open)(struct vfs_node* node, bool new_node, mode_t mode);
  struct backing*  (*mkdir)(struct vfs_node* node, mode_t mode);
  struct backing*  (*link)(struct vfs_node* node, mode_t mode);
  struct vfs_node* (*mount)(const char*, struct vfs_node*);
};

// Repersents a virtual filesystem node
struct vfs_node
{
  char    name[MAX_NAME_LEN];
  int64_t refcount;
  char*   symlink_target;
  struct  backing* backing;
  struct  filesystem* fs;

  struct vfs_node* parent;
  struct vfs_node* mountpoint;
  vec_t(struct vfs_node*) children;
};

// General routines
void vfs_register_filesystem(struct filesystem* fs);
void vfs_init(struct stivale2_struct_tag_modules* mods);
void vfs_mount(char* source, char* dest, char* fs);

// The heart of the VFS subsystem, the resolving function, which
// turns paths to nodes
#define RESOLVE_CREATE_SHALLOW (1 << 10)
#define RESOLVE_FAIL_IF_EXISTS (1 << 11)
struct vfs_resolved_node {
  struct vfs_node *parent, *target;
  char *basename, *raw_string;      // The caller has to clean up!
  bool success;
};
struct vfs_resolved_node vfs_resolve(struct vfs_node* root, char* path, int flags);
struct vfs_node* vfs_create_node(const char* basename, struct vfs_node* parent);

// VFS operations
void vfs_mkdir(struct vfs_node* parent, char* path, mode_t mode);
void vfs_symlink(struct vfs_node* root, char* target, char* source);
struct backing* vfs_open(struct vfs_node* root, char* path, bool create, mode_t creat_mode);

// VFS root node, aka the parent of all nodes
extern struct vfs_node* root_node;

// Kernel provided filesystems
extern struct filesystem tmpfs;
extern struct filesystem devtmpfs;

#endif // FS_VFS_H
