// Synthetic M02 CPU checks only. No generated game functions or runtime boot.
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>

extern "C" {
    extern size_t nx_cpu_heap_budget;
}
int runSwitchAllocTests();
int runPackedIntegerTests();
int runFloatTests();
int runHostBoundaryTests();

static void memoryReport(const char *stage)
{
    u64 total = 0, used = 0;
    const Result a = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    const Result b = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    std::printf("[memory] %s mapped_total=%llu mapped_used=%llu results=%08x,%08x allocator_budget=%zu\n",
        stage, static_cast<unsigned long long>(total), static_cast<unsigned long long>(used),
        a,b,nx_cpu_heap_budget);
}

int main()
{
    FILE *log = freopen("sdmc:/switch/gtavcs_nx/cpu-tests-M02.log", "w", stdout);
    if (!log) return EXIT_FAILURE;
    setvbuf(log,nullptr,_IOLBF,0);
    std::printf("[cpu-tests] synthetic public-source tests only; HOS=%08x applet=%d\n",
        hosversionGet(),appletGetAppletType());
    int result = EXIT_FAILURE;
    if (appletGetAppletType() != AppletType_Application &&
        appletGetAppletType() != AppletType_SystemApplication) {
        std::printf("[cpu-tests] Full application memory required\n");
    } else if (chdir("sdmc:/switch/gtavcs_nx/gamefolder") != 0) {
        std::printf("[cpu-tests] checked chdir failed: %s\n",std::strerror(errno));
    } else if (appletMainLoop()) {
        MemoryInfo stack{};
        u32 page = 0;
        char stack_probe;
        const Result rc = svcQueryMemory(&stack,&page,reinterpret_cast<u64>(&stack_probe));
        std::printf("[cpu-tests] char_signed=%d vector_bytes=16 stack_region_bytes=%llu result=%08x temporary_workers_peak=2 joined_before_exit\n",
            char(-1) < 0,static_cast<unsigned long long>(stack.size),rc);
        memoryReport("before");
        void *oversized = std::malloc(nx_cpu_heap_budget + 1);
        const bool heap_bounded = oversized == nullptr;
        std::free(oversized);
        std::printf("[memory] allocation_over_budget_rejected=%d loader_override_bytes=%llu\n",
            heap_bounded,static_cast<unsigned long long>(envHasHeapOverride() ? envGetHeapOverrideSize() : 0));
        const u64 start = armGetSystemTick();
        const int allocation = heap_bounded ? runSwitchAllocTests() : -1;
        const int integers = allocation == 0 ? runPackedIntegerTests() : -1;
        const int floating = integers == 0 ? runFloatTests() : -1;
        const int boundary = floating == 0 ? runHostBoundaryTests() : -1;
        memoryReport("after");
        result = allocation == 0 && integers == 0 && floating == 0 && boundary == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        std::printf("[cpu-tests] %s allocation=%d packed_integer=%d floating=%d boundary=%d elapsed_ns=%llu\n",
            result == EXIT_SUCCESS ? "PASS" : "FAIL",allocation,integers,floating,boundary,
            static_cast<unsigned long long>(armTicksToNs(armGetSystemTick()-start)));
    }
    std::printf("[cpu-tests] closing log before process exit result=%d\n",result);
    std::fflush(log);
    std::fclose(log);
    return result;
}
