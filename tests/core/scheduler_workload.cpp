// Public deterministic guest callbacks through the real EE dispatcher. No game
// function is called. Empty table slots are restored even after failure.
#include "ps2_runtime_macros.h"
#include "ps2_syscalls.h"
#include "runtime/ee_scheduler.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
struct Probe {
    std::array<uint32_t,3> pc{};
    unsigned steps=0, yields=0, wakes=0;
    uint32_t trace=0, result=0;
    bool bad=false, afterSleep=false;
};
Probe *active=nullptr;
void note(unsigned role, R5900Context *ctx, PS2Runtime *runtime) {
    auto &p=*active;
    p.bad |= !runtime->eeScheduler().isExecutingGuest() || ctx->pc!=p.pc[role-1];
    ++p.steps; p.trace=p.trace*17u+role;
}
void PS2_GUEST_CC entry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(1,ctx,runtime);
    uint32_t sum=0x12345678u;
    for(unsigned i=0;i<64;++i) sum=(sum*33u)^i;
    std::memcpy(ram+0x400,&sum,4);
    SET_GPR_U64(ctx,16,0x1234567887654321ull);
    SET_GPR_U32(ctx,31,active->pc[2]);
    ctx->pc=active->pc[2]; ++active->yields;
    ps2_syscalls::SleepThread(ram,ctx,runtime);
    active->afterSleep=true; // Must unwind to the dispatcher, never fall through.
}
void PS2_GUEST_CC driver(uint8_t *,R5900Context *ctx,PS2Runtime *runtime) {
    note(2,ctx,runtime);
    ctx->pc=0;
    const int rc=runtime->eeScheduler().wakeupThread(EeScheduler::kMainThreadId,false);
    active->bad |= rc!=0; ++active->wakes;
    runtime->eeScheduler().transferIfRequested(false);
}
void PS2_GUEST_CC resume(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(3,ctx,runtime);
    std::memcpy(&active->result,ram+0x400,4);
    active->bad |= GPR_U64(ctx,16)!=0x1234567887654321ull ||
        GPR_U32(ctx,31)!=active->pc[2];
    ctx->pc=0;
    runtime->eeScheduler().requestStop();
}
struct ExpectedFailure {};
void PS2_GUEST_CC failure(uint8_t *,R5900Context *,PS2Runtime *) { throw ExpectedFailure{}; }
static_assert(std::is_same_v<decltype(&entry),PS2Runtime::RecompiledFunction>);
struct Slots {
    std::array<uint32_t,3> index{};
    bool valid=false;
    Slots() {
        unsigned count=0;
        for(uint32_t i=0;i<g_ps2RecompiledFunctionTableSlotCount && count<3;++i) {
            if(g_ps2RecompiledFunctionTableBase+i*4u==0) continue;
            if(!g_ps2RecompiledFunctionTable[i]) index[count++]=i;
        }
        valid=count==3;
    }
    ~Slots() { if(valid) for(auto i:index) g_ps2RecompiledFunctionTable[i]=nullptr; }
};
}

int runSchedulerWorkload() {
    Slots slots;
    if(!slots.valid) { std::puts("FAIL scheduler workload: no spare synthetic slots"); return 1; }
    std::vector<uint8_t> ram(PS2_RAM_SIZE,0);
    auto runtime=std::make_unique<PS2Runtime>();
    Probe probe;
    for(unsigned i=0;i<3;++i) probe.pc[i]=g_ps2RecompiledFunctionTableBase+slots.index[i]*4u;
    active=&probe;
    struct Clear { ~Clear() { active=nullptr; } } clear;
    auto &ee=runtime->eeScheduler();
    uint32_t expected=0x12345678u;
    for(unsigned i=0;i<64;++i) expected=(expected*33u)^i;
    std::printf("[scheduler-workload] entry=%08x driver=%08x resume=%08x expected=%08x\n",
        probe.pc[0],probe.pc[1],probe.pc[2],expected);
    for(unsigned repetition=0;repetition<3;++repetition) {
        const auto pc=probe.pc; probe=Probe{}; probe.pc=pc;
        runtime->registerFunction(pc[0],entry);
        runtime->registerFunction(pc[1],driver);
        runtime->registerFunction(pc[2],resume);
        R5900Context ctx; ctx.pc=pc[0]; SET_GPR_U32((&ctx),29,0x01f00000u);
        ee.reset(ram.data(),ctx);
        const int id=ee.createThread(EeThreadCreateParams{0,pc[1],0x10000,0x1000,0,5,0});
        if(id<0 || ee.startThread(id,0,ctx,false)!=0) return 2;
        std::mutex mutex; std::condition_variable condition; bool finished=false,timedOut=false;
        std::thread watchdog([&] {
            std::unique_lock lock(mutex);
            if(!condition.wait_for(lock,std::chrono::seconds(2),[&]{return finished;})) {
                timedOut=true; ee.requestStop();
            }
        });
        try { ee.run(); }
        catch(...) {
            { std::lock_guard lock(mutex); finished=true; }
            condition.notify_one(); watchdog.join(); throw;
        }
        { std::lock_guard lock(mutex); finished=true; }
        condition.notify_one(); watchdog.join();
        const uint64_t cycles=ee.snapshot().eeCycle;
        std::printf("[scheduler-workload] run=%u steps=%u trace=%u yields=%u wakes=%u cycles=%llu result=%08x timeout=%d\n",
            repetition,probe.steps,probe.trace,probe.yields,probe.wakes,
            static_cast<unsigned long long>(cycles),probe.result,timedOut);
        if(timedOut || probe.bad || probe.afterSleep || probe.steps!=3 || probe.trace!=326u ||
            probe.yields!=1 || probe.wakes!=1 || probe.result!=expected ||
            cycles!=3u*EeScheduler::kGuestDispatchCycles || ee.isExecutingGuest()) return 3;
    }
    // Dispatcher failure must clear its guest-executing flag and allow reset.
    runtime->registerFunction(probe.pc[0],failure);
    R5900Context ctx; ctx.pc=probe.pc[0]; ee.reset(ram.data(),ctx);
    bool caught=false;
    try { ee.run(); } catch(const ExpectedFailure &) { caught=true; }
    if(!caught || ee.isExecutingGuest()) return 4;
    ee.reset(ram.data(),ctx); ee.requestStop(); ee.run();
    if(ee.isExecutingGuest()) return 5;
    std::puts("PASS scheduler workload: repeated run/yield/wake/resume/stop; failure unwind/reset; table slots restored");
    return 0;
}
