#pragma once

#include <stdint.h>

typedef struct {
	uint8_t magic[4];
	uint32_t version;
	uint32_t headersize;
	uint32_t flags;

	uint32_t nglyph;
	uint32_t glyph_size;
	uint32_t height;
	uint32_t width;

	uint8_t data[];
} psf2_font_t;

void console_init();

