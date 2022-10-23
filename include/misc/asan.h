#pragma once

/* KASAN is not yet ready for use :-( */
#define ASAN_POISON_MEMORY_REGION(addr, size)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size)
#define ASAN_NO_SANITIZE_ADDRESS
