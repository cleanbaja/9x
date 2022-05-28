#include <fs/vfs.h>
#include <lib/builtin.h>
#include <lib/cmdline.h>
#include <lib/kcon.h>
#include <vm/vm.h>

struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    uint8_t type;
    char link_name[100];
    char signature[6];
    char version[2];
    char owner[32];
    char group[32];
    char device_maj[8];
    char device_min[8];
    char prefix[155];
};

enum {
    USTAR_FILE = '0',
    USTAR_HARD_LINK = '1',
    USTAR_SYM_LINK = '2',
    USTAR_CHAR_DEV = '3',
    USTAR_BLOCK_DEV = '4',
    USTAR_DIRECTORY = '5',
    USTAR_FIFO = '6'
};

#define TAR_ALIGN(A, B) ({           \
    typeof(A) _a_ = A;               \
    typeof(B) _b_ = B;               \
    ((_a_ + (_b_ - 1)) / _b_) * _b_; \
})

static uint64_t parse_octal(const char* str) {
  uint64_t res = 0;

  while(*str) {
    res *= 8;
    res += *str - '0';
    str++;
  }

  return res;
}

void initramfs_populate(struct stivale2_struct_tag_modules* mods) {
  if (mods == NULL || mods->module_count < 1) {
    klog("vfs: initramfs missing, kernel might fail on real hardware!");
    return;
  }

  uint64_t initramfs_base = mods->modules[0].begin;
  uint64_t initramfs_size = mods->modules[0].end - mods->modules[0].begin;
  klog("initramfs: initrd found at 0x%lx with %u bytes", initramfs_base, initramfs_size);

  struct ustar_header* hdr = (struct ustar_header*)initramfs_base;
  struct backing* file = NULL;
  while (true) {
    if (memcmp(hdr->signature, "ustar", 5) != 0)
	break; // Invalid magic!
   
    // Create the file
    uint64_t filesize = parse_octal(hdr->size);
    switch (hdr->type) {
      case USTAR_DIRECTORY: {
        vfs_mkdir(root_node, hdr->name, parse_octal(hdr->mode));
	struct vfs_resolved_node result = vfs_resolve(NULL, hdr->name, 0);
	kfree(result.raw_string);
	file = result.target->backing;
	break;
      }
      case USTAR_FILE: {
	file = vfs_open(root_node, hdr->name, true, parse_octal(hdr->mode));
	void* buffer = (void*)((uintptr_t)hdr + 512);
	file->write(file, buffer, 0, filesize);
	file->close(file);
	break;
      }
    }

    // Update the stats of the file/dir
    file->st.st_uid = parse_octal(hdr->uid);
    file->st.st_gid = parse_octal(hdr->gid);
    file->st.st_size = parse_octal(hdr->size);
    file->st.st_mode = parse_octal(hdr->mode);

    // Move onto the next file
    hdr = (void*)((uint64_t)hdr + 512 + TAR_ALIGN(filesize, 512));
    if ((uintptr_t)hdr >= initramfs_base + initramfs_size)
	break; // Out of bounds!
  }
}

