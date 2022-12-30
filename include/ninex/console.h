#pragma once

#include <lib/print.h>
#include <stdint.h>

#define IMPORT_FONT(name)               \
  extern char name[];                   \
  asm (                                 \
    ".global " #name "\n"               \
    ".align 16\n"                       \
    #name ":\n\t"                       \
    ".incbin \"root/" #name ".psfu\"\n" \
  );

// pixel margin from the corners of the screen
#define CONSOLE_MARGIN 32

typedef struct {
  uint8_t  magic[4];
  uint32_t version;
  uint32_t headersize;
  uint32_t flags;

  uint32_t nglyph;
  uint32_t glyph_size;
  uint32_t height;
  uint32_t width;

  uint8_t data[];
} psf2_font_t;

extern struct print_sink console_sink;
