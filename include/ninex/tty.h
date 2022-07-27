#ifndef NINEX_TTY_H
#define NINEX_TTY_H

#include <lib/lock.h>
#include <lib/types.h>
#include <lib/vec.h>
#include <ninex/condvar.h>
#include <stdbool.h>

// types used in 'struct termios`
typedef unsigned int cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

// bitflags for c_iflag
#define BRKINT 0x0001
#define ICRNL 0x0002
#define IGNBRK 0x0004
#define IGNCR 0x0008
#define IGNPAR 0x0010
#define INLCR 0x0020
#define INPCK 0x0040
#define ISTRIP 0x0080
#define IXANY 0x0100
#define IXOFF 0x0200
#define IXON 0x0400
#define PARMRK 0x0800

// bitflags for c_oflag
#define OPOST 0x0001
#define ONLCR 0x0002
#define OCRNL 0x0004
#define ONOCR 0x0008
#define ONLRET 0x0010
#define OFDEL 0x0020
#define OFILL 0x0040

#define NLDLY 0x0080
#define NL0 0x0000
#define NL1 0x0080

#define CRDLY 0x0300
#define CR0 0x0000
#define CR1 0x0100
#define CR2 0x0200
#define CR3 0x0300

#define TABDLY 0x0C00
#define TAB0 0x0000
#define TAB1 0x0400
#define TAB2 0x0800
#define TAB3 0x0C00

#define BSDLY 0x1000
#define BS0 0x0000
#define BS1 0x1000

#define VTDLY 0x2000
#define VT0 0x0000
#define VT1 0x2000

#define FFDLY 0x4000
#define FF0 0x0000
#define FF1 0x4000

// constants for transforming baudrate to 'struct termios' format
#define B0 0
#define B50 1
#define B75 2
#define B110 3
#define B134 4
#define B150 5
#define B200 6
#define B300 7
#define B600 8
#define B1200 9
#define B1800 10
#define B2400 11
#define B4800 12
#define B9600 13
#define B19200 14
#define B38400 15

// bitflags for c_cflag
#define CSIZE 0x0003
#define CS5 0x0000
#define CS6 0x0001
#define CS7 0x0002
#define CS8 0x0003

#define CSTOPB 0x0004
#define CREAD 0x0008
#define PARENB 0x0010
#define PARODD 0x0020
#define HUPCL 0x0040
#define CLOCAL 0x0080

// bitflags for c_lflag
#define ECHO 0x0001
#define ECHOE 0x0002
#define ECHOK 0x0004
#define ECHONL 0x0008
#define ICANON 0x0010
#define IEXTEN 0x0020
#define ISIG 0x0040
#define NOFLSH 0x0080
#define TOSTOP 0x0100

// tcsetattr() constants
#define TCSANOW 1
#define TCSADRAIN 2
#define TCSAFLUSH 3

// tcflush() constants
#define TCIFLUSH 1
#define TCIOFLUSH 2
#define TCOFLUSH 3

// tcflow() constants
#define TCIOFF 1
#define TCION 2
#define TCOOFF 3
#define TCOON 4

// indices into the c_cc array
#define VEOF 0
#define VEOL 1
#define VERASE 2
#define VINTR 3
#define VKILL 4
#define VMIN 5
#define VQUIT 6
#define VSTART 7
#define VSTOP 8
#define VSUSP 9
#define VTIME 10
#define NCCS 11

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_cc[NCCS];
  speed_t ibaud;
  speed_t obaud;
};

enum tcolor {
  TTY_COLOR_RED,
  TTY_COLOR_GREEN,
  TTY_COLOR_BLUE,
  TTY_COLOR_YELLOW,
  TTY_COLOR_MAGENTA,
  TTY_COLOR_CYAN,
  TTY_COLOR_BLACK,
  TTY_COLOR_WHITE
};

#define TTY_STATE_NORMAL 0x2
#define TTY_STATE_CSI 0x4
#define TTY_STATE_ESCAPE 0x8
struct tty {
  struct termios tio;
  vec_t(int) params;
  int cur_x, cur_y;
  int width, height, state;
  enum tcolor fg_color, bg_color;
  uint32_t *fg_buf, *bg_buf;
  bool is_serial;

  // Input structures
  char *in_buf, *line_buf;
  size_t in_idx, line_idx;
  cv_t input_event;
  lock_t input_lock;
  bool tcioff;

  // Output structures
  char *out_buf;
  lock_t output_lock;
  bool tcooff;

  void (*set_cursor)(int, int);
  void (*set_char)(struct tty *, int, int, char, uint32_t, uint32_t);
};

struct tty *create_tty(int width, int height);
ssize_t tty_read(struct tty *t, void *buffer, off_t offset, size_t len);
ssize_t tty_write(struct tty *t, const void *buffer, off_t offset, size_t len);
void tty_insert(struct tty *t, const char *data, size_t len);
int tty_ioctl(struct tty *t, int64_t req, void *argp);

#endif  // NINEX_TTY_H
