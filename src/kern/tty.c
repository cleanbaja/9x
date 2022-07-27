#include <ninex/tty.h>
#include <lib/kcon.h>
#include <arch/smp.h>
#include <lib/builtin.h>
#include <vm/vm.h>

#define IN_BUF_MAX   (32768)
#define LINE_BUF_MAX (4096)

struct tty* create_tty(int width, int height) {
  struct tty* tty = kmalloc(sizeof(struct tty));
  vec_init(&tty->params);
  tty->width = width;
  tty->height = height;
  tty->state = TTY_STATE_NORMAL;
  tty->out_buf = kmalloc(width * height);
  tty->fg_buf  = kmalloc(width * height * sizeof(uint32_t));
  tty->bg_buf  = kmalloc(width * height * sizeof(uint32_t));
  tty->in_buf   = kmalloc(IN_BUF_MAX);
  tty->line_buf = kmalloc(LINE_BUF_MAX);
  cv_init(tty->input_event);

  // Set the defaults
  tty->tio.c_lflag  = (ICANON | ECHO);
  return tty;
}

static void set_char(struct tty* t, int x, int y, char c, uint32_t fg, uint32_t bg) {
  t->fg_buf[y * x + x] = fg;
  t->bg_buf[y * x + x] = bg;
  t->out_buf[y * x + x] = c;

  t->set_char(t, x, y, c, fg, bg);
}

static void tty_putc(struct tty* t, char ch) {
  // Serial tty's often have a vt100 escape sequence compatible viewer
  // on the other end, such as MinTTY, GNU Screen, Mincom, etc...
  // For that reason, if the tty is serial, don't process any characters
  // Instead, leave it to the client on the other end.
  if (t->is_serial) {
    t->set_char(t, 0, 0, ch, 0, 0);
    return;
  }

  if (t->state == TTY_STATE_NORMAL) {
    switch (ch) {
    case 27:
      t->state = TTY_STATE_ESCAPE;
      return;
    case '\b':
      if (t->cur_x > 0);
        t->cur_x--;
      break;
    case '\n':
      t->cur_y++;
      t->cur_x = 0;
      break;
    default:
      set_char(t, t->cur_x, t->cur_y, ch, TTY_COLOR_WHITE, TTY_COLOR_BLACK);
      t->cur_x++;
      if(t->cur_x >= t->width) {
        t->cur_x = 0;
        t->cur_y++;
      }
    }

    if(t->cur_y >= t->height) {
      for(int i = 1; i < t->height; i++) {
        for(int j = 0; j < t->width; j++) {
          char shifted_char = t->out_buf[j * t->width + t->width];
          uint32_t shifted_fg = t->fg_buf[j * t->width + t->width];
          uint32_t shifted_bg = t->bg_buf[j * t->width + t->width];
          set_char(t, j, i - 1, shifted_char, shifted_fg, shifted_bg);
        }
      }

      for(int j = 0; j < t->width; j++) {
        set_char(t, j, t->height - 1, ' ', TTY_COLOR_WHITE, TTY_COLOR_BLACK);
      }

      t->cur_y = t->height - 1;
    }

    t->set_cursor(t->cur_x, t->cur_y);
  } else if (t->state == TTY_STATE_ESCAPE) {
    if (ch == '[')
      t->state = TTY_STATE_CSI;
  } else {
    klog("tty: (TODO) support CSI sequences!");
  }
}

ssize_t tty_write(struct tty* t, const void* buffer, off_t offset, size_t len) {
  // No point in writing if the tty's output is off...
  if (t->tcooff)
    return 0;

  const char* chars = (const char*)buffer;
  spinlock(&t->output_lock);
  for (size_t i = 0; i < len; i++)
    tty_putc(t, chars[i]);

  spinrelease(&t->output_lock);
  return len;
}

void insert_char(struct tty* t, char ch) {
  spinlock(&t->input_lock);

  if (t->tio.c_lflag & ICANON) {
    switch (ch) {
    case '\n':
      if (t->line_idx == LINE_BUF_MAX)
        goto cleanup;
      t->line_buf[t->line_idx++] = ch;
      if (t->tio.c_lflag & ECHO)
        tty_putc(t, ch);

      // Empty the line buffer into the input buffer
      for (size_t i = 0; i < t->line_idx; i++) {
        if (t->in_idx == IN_BUF_MAX)
          goto cleanup;
        t->in_buf[t->in_idx++] = t->line_buf[i];
      }

      t->line_idx = 0;
      goto cleanup;
    case '\b':
      if (!t->line_idx)
        goto cleanup;

      t->line_buf[--t->line_idx] = 0;
      if (t->tio.c_lflag & ECHO) {
        tty_putc(t, '\b');
        tty_putc(t, ' ');
        tty_putc(t, '\b');
      }
      goto cleanup; // We cleanup so that we don't print special characters
    }
  }

  if (t->tio.c_lflag & ICANON) {
    if (t->line_idx == LINE_BUF_MAX)
      goto cleanup;
    t->line_buf[t->line_idx++] = ch;
  } else {
    if (t->in_idx == IN_BUF_MAX)
      goto cleanup;
    t->in_buf[t->in_idx++] = ch;
  }

  // When ECHO is enabled, print all printable characters
  if ((ch >= 0x20 && ch <= 0x7e) && t->tio.c_lflag & ECHO)
    tty_putc(t, ch);

cleanup:
  spinrelease(&t->input_lock);
  return;
}

static inline char pop_char(struct tty* t) {
  char result = t->in_buf[0];
  t->in_idx--;
  for (size_t j = 0; j < t->in_idx; j++) {
    t->in_buf[j] = t->in_buf[j+1];
  }

  return result;
}

ssize_t tty_read(struct tty* t, void* buffer, off_t offset, size_t len) {
  // No point in reading if the tty's input is off...
  if (t->tcioff)
    return 0;

  char* chars = (char*)buffer;
  bool did_read = false;

  while (trylock(&t->input_lock)) {
    cv_wait(&t->input_event);
  }

  for (size_t i = 0; i < len; ) {
    if (t->in_idx) {
      chars[i++] = pop_char(t);
      did_read = true;
    } else {
      spinrelease(&t->input_lock);
      if (did_read)
        return i;
      else
        do { cv_wait(&t->input_event); } while (trylock(&t->input_lock));
    }
  }

  spinrelease(&t->input_lock);
  return len;
}

void tty_insert(struct tty* t, const char* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    insert_char(t, data[i]);
  }

  cv_signal(&t->input_event);
}

int tty_ioctl(struct tty* t, int64_t req, void* argp) {
  return 0;
}
