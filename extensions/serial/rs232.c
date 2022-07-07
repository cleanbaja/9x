#include <fs/devtmpfs.h>
#include <lib/kcon.h>
#include <lib/builtin.h>
#include <arch/asm.h>

#include "serial_priv.h"

static uint8_t active_ports = 0;
static uint16_t portmap[] = {
  0x3F8, 0x2F8,
  0x3E8, 0x2E8,
  0x5F8, 0x4F8,
  0x5E8, 0x4E8
};

static ssize_t serial_read(struct backing* bck,
                             void* buf,
                             off_t offset,
                             size_t count) {
  spinlock(&bck->lock);

  uint16_t port = portmap[EXTRACT_DEVICE_MINOR(bck->st.st_dev) - 1];
  for (int i = 0; i < count; i++) {
    while((asm_inb(port + 5) & 1) == 0);

    // TODO: Find a more efficent way, rather
    // than waiting for every character
    ((char*)buf)[i] = asm_inb(port);
  }

  spinrelease(&bck->lock);
  return count;
}

static ssize_t serial_write(struct backing* bck,
                              const void* buf,
                              off_t offset,
                              size_t count) {
  spinlock(&bck->lock);

  uint16_t port = portmap[EXTRACT_DEVICE_MINOR(bck->st.st_dev) - 1];
  for (int i = 0; i < count; i++) {
    while((asm_inb(port + 5) & 0x20) == 0);

    // TODO: Find a more efficent way, rather
    // than waiting for every character
    if (((char*)buf)[i] == '\n') {
      asm_outb(port, '\r');
      asm_outb(port, '\n');
    } else {
      asm_outb(port, ((char*)buf)[i]);
    }
  }

  spinrelease(&bck->lock);
  return count;
}

static ssize_t serial_resize(struct backing* bck, off_t new_size) {
  return 0;
}

static void serial_close(struct backing* bck) {
  spinlock(&bck->lock);
  bck->refcount--;
  spinrelease(&bck->lock);
}

void rs232_init() {
  // Loop through COM1-8, setting up the various ports...
  for (int i = 0; i < 8; i++) {
    uint16_t base = portmap[i];

    asm_outb(base + 1, 0x00);    // Disable all interrupts
    asm_outb(base + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    asm_outb(base, 0x01);        // Set divisor to 1 (lo byte) 115200 baud
    asm_outb(base + 1, 0x00);    //                  (hi byte)
    asm_outb(base + 3, 0x03);    // 8 bits, no parity, one stop bit
    asm_outb(base + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    asm_outb(base + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    asm_outb(base + 4, 0x1E);    // Set in loopback mode, test the serial chip
    asm_outb(base, 0xFA);        // Test serial chip (send byte 0xFA and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if(asm_inb(base) != 0xFA) {
      continue;
    }

    // Enable the port (IRQs disabled, OUT1/OUT2 disabled as well) and mark as active
    asm_outb(base + 4, 0x0);
    active_ports |= (1 << i);

    char dev_name[25];
    snprintf(dev_name, 25, "ttyS%d", i);

    // Create the respective resource
    struct backing* dev = devtmpfs_create_device(dev_name, 0);
    dev->st.st_dev = MKDEV(SERIAL_DEV_CLASS, i+1);
    dev->st.st_size = 0;
    dev->st.st_blocks = 0;
    dev->st.st_blksize = 512;
    dev->st.st_ino = 1;
    dev->st.st_mode = (0660 & ~S_IFMT) | S_IFCHR;
    dev->st.st_nlink = 1;
    dev->refcount = 1;
    dev->read   = serial_read;
    dev->write  = serial_write;
    dev->resize = serial_resize;
    dev->close  = serial_close;

    // Finally, print a small greeting...
    char greeting[128];
    memset(greeting, 0, 128);
    snprintf(greeting, 128, "\n9x Kernel on /dev/%s\n\n", dev_name);
    dev->write(dev, greeting, 0, 128);
  }


  int total_ports = 0;
  for (int i = 0; i < 8; i++)
    if (active_ports & (1 << i))
      total_ports++;

  klog("rs232: %d total ports!", total_ports);
}

