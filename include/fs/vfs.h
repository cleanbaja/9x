#ifndef FS_VFS_H
#define FS_VFS_H

#include <lib/stivale2.h>
#include <lib/vec.h>
#include <fs/handle.h>
#include <stdbool.h>

#define MAX_NAME_LEN 256

// A collection of API calls and decls that make up a filesystem
struct filesystem
{
  const char* name;
  bool needs_backing;

  struct vfs_ent*  (*populate)(struct vfs_ent* node);
  struct vnode*  (*open)(struct vfs_ent* node, bool new_node, mode_t mode);
  struct vnode*  (*mkdir)(struct vfs_ent* node, mode_t mode);
  struct vnode*  (*link)(struct vfs_ent* node, mode_t mode);
  struct vfs_ent*  (*mount)(const char*, struct vfs_ent*);
};

// Repersents a virtual filesystem node
struct vfs_ent
{
  char    name[MAX_NAME_LEN];
  int64_t refcount;
  char*   symlink_target;
  struct  vnode* backing;
  struct  filesystem* fs;

  struct vfs_ent* parent;
  struct vfs_ent* mountpoint;
  vec_t(struct vfs_ent*) children;
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
  struct vfs_ent *parent, *target;
  char *basename, *raw_string;      // The caller has to clean up!
  bool success;
};
struct vfs_resolved_node vfs_resolve(struct vfs_ent* root, char* path, int flags);
struct vfs_ent* vfs_create_node(const char* basename, struct vfs_ent* parent);

// VFS operations
void vfs_mkdir(struct vfs_ent* parent, char* path, mode_t mode);
void vfs_symlink(struct vfs_ent* root, char* target, char* source);
struct vnode* vfs_open(struct vfs_ent* root, char* path, bool create, mode_t creat_mode);

// VFS root node, aka the parent of all nodes
extern struct vfs_ent* root_node;

// Kernel provided filesystems
extern struct filesystem tmpfs;
extern struct filesystem devtmpfs;

// Kernel VFS initalization
void vfs_setup();

#endif // FS_VFS_H
