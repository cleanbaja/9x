#ifndef LIB_KCON_H
#define LIB_KCON_H

struct kcon_sink {
  void (*setup)(const char* initial_buffer);
  void (*write)(const char* message);
  void (*flush)();
};

// KCON management functions...
void kcon_init();
void kcon_disable();
void kcon_register_sink(struct kcon_sink* sink);

// Logging functions...
void klog(char* fmt, ...);
void klog_unlocked(char* fmt, ...);

// Panics the kernel
#define PANIC(a, b, ...) panic(a, b, ##__VA_ARGS__)
void panic(void* frame, char* fmt, ...);

#endif // LIB_KCON_H

