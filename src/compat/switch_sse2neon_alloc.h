#pragma once
/* Port-owned allocation boundary for pinned SSE2NEON on Switch/newlib.
 * newlib supplies memalign/free, but this devkitA64 libc has no posix_memalign
 * symbol. Preserve SSE2NEON's small-alignment normalization and reject invalid
 * POSIX alignments before calling the supported allocator. No SIMD math changes.
 * Allocation conventions follow SSE2NEON v1.9.1 (MIT); source/license provenance
 * is pinned in tools/core-dependencies.json.
 */
#if !defined(__SWITCH__) || !defined(USE_SSE2NEON)
#error "This allocation boundary is only for the Switch SSE2NEON target"
#endif
#include <cstdlib>
#include <malloc.h>
#define SSE2NEON_ALLOC_DEFINED

static inline void *_mm_malloc(std::size_t size, std::size_t alignment)
{
    if (alignment == 1) return std::malloc(size);
    if (alignment == 2 || (sizeof(void *) == 8 && alignment == 4))
        alignment = sizeof(void *);
    if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0)
        return nullptr;
    return memalign(alignment, size);
}

static inline void _mm_free(void *pointer)
{
    std::free(pointer);
}
