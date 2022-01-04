#include <internal/stivale2.h>
#include <lib/builtin.h>
#include <lib/console.h>
#include <stddef.h>

#define SSFN_CONSOLEBITMAP_TRUECOLOR /* use the special renderer for 32 bit    \
                                        truecolor packed pixels */
#include <internal/ssfn.h>

extern unsigned char font_data;

void
console_init()
{
  // Get the framebuffer information
  struct stivale2_struct_tag_framebuffer* fb_tag;
  fb_tag = (struct stivale2_struct_tag_framebuffer*)stivale2_find_tag(
    STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID);

  // Setup ssfn
  ssfn_src = (ssfn_font_t*)&font_data; /* the bitmap font to use */
  ssfn_dst.ptr =
    (uint8_t*)fb_tag->framebuffer_addr; /* address of the linear frame buffer */
  ssfn_dst.w = fb_tag->framebuffer_width;  /* width */
  ssfn_dst.h = fb_tag->framebuffer_height; /* height */
  ssfn_dst.p = fb_tag->framebuffer_pitch;  /* bytes per line */
  ssfn_dst.x = 0;                          /* pen position (x-axis) */
  ssfn_dst.y = 16;                         /* pen position (y-axis) */
  ssfn_dst.fg = 0xFFFFFF;                  /* foreground color */
  ssfn_dst.bg = 0xB84C00;                  /* background color */

  // Set the background color
  uint32_t* p = (uint32_t*)fb_tag->framebuffer_addr;
  for (int i = 0; i < ssfn_dst.h * ssfn_dst.p; i++) {
    p[i] = 0xB84C00;
  }
}

void
console_write(char* str)
{
  while (*str != 0) {
    ssfn_putc(*str);
    str++;
  }
}
