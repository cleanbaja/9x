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

static uint64_t parse_octal(const char* str) {
  uint64_t res = 0;

  while (*str) {
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
  klog("initramfs: initrd found at 0x%lx with %u bytes", initramfs_base,
       initramfs_size);

  struct ustar_header* hdr = (struct ustar_header*)initramfs_base;
  for (;;) {
    if (memcmp(hdr->signature, "ustar", 5) != 0)
      break;

    uintptr_t size = parse_octal(hdr->size);
    switch (hdr->type) {
      case USTAR_DIRECTORY: {
        vfs_mkdir(NULL, hdr->name, parse_octal(hdr->mode));
        break;
      }
      case USTAR_FILE: {
        struct vnode* r =
            vfs_open(NULL, hdr->name, true, parse_octal(hdr->mode));
        void* buf = (void*)hdr + 512;
        r->write(r, buf, 0, size);
        r->close(r);
        break;
      }
      case USTAR_SYM_LINK: {
        vfs_symlink(NULL, hdr->link_name, hdr->name);
        break;
      }
    }

    hdr = (void*)hdr + 512 + ALIGN_UP(size, 512);
    if ((uintptr_t)hdr >= initramfs_base + initramfs_size)
      break;
  }
}
