#pragma once

#include <stddef.h>
#include <stdbool.h>

struct print_sink {
  void (*write)(char*, size_t);
  bool (*init)();
  bool active;
};

void snprintf(char* buffer, size_t buf_len, const char* fmt, ...);
void kprint(const char* fmt, ...);

void print_init();
int  print_register_sink(struct print_sink bck);
void print_disable(int spot);

