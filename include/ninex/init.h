#ifndef NINEX_INIT_H
#define NINEX_INIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(*arr))
#define INIT_SMP_READY (1 << 2)
#define INIT_COMPLETE  (1 << 3)
struct init_stage {
    struct init_stage** deps;
    struct init_stage* next;
    struct init_stage* next_resolve;
    
    const char* name;
    void (*func)();
    int count, depth, flags;
    bool completed;
};
 
// Makes a stage public, so that other code can refer to it
#define EXPORT_STAGE(name) extern struct init_stage name[]
#define STAGE_COMPLETE(stage) ({   \
    EXPORT_STAGE(stage);           \
    stage->completed;              \
})

#define CREATE_STAGE(stage, callback, flgs, ...)             \
    static void callback();                                  \
    static struct init_stage* stage##deps[] = __VA_ARGS__;   \
    struct init_stage stage[] = {(struct init_stage){        \
        .deps = stage##deps,                                 \
        .count = ARRAY_LEN(stage##deps),                     \
        .func = callback,                                    \
        .name = #stage,                                      \
        .next = NULL,                                        \
        .next_resolve = NULL,                                \
        .depth = 0,                                          \
        .flags = flgs,                                       \
        .completed = false,                                  \
    }};

// Dummy callback for init stages without a callback
#define DUMMY_CALLBACK dummy_func
static void dummy_func(void) {} 

// The main kernel init stage, which is the last one to run
EXPORT_STAGE(root_stage);
 
#endif // NINEX_INIT_H

