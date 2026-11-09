// Existing public runtime suites; separate from the private generated game core.
#include "MiniTest.h"
#include <cstdio>
#include <cstdlib>
#ifdef __SWITCH__
#include <switch.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include "resources.h"
#endif
void reset_ps2_test_function_table();
void register_pad_input_tests();
void register_ps2_runtime_io_tests();
void register_ps2_runtime_kernel_tests();
void register_ps2_runtime_interrupt_tests();
void register_ps2_memory_tests();
void register_ps2_vu1_tests();
void register_ps2_vu_tests();
void register_ps2_gs_tests();
void register_ps2_iop_tests();
void register_ps2_sif_rpc_tests();
void register_ps2_sif_dma_tests();
int runSchedulerWorkload();
int runGuestDispatchTests();
int runCompiledVuTests();
int runIopDirectoryBoundsTests();
#ifdef NX_EXTENDED_CPU_TESTS
int runPackedIntegerTests();
int runFloatTests();
int runHostBoundaryTests();
#ifdef __SWITCH__
int runSwitchAllocTests();
#endif
#endif
static int runSuites() {
#ifdef __SWITCH__
    if(!reportCpuResources("runtime-suite-start",3)) return EXIT_FAILURE;
#endif
    // Existing pipeline tests require accurate timing; upstream defaults to its
    // throughput mode for games. Keep the test profile explicit on both hosts.
#ifdef _WIN32
    if(_putenv_s("PS2X_VU1_FAST","0") || _putenv_s("PS2X_IOP_REALTIME","0")) return EXIT_FAILURE;
#else
    if(setenv("PS2X_VU1_FAST","0",1) || setenv("PS2X_IOP_REALTIME","0",1)) return EXIT_FAILURE;
#endif
    std::puts("M02 public upstream runtime suites; accurate VU; EE-derived IOP; null host; no game boot");
    MiniTest::BeforeEach(reset_ps2_test_function_table);
    register_pad_input_tests(); register_ps2_runtime_io_tests();
    register_ps2_runtime_kernel_tests(); register_ps2_runtime_interrupt_tests();
    register_ps2_memory_tests(); register_ps2_vu1_tests(); register_ps2_vu_tests();
    register_ps2_gs_tests(); register_ps2_iop_tests();
    register_ps2_sif_rpc_tests(); register_ps2_sif_dma_tests();
    int failures=MiniTest::Run();
    failures+=runSchedulerWorkload();
    failures+=runGuestDispatchTests();
    failures+=runCompiledVuTests();
    failures+=runIopDirectoryBoundsTests();
#ifdef NX_EXTENDED_CPU_TESTS
    failures+=runPackedIntegerTests();
    failures+=runFloatTests();
    failures+=runHostBoundaryTests();
#ifdef __SWITCH__
    failures+=runSwitchAllocTests();
#endif
#endif
    std::cout.flush(); std::cerr.flush();
#ifdef __SWITCH__
    if(!reportCpuResources("runtime-suite-end",3)) ++failures;
#endif
    std::printf("M02 runtime suites finished failures=%d\n",failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
#ifdef __SWITCH__
struct SuiteWorker { int result=EXIT_FAILURE; };
static void workerEntry(void *argument) {
    try { static_cast<SuiteWorker *>(argument)->result=runSuites(); }
    catch(const std::exception &e) { std::printf("FAIL suite worker exception: %s\n",e.what()); }
    catch(...) { std::puts("FAIL suite worker unknown exception"); }
}
#endif
int main() {
#ifdef __SWITCH__
    FILE *log=freopen("sdmc:/switch/gtavcs_nx/runtime-tests-M02.log","w",stdout);
    if(!log) return EXIT_FAILURE;
    setvbuf(log,nullptr,_IOLBF,0);
    std::printf("[runtime-tests] HOS=%08x applet=%d\n",hosversionGet(),appletGetAppletType());
    if((appletGetAppletType()!=AppletType_Application &&
        appletGetAppletType()!=AppletType_SystemApplication) ||
        chdir("sdmc:/switch/gtavcs_nx/gamefolder")!=0) {
        std::puts("FAIL runtime test launch preconditions"); fclose(log); return EXIT_FAILURE;
    }
    // Route diagnostics alongside the suite output without opening sockets.
    dup2(fileno(stdout),fileno(stderr));
    // Test-created discs/cards are synthetic and isolated from game saves.
    const char *work="sdmc:/switch/gtavcs_nx/test-work-M02";
    const char *temporary="sdmc:/switch/gtavcs_nx/test-work-M02/tmp";
    for(const char *path : {work,temporary}) {
        struct stat info{};
        if((mkdir(path,0777)!=0 && errno!=EEXIST) || stat(path,&info)!=0 || !S_ISDIR(info.st_mode)) {
            std::printf("FAIL runtime fixture directory: %s\n",path);
            std::fclose(stderr); std::fclose(stdout); return EXIT_FAILURE;
        }
    }
    if(setenv("TMPDIR",temporary,1) || setenv("VCS_DATA_DIR","sdmc:/switch/gtavcs_nx/test-work-M02/userdata",1)) {
        std::puts("FAIL runtime fixture environment");
        std::fclose(stderr); std::fclose(stdout); return EXIT_FAILURE;
    }
    // Loader main-stack size cannot be changed by an arbitrary weak symbol.
    // Use libnx's explicit stack allocation contract for these large fixtures.
    constexpr size_t stackBytes=8u*1024u*1024u;
    void *stack=std::aligned_alloc(0x1000,stackBytes);
    SuiteWorker state;
    Thread worker{};
    Result rc=stack ? threadCreate(&worker,workerEntry,&state,stack,stackBytes,0x2c,-2) : MAKERESULT(Module_Libnx,LibnxError_OutOfMemory);
    if(R_SUCCEEDED(rc)) {
        rc=threadStart(&worker);
        if(R_SUCCEEDED(rc)) {
            rc=threadWaitForExit(&worker);
            if(R_FAILED(rc)) {
                // Do not release a stack or tear down stdio while its owner
                // might still execute. Process termination releases both.
                std::printf("FAIL worker join result=%08x; terminating process\n",rc);
                std::fflush(stdout);
                svcExitProcess();
            }
        }
        threadClose(&worker);
    }
    std::printf("[runtime-tests] worker_stack_bytes=%zu launch_result=%08x result=%d\n",stackBytes,rc,state.result);
    std::free(stack);
    const int result=R_SUCCEEDED(rc) ? state.result : EXIT_FAILURE;
#else
    const int result=runSuites();
#endif
    std::fflush(stdout);
#ifdef __SWITCH__
    std::fclose(stderr); std::fclose(stdout);
#endif
    return result;
}
