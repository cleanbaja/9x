#include <ninex/console.h>
#include <misc/limine.h>
#include <lvm/lvm.h>
#include <lib/libc.h>

struct charpoint {
    char c;
    uint32_t fg, bg;
};

struct queue_item {
    struct charpoint ch;
    size_t x, y;
};

struct video_info {
  size_t pitch, bpp, n_bytes;
  size_t width, height;
};

struct console {
  volatile uint32_t* buffer;
  struct video_info info;

  struct queue_item *queue;
  struct queue_item **map;
  struct charpoint *grid;
  uint32_t *bg_canvas;

  uint32_t theme[2];
  size_t x_pos, y_pos, old_x, old_y;
  size_t x_off, y_off, queue_pos;
  size_t rows, cols;
};

volatile static struct limine_framebuffer_request fb_req = {
  .id = LIMINE_FRAMEBUFFER_REQUEST,
  .revision = 0
};

static struct console *g_console;
static psf2_font_t* fnt;
IMPORT_FONT(sun12x22);

/////////////////////////////
//    Rendering Helpers    //
/////////////////////////////
static inline void write_pixel(struct console* c, size_t x, size_t y, uint32_t color) {
  c->buffer[x + (c->info.pitch / sizeof(uint32_t)) * y] = color;
  asm volatile ("" ::: "memory");
}

static inline uint32_t rgb(int red, int green, int blue) {
  struct limine_framebuffer* fb = fb_req.response->framebuffers[0];

  size_t r_mask = (1 << fb->red_mask_size) - 1;
  size_t g_mask = (1 << fb->green_mask_size) - 1;
  size_t b_mask = (1 << fb->blue_mask_size) - 1;

  uint32_t pixel = 0;
  pixel |= (red & r_mask) << fb->red_mask_shift;
  pixel |= (green & g_mask) << fb->green_mask_shift;
  pixel |= (blue & b_mask) << fb->blue_mask_shift;

  return pixel;
}

static void write_char(struct console *ct, size_t x, size_t y, struct charpoint ch) {
  size_t rx = ct->x_off + x * fnt->width;
  size_t ry = ct->y_off + y * fnt->height;
  size_t orig_x = rx;

  uint8_t* glyph = (uint8_t*)((uintptr_t)fnt + fnt->headersize + (ch.c * fnt->glyph_size));

  for (size_t i = 0; i < fnt->height; i++) {
    for (size_t j = 0; j < fnt->width; j++) {
      uint8_t bit = glyph[i * (fnt->glyph_size / fnt->height) + j / 8] >> (7 - j % 8);
      write_pixel(ct, rx++, ry + i, (bit & 1) ? ch.fg : ch.bg);
    }

    rx = orig_x;
  }
}

/////////////////////////////
//     Character Queue     //
/////////////////////////////
static void flush_queue(struct console *cons) {
  // Draw the cursor (if needed)
  int cursor_idx = cons->x_pos + cons->y_pos * cons->cols;
  struct charpoint ch;
  if (cons->map[cursor_idx] != NULL)
    ch = cons->map[cursor_idx]->ch;
  else
    ch = cons->grid[cursor_idx];

  uint32_t tmp = ch.bg;
  ch.bg = ch.fg;
  ch.fg = tmp;
  write_char(cons, cons->x_pos, cons->y_pos, ch);

  if (cons->map[cursor_idx] != NULL) {
    cons->grid[cursor_idx] = cons->map[cursor_idx]->ch;
    cons->map[cursor_idx] = NULL;
  }

  // Draw all the characters in the queue to the screen
  for (int i = 0; i < cons->queue_pos; i++) {
    struct queue_item c = cons->queue[i];
    int off = c.x + c.y * cons->cols;

    if (cons->map[off] == NULL)
      continue;

    write_char(cons, c.x, c.y, c.ch);

    cons->grid[off] = c.ch;
    cons->map[off] = NULL;
  }

  if (cons->old_x != cons->x_pos || cons->old_y != cons->y_pos) {
    write_char(cons, cons->old_x, cons->old_y,
      cons->grid[cons->old_x + cons->old_y * cons->cols]);
  }

  cons->old_x = cons->x_pos;
  cons->old_y = cons->y_pos;
  cons->queue_pos = 0;
}

void insert_queue(struct console *cons, struct charpoint *ch, size_t x, size_t y) {
  if (x >= cons->cols || y >= cons->rows)
    return;

  size_t grid_idx = x + y * cons->cols;
  struct queue_item *q = cons->map[grid_idx];

  if (q == NULL) {
    struct charpoint* old_char = &cons->grid[grid_idx];
    if (!(old_char->c != ch->c ||
        old_char->bg != ch->bg ||
        old_char->fg != ch->fg))
      return;

    q = &cons->queue[cons->queue_pos++];
    q->x = x;
    q->y = y;
    cons->map[grid_idx] = q;
  }

  q->ch = *ch;
}

////////////////////////////
//  Canvas Implmentation  //
////////////////////////////
static void draw_canvas(struct console *c) {
  // TODO: bmp backdrop
  for (int i = 0; i < c->info.height; i++) {
    for (int j = 0; j < c->info.width; j++) {
      c->bg_canvas[i * c->info.width + j] = c->theme[1];
      write_pixel(c, j, i, c->theme[1]);
    }
  }
}

static void clear_canvas(struct console *c) {
  struct charpoint f = {
    .c = ' ',
    .fg = c->theme[0],
    .bg = c->theme[1]
  };

  for (int i = 0; i < c->rows * c->cols; i++)  {
    insert_queue(c, &f, i % c->cols, i / c->cols);
  }

  c->x_pos = c->y_pos = 0;
}

static void scroll_canvas(struct console *c) {
  struct charpoint tmp, clean = {
    .c = ' ',
    .fg = c->theme[0],
    .bg = c->theme[1]
  };

  for (int i = c->cols; i < c->rows * c->cols; i++) {
    struct queue_item *q = c->map[i];

    if (q != NULL)
      tmp = q->ch;
    else
      tmp = c->grid[i];

    insert_queue(c, &tmp, (i - c->cols) % c->cols, (i - c->cols) / c->cols);
  }

  for (int i = ((c->rows - 1) * c->cols); i < c->rows * c->cols; i++)
    insert_queue(c, &clean, i % c->cols, i / c->cols);
}


/////////////////////////////
//   Print Implmentation   //
/////////////////////////////
static void console_putc(struct console *cons, char c) {
  switch (c) {
    case '\n': {
      if (cons->y_pos == cons->rows - 1) {
        cons->x_pos = 0;
        scroll_canvas(cons);
      } else {
        cons->y_pos++;
        cons->x_pos = 0;
      }
      break;
    }

    case '\r': break;
    default: {
      struct charpoint ch = {
        .c = c,
        .fg = cons->theme[0],
        .bg = cons->theme[1]
      };

      insert_queue(cons, &ch, cons->x_pos, cons->y_pos);
      cons->x_pos++;

      if (cons->x_pos == cons->cols) {
          cons->x_pos = 0;
          cons->y_pos++;
      }

      if (cons->y_pos == cons->rows) {
        cons->x_pos = 0;
        cons->y_pos--;
        scroll_canvas(g_console);
      }

      break;
    }
  }
}

static void console_write(char* str, size_t len) {
  for (size_t i = 0; i < len; i++)
    console_putc(g_console, str[i]);

  flush_queue(g_console);
}

bool console_init() {
  struct limine_framebuffer* fb = fb_req.response->framebuffers[0];
  fnt = (psf2_font_t*)sun12x22;

  // Setup the video info structure
  g_console = (struct console*)kmalloc(sizeof(struct console));
  g_console->info.pitch = fb->pitch;
  g_console->info.height = fb->height;
  g_console->info.width = fb->width;
  g_console->info.bpp = fb->bpp;
  g_console->info.n_bytes = fb->pitch * fb->height * (fb->bpp / 8);

  // Setup the console structure
  g_console->buffer = (volatile uint32_t *)fb->address;
  g_console->cols = (fb->width - CONSOLE_MARGIN * 2) / fnt->width;
  g_console->rows = (fb->height - CONSOLE_MARGIN * 2) / fnt->height;
  g_console->x_off = CONSOLE_MARGIN + ((fb->width - CONSOLE_MARGIN * 2) % fnt->width) / 2;
  g_console->y_off = CONSOLE_MARGIN + ((fb->height - CONSOLE_MARGIN * 2) % fnt->height) / 2;
  g_console->theme[0] = rgb(101, 123, 131);
  g_console->theme[1] = rgb(253, 246, 227);

  // Setup buffers
  g_console->bg_canvas = (uint32_t*)kmalloc(fb->width * fb->height * sizeof(uint32_t));
  g_console->grid = (struct charpoint*)kmalloc(g_console->rows * g_console->cols * sizeof(struct charpoint));
  g_console->queue = (struct queue_item*)kmalloc(g_console->rows * g_console->cols * sizeof(struct queue_item));
  g_console->map = (struct queue_item**)kmalloc(g_console->rows * g_console->cols * sizeof(void*));

  // Setup the screen for rendering
  draw_canvas(g_console);
  clear_canvas(g_console);
  flush_queue(g_console);

  return true;
}

struct print_sink console_sink = {
  .write = console_write,
  .init = console_init
};
