// Private generated-code linkage probe. Never boot the game or execute its code.
#include "ps2_runtime.h"
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "resources.h"
int runSchedulerWorkload();
int runGuestDispatchTests();
int runIopDirectoryBoundsTests();

int main() {
    FILE *log=freopen("sdmc:/switch/gtavcs_nx/private-core-M02.log","w",stdout);
    if(!log) return EXIT_FAILURE;
    setvbuf(log,nullptr,_IOLBF,0);
    std::printf("[private-core] HOS=%08x applet=%d\n",hosversionGet(),appletGetAppletType());
    int result=EXIT_FAILURE;
    std::puts("M02 private generated EE/VU linked; public synthetic guest workload only; no game function executed");
    if((appletGetAppletType()==AppletType_Application ||
        appletGetAppletType()==AppletType_SystemApplication) &&
        chdir("sdmc:/switch/gtavcs_nx/gamefolder")==0) {
        const bool resources=reportCpuResources("private-core-start",1);
        size_t assigned=0;
        for(uint32_t i=0;i<g_ps2RecompiledFunctionTableSlotCount;++i)
            if(g_ps2RecompiledFunctionTable[i]) ++assigned;
        const bool bounds=g_ps2RecompiledFunctionTableEnd>g_ps2RecompiledFunctionTableBase &&
            (g_ps2RecompiledFunctionTableEnd-g_ps2RecompiledFunctionTableBase)/4==g_ps2RecompiledFunctionTableSlotCount;
        std::printf("[core-table] base=%08x end=%08x slots=%u assigned=%zu bounds=%d\n",
            g_ps2RecompiledFunctionTableBase,g_ps2RecompiledFunctionTableEnd,
            g_ps2RecompiledFunctionTableSlotCount,assigned,bounds);
        result=resources && bounds && assigned==236304u ? EXIT_SUCCESS : EXIT_FAILURE;
        if(result==EXIT_SUCCESS) {
            try { result=runSchedulerWorkload(); if(!result) result=runGuestDispatchTests(); if(!result) result=runIopDirectoryBoundsTests(); }
            catch(const std::exception &error) { std::printf("FAIL private workload: %s\n",error.what()); result=EXIT_FAILURE; }
            catch(...) { std::puts("FAIL private workload: unknown exception"); result=EXIT_FAILURE; }
        }
        if(!reportCpuResources("private-core-end",1)) result=EXIT_FAILURE;
    }
    std::printf("[private-core] linkage/constructor/synthetic-workload check %s result=%d\n",result ? "FAIL":"PASS",result);
    std::fflush(log); std::fclose(log);
    return result;
}
