#include <dev/console.h>
#include <misc/limine.h>
#include <lib/print.h>

struct terminal_ctx {
  volatile uint32_t* buffer;
  uint32_t fg_color, bg_color;
  size_t pitch, height, width;
  uint32_t cursor_x, cursor_y;
  size_t x_off, x_lim, y_off, y_lim;
};

#define CREATE_FONT(name)               \
  extern char name[];                   \
  asm (                                 \
    ".global " #name "\n"               \
    ".align 16\n"                       \
    #name ":\n\t"                       \
    ".incbin \"root/" #name ".psfu\"\n" \
  );

volatile static struct limine_framebuffer_request fb_req = {
  .id = LIMINE_FRAMEBUFFER_REQUEST,
  .revision = 0
};

static struct terminal_ctx g_ctx;
static psf2_font_t* fnt;
CREATE_FONT(sun12x22);

static void memset32(void *ptr, uint32_t val, int len) {
  uint32_t *real_ptr = (uint32_t *)ptr;

  for (int i = 0; i < (len / 4); i++) {
    real_ptr[i] = val;
  }
}

static inline uint32_t blend(int red, int green, int blue) {
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

static inline void write_pixel(struct terminal_ctx* c, int x, int y, uint32_t color) {
  uint32_t idx = x + (c->pitch / sizeof(uint32_t)) * y;
  c->buffer[idx] = color;

  asm volatile ("" ::: "memory");
}

static void write_char(struct terminal_ctx* ct, uint16_t c, int rx, int ry) {
  uint8_t* glyph = (uint8_t*)((uintptr_t)fnt + fnt->headersize + (c * fnt->glyph_size));
  int orig_x = rx;

  for (int i = 0; i < fnt->height; i++) {
    for (int j = 0; j < fnt->width; j++) {
      uint8_t bit = glyph[i * (fnt->glyph_size / fnt->height) + j / 8] >> (7 - j % 8);
      write_pixel(ct, rx++, ry + i, (bit & 1) ? ct->fg_color : ct->bg_color);
    }

    rx = orig_x;
  }
}

#define HLINE(y, xstart, xend, col) for (int ani = (xstart); ani < (xend); ani++) {write_pixel(ct, ani, y, col);}
#define VLINE(ystart, yend, x, col) for (int ani = (ystart); ani < (yend); ani++) {write_pixel(ct, x, ani, col);}

static void draw_canvas(struct terminal_ctx* ct) {
  memset32((void*)ct->buffer, ct->bg_color, ct->pitch * ct->height);
  uint32_t bg_color = blend(161, 202, 241);
  int off = fnt->width / 2;
  if (fnt->width == 8)
    off++;

  // Fill in the top and bottom...
  for (int i = 0; i < ct->width; i++) {
    for (int j = 0; j < (fnt->height * ct->y_off) - 5; j++) {
      write_pixel(ct, i, j, bg_color);
    }
  }
  for (int i = 0; i < ct->width; i++) {
    for (int j = (ct->y_lim * fnt->height); j < ct->height; j++) {
      write_pixel(ct, i, j, bg_color);
    }
  }

  // Then the left and right margins...
  for (int i = 0; i < (fnt->width * ct->x_off) - 5; i++) {
    for (int j = 0; j < ct->height; j++) {
      write_pixel(ct, i, j, bg_color);
    }
  }
  for (int i = (ct->x_lim * fnt->width); i < ct->width; i++) {
    for (int j = 0; j < ct->height; j++) {
      write_pixel(ct, i, j, bg_color);
    }
  }

  // Next, draw the borders
  VLINE(ct->y_off * fnt->height - off, ct->y_lim * fnt->height, (ct->x_off * fnt->width) - off, 0);
  VLINE(ct->y_off * fnt->height - off, ct->y_lim * fnt->height, ct->x_lim * fnt->width, 0);
  HLINE(ct->y_off * fnt->height - off, ct->x_off * fnt->width - off, ct->x_lim * fnt->width, 0);
  HLINE(ct->y_lim * fnt->height, ct->x_off * fnt->width - off, ct->x_lim * fnt->width, 0);
}

/////////////////////////////
//   Print Implmentation   //
/////////////////////////////
static void console_putc(struct terminal_ctx* ct, char c) {
  switch (c) {
  case '\n':
    ct->cursor_y++;
    ct->cursor_x = ct->x_off;
    break;
  default:
    if (ct->cursor_x >= ct->x_lim) {
      ct->cursor_x = ct->x_off;
      ct->cursor_y++;
    }

    if (ct->cursor_y >= ct->y_lim) {
      kprint("console: (TODO) scrolling\n");
      break;
    }

    write_char(ct, c, ct->cursor_x * fnt->width, ct->cursor_y * fnt->height);
    ct->cursor_x++;
  }
}

static void console_write(char* str, size_t len) {
  for (size_t i = 0; i < len; i++)
    console_putc(&g_ctx, str[i]);
}

static bool console_stub() { return true; }

static struct print_sink console_sink = {
  .write = console_write,
  .init = console_stub
};

void console_init() {
  struct limine_framebuffer* fb = fb_req.response->framebuffers[0];
  fnt = (psf2_font_t*)sun12x22;
  
  // Setup the global context
  g_ctx.buffer = (volatile uint32_t*)fb->address;
  g_ctx.pitch  = fb->pitch;
  g_ctx.height = fb->height;
  g_ctx.width  = fb->width;
  g_ctx.fg_color = blend(10, 10, 10);
  g_ctx.bg_color = blend(255, 255, 255);
  
  // Setup the screen to look like this...
  // --------------------------
  // |           10%          |
  // |     ______________     |
  // |     |            |     |
  // | 20% |            | 20% |
  // |     |____________|     |
  // |                        |
  // |           10%          |
  // --------------------------
  g_ctx.x_off = (g_ctx.width / fnt->width) / 14; 
  g_ctx.x_lim = (g_ctx.width / fnt->width) - g_ctx.x_off;
  g_ctx.y_off = ((g_ctx.height / fnt->height) / 15) + 1;
  g_ctx.y_lim = (g_ctx.height / fnt->height) - (g_ctx.y_off - 1);
  g_ctx.cursor_x = g_ctx.x_off;
  g_ctx.cursor_y = g_ctx.y_off;

  // Finally, draw the canvas
  print_register_sink(console_sink);
  draw_canvas(&g_ctx);
}
