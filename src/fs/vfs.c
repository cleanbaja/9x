#include <fs/vfs.h>
#include <lib/builtin.h>
#include <lib/kcon.h>
#include <vm/vm.h>

struct vfs_node* root_node = NULL;
vec_t(struct filesystem*) fs_list;
extern void initramfs_load(struct stivale2_struct_tag_modules* mods);

void
vfs_register_fs(struct filesystem* fs)
{
  vec_push(&fs_list, fs);
}

static struct filesystem*
find_fs(const char* name)
{
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

struct vfs_node*
create_node(const char* name,
            struct filesystem* filesystem,
            struct vfs_node* parent)
{
  struct vfs_node* vnode = (struct vfs_node*)kmalloc(sizeof(struct vfs_node));
  vnode->fs = filesystem;
  vnode->parent = parent;
  memcpy(vnode->name, name, (strlen(name) >= 256) ? 255 : strlen(name));

  return vnode;
}

static struct vfs_node*
search_for_node(struct vfs_node* dir, char* name, bool should_be_file)
{
  if (dir == NULL || name == NULL || dir->children.length == 0)
    return NULL;

  int i;
  struct vfs_node* cur;
  vec_foreach(&dir->children, cur, i)
  {
    if ((memcmp(cur->name, name, strlen(name)) == 0) &&
        (strlen(name) == strlen(cur->name))) {
      // TODO: Add and implment filetype checks
      if (cur->mountpoint != NULL)
        return cur->mountpoint;
      else if (cur->redir != NULL)
        return cur->redir;
      else
        return cur;
    }
  }

  return NULL;
}

#define RESOLVE_CREATE_NODE 0x10

static struct vfs_node*
resolve_path_ex(struct vfs_node* parent,
                struct vfs_node** real_parent,
                char* path,
                char** basename,
                int flags)
{
  struct vfs_node* cur_node = parent;
  struct vfs_node* old_node = NULL;
  char* basename_ret = NULL;
  char* testbuf = (char*)kmalloc(strlen(path));

  // Non-Relative paths aren't supported, nor are NULL parents...
  if (parent == NULL) {
    klog("vfs: resolve_path() doesn't know where to start!");
    return NULL;
  }

  // See if the root is getting requested
  if (path == NULL || (*path == '/' && path[1] == '\0')) {
    if (basename != NULL)
      *basename = "/";

    if (real_parent != NULL)
      *real_parent = root_node;

    return root_node;
  }

  // Finally, get rid of trailing slashes and uneeded characters
  if (*path == '/' || *path == '~')
    path++;
  if (path[strlen(path) - 1] == '/')
    path[strlen(path) - 1] = '\0';

  while (true) {
    size_t i;
    bool final = false;
    memset(testbuf, 0, strlen(path));
    for (i = 0; *path != '/'; path++) {
      if (*path == '\0') {
        final = true;
        break;
      }

      testbuf[i++] = *path;
    }

    testbuf[i] = '\0';
    path++;
    
 
    if (final) {
      if (strlen(testbuf) != 0) {
        old_node = cur_node;
        cur_node = search_for_node(cur_node, testbuf, true);
        basename_ret = testbuf;

        if (cur_node == NULL && flags & RESOLVE_CREATE_NODE) {
          cur_node = create_node(testbuf, old_node->fs, old_node);
          vec_push(&old_node->children, cur_node);
        }
        goto out;
      }
    } else {
      old_node = cur_node;
      cur_node = search_for_node(cur_node, testbuf, false);
      basename_ret = testbuf;
      if (cur_node == NULL)
        goto out;
    }
  }

out:
  // Return the basename, if wanted...
  if (basename_ret != NULL && basename != NULL) {
    char* new_basename = (char*)kmalloc(strlen(basename_ret) + 1);
    memcpy(new_basename, basename_ret, strlen(basename_ret) + 1);
    *basename = basename_ret;
  }

  // Return the parent of the final node, if wanted...
  if (real_parent != NULL && (old_node != NULL || cur_node == root_node)) {
    if (cur_node == root_node)
      *real_parent = root_node;
    else
      *real_parent = old_node;
  }

  kfree(testbuf);
  return cur_node;
}

// A simpler version, for those who don't care about the extra information...
static struct vfs_node*
resolve_path(struct vfs_node* parent, char* path, int flags)
{
  return resolve_path_ex(parent, NULL, path, NULL, flags);
}

static void
create_dotentries(struct vfs_node* nd, struct vfs_node* parent)
{
  struct vfs_node* dot = create_node(".", nd->fs, nd);
  struct vfs_node* dotdot = create_node("..", nd->fs, nd);

  // Set the redir entries and add the nodes
  dot->redir = nd;
  dotdot->redir = parent;
  vec_push(&nd->children, dot);
  vec_push(&nd->children, dotdot);
}

void
vfs_mkdir(struct vfs_node* parent, char* path, mode_t mode)
{
  char* basename; struct vfs_node* real_parent;
  struct vfs_node* old_node = resolve_path_ex(parent, &real_parent, path, &basename, 0);
  if (old_node != NULL || basename == NULL) {
    return; // Directory already exists!
  }

  struct vfs_node* new_dir = create_node(basename, parent->fs, real_parent);
  create_dotentries(new_dir, real_parent);
  new_dir->backing =
    parent->fs->mkdir(new_dir, mode); // TODO: Add proper permissions
  vec_push(&real_parent->children, new_dir);
  kfree(basename);
}

struct backing*
vfs_open(struct vfs_node* parent,
         const char* path,
         bool create,
         mode_t creat_mode)
{
  if (path == NULL)
    return NULL;

  if (parent == NULL || *path == '/') {
    parent = root_node;
    path++;
  }

  struct vfs_node* real_parent = NULL;
  struct vfs_node* nd =
    resolve_path_ex(parent, &real_parent, (char*)path, NULL, (create) ? RESOLVE_CREATE_NODE : 0);
  if (nd == NULL || real_parent == NULL)
    return NULL;

  if (nd->backing == NULL) {
    nd->backing = nd->fs->open(nd, create, creat_mode);
  } else if (nd->backing != NULL && create)
    create = false;

  // Refcount is already set to one by the FS driver, so don't increment it
  if (create)
    vec_push(&real_parent->children, nd);

  return nd->backing;
}

bool
vfs_mount(struct vfs_node* parent, char* source, char* target, char* filesystem)
{
  struct filesystem* fs = find_fs(filesystem);
  if (fs == NULL)
    return false;

  struct vfs_node* source_node = NULL;
  if (fs->needs_backing) {
    source_node = resolve_path(parent, source, 0);
    if (source_node == NULL || S_ISDIR(source_node->backing->st.st_mode)) {
      return false;
    }
  }

  char* basename = NULL;
  bool mounting_root = false;
  struct vfs_node* parent_of_target = NULL;
  struct vfs_node* target_node =
    resolve_path_ex(parent, &parent_of_target, target, &basename, 0);

  // Run some complex checks on the target, to make sure its suitable for
  // mounting
  if (target_node != NULL && target_node != root_node) {
    if (!S_ISDIR(target_node->backing->st.st_mode)) {
      return false;
    }
  } else {
    if (target_node == root_node) {
      mounting_root = true;
    } else {
      return false;
    }
  }

  struct vfs_node* mount_node =
    fs->mount(source_node, basename, parent_of_target);
  if (!mounting_root) {
    target_node->mountpoint = mount_node;
    create_dotentries(mount_node, parent_of_target);
  } else {
    // Prepare the mount node for its role as root_node
    create_dotentries(mount_node, mount_node);
    mount_node->parent = mount_node;
    kfree(root_node);
    root_node = mount_node;
    *root_node->name = '/';
  }

  if (source_node == NULL) {
    klog("vfs: mounted a %s instance to `%s`", filesystem, target);
  } else {
    klog("vfs: mounted `%s` to `%s` (type: %s)", source, target, filesystem);
  }
}

void
vfs_init(struct stivale2_struct_tag_modules* md)
{
  // Create the root node
  root_node = create_node("/", NULL, NULL);
  create_dotentries(root_node, NULL);

  // Register and mount the tmpfs
  vfs_register_fs(&tmpfs);
  vfs_mount(root_node, NULL, "/", "tmpfs");

  // Load the initramfs
  initramfs_load(md);
}

