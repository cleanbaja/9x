#pragma once

#if defined (__aarch64__)
#include <arch/aarch64/trap.h>
#elif defined (__x86_64__)
#include <arch/x86_64/trap.h>
#endif
