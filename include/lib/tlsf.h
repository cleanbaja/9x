#ifndef TLSF_H
#define TLSF_H

/*
 * Two Level Segregated Fit memory allocator, version 3.1.
 * Written by Matthew Conte
 *	http://tlsf.baisoku.org
 *
 * Based on the original documentation by Miguel Masmano:
 *	http://www.gii.upv.es/tlsf/main/docs
 *
 * This implementation was written to the specification
 * of the document, therefore no GPL restrictions apply.
 *
 * Copyright (c) 2006-2016, Matthew Conte
 * Copyright (c) 2018, Niometrics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL MATTHEW CONTE BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* tlsf_t: a TLSF structure. Can contain 1 to N pools */
/* tlsf_pool_t: a block of memory that TLSF can manage */
typedef struct tlsf tlsf_t;
typedef struct tlsf_pool tlsf_pool_t;

/* Create/destroy a memory pool */
tlsf_t *tlsf_create(void *mem);
tlsf_t *tlsf_create_with_pool(void *mem, size_t bytes);
void tlsf_destroy(tlsf_t *tlsf);
tlsf_pool_t *tlsf_get_pool(tlsf_t *tlsf);

/* Add/remove memory pools */
tlsf_pool_t *tlsf_add_pool(tlsf_t *tlsf, void *mem, size_t bytes);
void tlsf_remove_pool(tlsf_t *tlsf, tlsf_pool_t *pool);

/* malloc/memalign/realloc/free replacements */
void *tlsf_malloc(tlsf_t *tlsf, size_t bytes);
void *tlsf_memalign(tlsf_t *tlsf, size_t align, size_t bytes);
void *tlsf_realloc(tlsf_t *tlsf, void *ptr, size_t size);
void tlsf_free(tlsf_t *tlsf, void *ptr);

#ifndef DNDEBUG
void tlsf_track_allocation(void* addr, void* data);
#endif

/* Returns internal block size, not original request size */
size_t tlsf_block_size(void *ptr);

/* Returns TLSF structure used for allocation of pointer */
tlsf_t *tlsf_from_ptr(void *ptr);

/* Overheads/limits of internal structures */
size_t tlsf_size(void);
size_t tlsf_align_size(void);
size_t tlsf_block_size_min(void);
size_t tlsf_block_size_max(void);
size_t tlsf_pool_overhead(void);
size_t tlsf_alloc_overhead(void);

/* Debugging */
typedef void (*tlsf_walker)(void *ptr, size_t size, int used, void *user);
void tlsf_walk_pool(tlsf_pool_t *pool, tlsf_walker walker, void *user);
/* Returns nonzero if any internal consistency check fails */
int tlsf_check(tlsf_t *tlsf);
int tlsf_check_pool(tlsf_pool_t *pool);

void tlsf_init();

#if defined(__cplusplus)
};
#endif

#ifndef KASAN

#define ASAN_POISON_MEMORY_REGION(addr, size)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size)
#define ASAN_NO_SANITIZE_ADDRESS

#else

#include <lvm/lvm.h>

#define ASAN_POISON_MEMORY_REGION(addr, size) kasan_poison_shadow((uintptr_t)addr, size, 0xFF)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) kasan_unpoison_shadow((uintptr_t)addr, size)
#define ASAN_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))

#endif

#endif /* TLSF_H */