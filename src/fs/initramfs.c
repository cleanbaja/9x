#include <9x/vfs.h>
#include <9x/vm.h>
#include <lib/builtin.h>
#include <lib/cmdline.h>
#include <lib/log.h>

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

void initramfs_load(struct stivale2_struct_tag_modules* mods) {
  if (mods->module_count < 1) {
    PANIC(NULL, "initramfs module missing!\n");
  }

  uint64_t initramfs_base = mods->modules[0].begin;
  uint64_t initramfs_size = mods->modules[0].end - mods->modules[0].begin;
  log("initramfs: initrd found at 0x%lx with %u bytes", initramfs_base, initramfs_size);

  struct ustar_header* hdr = (struct ustar_header*)initramfs_base;
  while (true) {
    if (memcmp(hdr->signature, "ustar", 5) != 0)
	break; // Invalid magic!
   
    uint64_t filesize = parse_octal(hdr->size);
    switch (hdr->type) {
      case USTAR_DIRECTORY:
        vfs_mkdir(root_node, hdr->name, parse_octal(hdr->mode));
	break;
      case USTAR_FILE: {
	struct backing* bck = vfs_open(root_node, hdr->name, true, parse_octal(hdr->mode));
	void* buffer = (void*)((uintptr_t)hdr + 512);
	bck->write(bck, buffer, 0, filesize);
	break;
      }
    }

    hdr = (void*)((uint64_t)hdr + 512 + TAR_ALIGN(filesize, 512));
    if ((uintptr_t)hdr >= initramfs_base + initramfs_size)
	break; // Out of bounds!
  }

  // Read the cmdline, and print it
  struct backing* bc = vfs_open(NULL, "/boot/cmdline", false, 0);
  char* buffer = (char*)kmalloc(bc->st.st_size);
  bc->read(bc, buffer, 0, bc->st.st_size);
  cmdline_load(buffer);
}

