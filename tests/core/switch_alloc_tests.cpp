#include "compat/switch_sse2neon_alloc.h"
#include <cstdint>
#include <cstdio>
#include <initializer_list>

int runSwitchAllocTests()
{
    unsigned checked = 0;
    for (std::size_t alignment : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 4096u}) {
        auto *memory = static_cast<unsigned char *>(_mm_malloc(257, alignment));
        if (!memory || reinterpret_cast<std::uintptr_t>(memory) % alignment != 0) return 1;
        for (unsigned i = 0; i < 257; ++i) memory[i] = static_cast<unsigned char>(i ^ 0xa5u);
        for (unsigned i = 0; i < 257; ++i)
            if (memory[i] != static_cast<unsigned char>(i ^ 0xa5u)) return 2;
        _mm_free(memory);
        ++checked;
    }
    for (std::size_t alignment : {0u, 3u, 5u, 6u, 7u, 12u, 24u}) {
        if (_mm_malloc(257, alignment) != nullptr) return 3;
        ++checked;
    }
    _mm_free(nullptr);
    void *zero = _mm_malloc(0, 16);
    _mm_free(zero); // Both null and a freeable allocation are allowed for size 0.
    std::printf("PASS Switch allocation contract: %u alignment cases, write/read/free, null/zero size\n", checked);
    return 0;
}

#ifndef NX_CORE_TEST_SUITE
int main() { return runSwitchAllocTests(); }
#endif
