// Bound the CPU test allocator even when the NRO loader provides a larger heap.
// Uses libnx's documented __libnx_initheap/fake_heap_start/end override.
#include <switch.h>
#include <cstddef>

extern "C" {
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    size_t nx_cpu_heap_budget = 128u * 1024u * 1024u;

    void __libnx_initheap(void)
    {
        void *base = nullptr;
        if (envHasHeapOverride()) {
            if (envGetHeapOverrideSize() < nx_cpu_heap_budget)
                diagAbortWithResult(MAKERESULT(Module_Libnx,LibnxError_HeapAllocFailed));
            base = envGetHeapOverrideAddr();
        } else if (R_FAILED(svcSetHeapSize(&base,nx_cpu_heap_budget))) {
            diagAbortWithResult(MAKERESULT(Module_Libnx,LibnxError_HeapAllocFailed));
        }
        fake_heap_start = static_cast<char *>(base);
        fake_heap_end = fake_heap_start + nx_cpu_heap_budget;
    }
}
