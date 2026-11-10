#pragma once
// CPU-test budgets, distinct from the loader's kernel-mapped reservation.
#ifdef __SWITCH__
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <malloc.h>
extern "C" {
extern char *fake_heap_start;
extern char *fake_heap_end;
extern size_t nx_cpu_heap_budget;
}
inline bool reportCpuResources(const char *stage,unsigned temporaryWorkers) {
    u64 total=0,used=0; MemoryInfo stack{}; u32 page=0; char probe;
    const Result a=svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
    const Result b=svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
    const Result c=svcQueryMemory(&stack,&page,reinterpret_cast<u64>(&probe));
    const auto allocation=mallinfo();
    const size_t arena=static_cast<size_t>(fake_heap_end-fake_heap_start);
    void *oversized=std::malloc(nx_cpu_heap_budget+1);
    const bool rejected=oversized==nullptr; std::free(oversized);
    std::printf("[resources] %s mapped_total=%llu mapped_used=%llu allocator_limit=%zu arena=%zu allocator_in_use=%zu allocator_obtained=%zu stack_region=%llu temporary_workers_peak=%u over_budget_rejected=%d svc=%08x,%08x,%08x\n",
        stage,static_cast<unsigned long long>(total),static_cast<unsigned long long>(used),
        nx_cpu_heap_budget,arena,allocation.uordblks,allocation.arena,
        static_cast<unsigned long long>(stack.size),temporaryWorkers,rejected,a,b,c);
    // libnx's argvSetup carves 0x9000 bytes from the heap start when given an argv.
    const bool arenaExpected=arena==nx_cpu_heap_budget || (envHasArgv() && arena==nx_cpu_heap_budget-0x9000);
    return arenaExpected && rejected && R_SUCCEEDED(a) && R_SUCCEEDED(b) && R_SUCCEEDED(c);
}
#endif
