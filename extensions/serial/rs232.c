#include <fs/devtmpfs.h>
#include <lib/kcon.h>
#include <lib/builtin.h>
#include <arch/asm.h>
#include <ninex/tty.h>

#include "serial_priv.h"

struct serial_vnode {
  struct vnode;
  struct tty* tty;
};

static uint8_t active_ports = 0;
static vec_t(struct tty*) tty_devices;
static uint16_t portmap[] = {
  0x3F8, 0x2F8,
  0x3E8, 0x2E8,
  0x5F8, 0x4F8,
  0x5E8, 0x4E8
};

static ssize_t serial_read(struct vnode* vn,
                             void* buf,
                             off_t offset,
                             size_t count) {
  return tty_read(((struct serial_vnode*)vn)->tty, buf, offset, count);
}

static ssize_t serial_write(struct vnode* vn,
                              const void* buf,
                              off_t offset,
                              size_t count) {
  return tty_write(((struct serial_vnode*)vn)->tty, buf, offset, count);
}

static ssize_t serial_resize(struct vnode* vn, off_t new_size) {
  return 0;
}

static void serial_close(struct vnode* vn) {
  spinlock(&vn->lock);
  vn->refcount--;
  spinrelease(&vn->lock);
}

static ssize_t serial_ioctl(struct vnode* v, int64_t req, void* argp)
{
  return tty_ioctl(((struct serial_vnode*)v)->tty, req, argp);
}

static void poll_thread(struct serial_vnode* v) {
  uint16_t port = portmap[EXTRACT_DEVICE_MINOR(v->st.st_dev) - 1];
  char data;

  for (;;) {
    while((asm_inb(port + 5) & 1) == 0);

    data = asm_inb(port);
    if (data == '\r')
      data = '\n';

    tty_insert(v->tty, &data, 1);
  }
}

static void serial_set_char(struct tty* tty, int x, int y, char c, uint32_t fg, uint32_t bg) {
  (void)x;
  (void)y;
  (void)fg;
  (void)bg;

  if (c == '\n')
    asm_outb(tty->fg_buf[0], '\r');
  asm_outb(tty->fg_buf[0], c);
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
    struct serial_vnode* dev = devtmpfs_create_device(dev_name, sizeof(struct serial_vnode));
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
    dev->ioctl  = serial_ioctl;
    dev->close  = serial_close;
    dev->tty    = create_tty(80, 25);
    dev->tty->is_serial = true;
    dev->tty->set_char  = serial_set_char;
    dev->tty->fg_buf[0] = base;

    // Start the worker thread, that polls for input
    thread_t* worker_thread = kthread_create((uintptr_t)poll_thread, dev);
    sched_queue(worker_thread);
  }

  int total_ports = 0;
  for (int i = 0; i < 8; i++)
    if (active_ports & (1 << i))
      total_ports++;

  klog("rs232: %d total ports!", total_ports);
}

