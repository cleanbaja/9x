#include <fs/vfs.h>
#include <fs/devtmpfs.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <vm/vm.h>

struct vfs_ent* root_node = NULL;
vec_t(struct filesystem*) fs_list;
static lock_t vfs_lock;
extern void initramfs_populate(struct stivale2_struct_tag_modules* mods);

void vfs_register_fs(struct filesystem* fs) {
  vec_push(&fs_list, fs);
}

static struct filesystem* find_fs(const char* name) {
  int i;
  struct filesystem* cur;
  vec_foreach(&fs_list, cur, i)
  {
    if (memcmp(cur->name, name, strlen(cur->name)) == 0) {
      return cur;
    }
  }

  return NULL;
}

struct vfs_ent* vfs_create_node(const char* basename, struct vfs_ent* parent) {
  struct vfs_ent* nd = kmalloc(sizeof(struct vfs_ent));
  memcpy(nd->name, basename, strlen(basename));
  nd->parent = parent;
  nd->fs = parent->fs;

  return nd;
}

static inline struct vfs_ent* simplify_node(struct vfs_ent* nd) {
  if (nd->mountpoint) {
    return nd->mountpoint;
  } else if (nd->symlink_target && S_ISLNK(nd->backing->st.st_mode)) {
    klog("vfs: (TODO) add support for symlinks!");
    return NULL;
  } else {
    return nd;
  }
}

static struct vfs_ent* search_relative(struct vfs_ent* parent, char* name, int flags) {
  if (!parent || !name)
    return NULL;

  struct vfs_ent* cur; int cnt;
  vec_foreach(&parent->children, cur, cnt) {
    if (memcmp(name, cur->name, strlen(name)) == 0)
      return simplify_node(cur);
  }

  // Check for . and ..
  if (strlen(name) > 1 && (memcmp(name, "..", 2) == 0)) {
    return parent->parent;
  } else if (strlen(name) == 1 && (memcmp(name, ".", 1) == 0)) {
    return parent;
  } else {
    return NULL;
  }
}

struct vfs_resolved_node vfs_resolve(struct vfs_ent* root, char* path, int flags) {
  struct vfs_resolved_node result = {0};
  char* token = NULL;
  spinlock(&vfs_lock);

  // Setup the enviorment, so that we may parse the path
  if (root == NULL)
    result.parent = simplify_node(root_node);
  else
    result.parent = simplify_node(root);
  result.target = result.parent;

  // Make sure the path is sane
  if (strlen(path) == 1) {
    if (*path == '/') {
      result.target   = simplify_node(root_node);
      result.basename = "/";
      result.success  = true;
    }

    spinrelease(&vfs_lock);
    return result;
  } else if (path == NULL) {
    klog("vfs: NULL path was passed to vfs_resolve()!");
  }
  char* real_str = strdup(path);
  char* context = real_str;
  token = strtok_r(context, "/", &context);

  // Use strtok to tokenize the path, and decend the node tree
  while (token) {
    result.basename = token;
    result.parent = result.target;
    result.target = search_relative(result.parent, token, flags);

    token = strtok_r(context, "/", &context);
    if (!token && !result.target && (flags & RESOLVE_CREATE_SHALLOW)) {
      // Create the final node, if needed
      result.target = vfs_create_node(result.basename, result.parent);
      vec_push(&result.parent->children, result.target);
      flags &= ~(RESOLVE_FAIL_IF_EXISTS);
    } else if (!result.target) {
      result.success = false;
      spinrelease(&vfs_lock);
      return result;
    }
  }

  if ((flags & RESOLVE_FAIL_IF_EXISTS) && result.target) {
    result.success = false;
  } else {
    result.success = true;
  }

  spinrelease(&vfs_lock);
  return result;
}

void vfs_mount(char* source, char* dest, char* fs) {
  struct filesystem* filesystem = find_fs(fs);
  if (!filesystem) {
    klog("vfs: unknown filesystem '%s'", fs);
    return;
  }

  // We don't support physical filesystems at the moment...
  if (filesystem->needs_backing) {
    klog("vfs: (TODO) support filesystems that need physical backings!");
    return;
  }

  struct vfs_resolved_node data = vfs_resolve(NULL, dest, 0);
  if (!data.success) {
    klog("vfs: non-existent mount destination '%s'!", dest);
    return;
  } else if (!S_ISDIR(data.target->backing->st.st_mode)) {
    klog("vfs: '%s' is not a valid mountpoint!", dest);
    return;
  } else {
    data.target->mountpoint = filesystem->mount(data.basename, data.parent);
    struct vfs_ent* new_point = data.target->mountpoint;
    new_point->backing = filesystem->mkdir(new_point, 0667);
    kfree(data.raw_string);

    if (filesystem->needs_backing) {
      klog("vfs: Mounted '%s' to '%s' using filesystem '%s'", source, dest, filesystem->name);
    } else {
      klog("vfs: Mounted '%s' to '%s'", filesystem->name, dest);
    }
  }
}

char* vfs_get_path(struct vfs_ent* node) {
  vec_t(struct vfs_ent*) nodes = { 0 };
  char *result = kmalloc(512); // TODO: allocate dynamically

  while(node) {
    vec_push(&nodes, node);
    node = node->parent;
  }

  for(size_t i = nodes.length; i-- > 0;) {
    if(S_ISDIR(nodes.data[i]->backing->st.st_mode)) {
      snprintf(result + strlen(result), 256, "%s/", nodes.data[i]->name);
    } else {
      snprintf(result + strlen(result), 256, "%s", nodes.data[i]->name);
    }
  }

  vec_deinit(&nodes);
  return ++result;
}

void vfs_mkdir(struct vfs_ent* parent, char* path, mode_t mode) {
  struct vfs_resolved_node res = vfs_resolve(parent, path, 0);
  if (res.success)
    return; // Directory already exists
  else if (res.basename == NULL || res.parent == NULL)
    return; // Mysterious error :-(

  // Create the directory and insert it into the node space
  struct vfs_ent* new_dir = vfs_create_node(res.basename, res.parent);
  new_dir->backing = new_dir->fs->mkdir(new_dir, mode);
  vec_push(&res.parent->children, new_dir);
  kfree(res.raw_string);
}

void vfs_symlink(struct vfs_ent* root, char* target, char* source) {
  int resolve_flags = RESOLVE_CREATE_SHALLOW | RESOLVE_FAIL_IF_EXISTS;
  struct vfs_resolved_node res = vfs_resolve(root, target, resolve_flags);
  if (!res.success)
    return;

  memcpy(res.target->name, res.basename, strlen(res.basename));
  res.target->symlink_target = strdup(source);
  res.target->backing = res.target->fs->link(res.target, 0777);
}

struct vnode* vfs_open(struct vfs_ent* root, char* path, bool create, mode_t creat_mode) {
  // TODO: Return EEXISTS if (creat_mode == (O_CREAT | O_EXCEL))
  struct vfs_resolved_node res = vfs_resolve(root, path, ((create) ? RESOLVE_CREATE_SHALLOW : 0));
  if (!res.success)
    return NULL;

  if (res.target->backing == NULL && create)
    res.target->backing = res.parent->fs->open(res.target, true, creat_mode);

  res.target->refcount++;
  return res.target->backing;
}

// A simple funciton I use for debugging the VFS tree...
static void dump_all_nodes(struct vfs_ent* node, int depth) {
  if (node->mountpoint) {
    return dump_all_nodes(node->mountpoint, depth);
  } else if (node->symlink_target) {
    klog("%*s%s -| (link)", depth, "", node->name);
    return;
  } else {
    klog("%*s%s -|", depth, "", node->name);
  }

  if (S_ISDIR(node->backing->st.st_mode)) {
    struct vfs_ent* cur; int cnt;
    vec_foreach(&node->children, cur, cnt) {
      dump_all_nodes(cur, depth+1);
    }
    return;
  }
}


void vfs_setup() {
  // Create the root node...
  root_node = kmalloc(sizeof(struct vfs_ent));
  memcpy(root_node->name, "/", 1);
  root_node->parent = root_node;
  root_node->backing = create_resource(0);
  root_node->backing->st.st_mode |= S_IFDIR;

  // Mount tmpfs to the VFS root
  vfs_register_fs(&tmpfs);
  vfs_mount(NULL, "/", "tmpfs");

  // Then mount the devfs to /dev
  vfs_register_fs(&devtmpfs);
  vfs_mkdir(NULL, "/dev", 0775);
  vfs_mount(NULL, "/dev", "devtmpfs");

  // Populate the tmpfs (via the initramfs) and the devtmpfs
  struct stivale2_struct_tag_modules* mods =
      stivale2_find_tag(STIVALE2_STRUCT_TAG_MODULES_ID);
  initramfs_populate(mods);
  setup_unix_streams();
  setup_random_streams();
}
