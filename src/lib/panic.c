#include <lib/panic.h>
#include <lib/print.h>
#include <misc/nanoprintf.h>
#include <arch/trap.h>
#include <stdbool.h>
#include <stdatomic.h>

static atomic_bool in_panic = ATOMIC_VAR_INIT(false); 
static char panic_buf[512] = {0};

static void do_stacktrace(struct cpu_regs* context) {
  kprint("Stacktrace:\n");
  
  struct stack_frame {
    struct stack_frame* next;
    uintptr_t pc;
  } *frame = __builtin_frame_address(0);

  if (context) {
#ifdef __x86_64__
    frame = (struct stack_frame*)context->rbp;
#else
    frame = (struct stack_frame*)context->x[29];
#endif
  }

  int framecnt = 0;
  while (frame && frame->pc) {
    kprint(" * [%d] 0x%lx\n", framecnt++, frame->pc);
    frame = frame->next;
  }
}

__attribute__((noreturn)) void panic(struct cpu_regs* context, const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  
  // If we're currently panic'ing, stop
  if (in_panic) {
    goto halt;
  } else {
    in_panic = true;
  }

  kprint("\n---------- KERNEL PANIC ----------\n");

  // Print a message if needed
  if (fmt) {
    int len = npf_vsnprintf(panic_buf, 512, fmt, va);
    kprint("%s\n", panic_buf);
  }

  // Print context (if needed)
  if (context)
    trap_dump_frame(context);

  // Do a stack trace, then go to rest
  do_stacktrace(context);

halt:
  va_end(va);
  for (;;) {
#if defined (__x86_64__)
    asm volatile ("cli; hlt");
#elif defined (__aarch64__)
    asm volatile ("msr DAIFSet, #15; wfi");
#endif
  }
}

#ifdef SECURE_BUILD

/***********************
 *    Ubsan Support    *
 ***********************/

struct source_location {
  const char *file;
  uint32_t line;
  uint32_t column;
};

struct type_descriptor {
  uint16_t kind;
  uint16_t info;
  char name[];
};

struct ubsan_overflow_data {
  struct source_location location;
  struct type_descriptor *type;
};

struct ubsan_shift_out_of_bounds_data {
  struct source_location location;
  struct type_descriptor *left_type;
  struct type_descriptor *right_type;
};

struct ubsan_invalid_value_data {
  struct source_location location;
  struct type_descriptor *type;
};

struct ubsan_array_out_of_bounds_data {
  struct source_location location;
  struct type_descriptor *array_type;
  struct type_descriptor *index_type;
};

struct ubsan_type_mismatch_v1_data {
  struct source_location location;
  struct type_descriptor *type;
  uint8_t align;
  uint8_t type_check_kind;
};

struct ubsan_negative_vla_data {
    struct source_location location;
    struct type_descriptor *type;
};

struct ubsan_nonnull_return_data {
    struct source_location location;
};

struct ubsan_nonnull_arg_data {
    struct source_location location;
};

struct ubsan_unreachable_data {
    struct source_location location;
};

struct ubsan_invalid_builtin_data {
    struct source_location location;
    unsigned char kind;
};

static void __report(char *message, struct source_location loc) {
    panic(NULL, "ubsan: panic at %s:%d, col %d (%s)", loc.file, loc.line, loc.column, message);
}

void __ubsan_handle_add_overflow(struct ubsan_overflow_data *data) {
    __report("Addition overflow", data->location);
}

void __ubsan_handle_sub_overflow(struct ubsan_overflow_data *data) {
    __report("Subtraction overflow", data->location);
}

void __ubsan_handle_mul_overflow(struct ubsan_overflow_data *data) {
    __report("Multiplication overflow", data->location);
}

void __ubsan_handle_divrem_overflow(struct ubsan_overflow_data *data) {
    __report("Division overflow", data->location);
}

void __ubsan_handle_negate_overflow(struct ubsan_overflow_data *data) {
    __report("Negation overflow", data->location);
}

void __ubsan_handle_pointer_overflow(struct ubsan_overflow_data *data) {
    __report("Pointer overflow", data->location);
}

void __ubsan_handle_shift_out_of_bounds(struct ubsan_shift_out_of_bounds_data *data) {
    __report("Shift out of bounds", data->location);
}

void __ubsan_handle_load_invalid_value(struct ubsan_invalid_value_data *data) {
    __report("Invalid load value", data->location);
}

void __ubsan_handle_out_of_bounds(struct ubsan_array_out_of_bounds_data *data) {
    __report("Array out of bounds", data->location);
}

void __ubsan_handle_type_mismatch_v1(struct ubsan_type_mismatch_v1_data *data, uintptr_t ptr) {
    if (!ptr)
        __report("NULL pointer accessed", data->location);
    else if (ptr & ((1 << data->align) - 1))
        __report("Use of misaligned pointer", data->location);
    else
        __report("No space for object", data->location);
}

void __ubsan_handle_vla_bound_not_positive(struct ubsan_negative_vla_data *data) {
    __report("Variable-Length argument is negative", data->location);
}

void __ubsan_handle_nonnull_return(struct ubsan_nonnull_return_data *data) {
    __report("Non-NULL return is null", data->location);
}

void __ubsan_handle_nonnull_arg(struct ubsan_nonnull_arg_data *data) {
    __report("Non-NULL argument is null", data->location);
}

void __ubsan_handle_builtin_unreachable(struct ubsan_unreachable_data *data) {
    __report("Unreachable code accessed", data->location);
}

void __ubsan_handle_invalid_builtin(struct ubsan_invalid_builtin_data *data) {
    __report("Invalid Builtin", data->location);
}

#endif
