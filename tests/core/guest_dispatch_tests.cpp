// Public synthetic guest callbacks admitted through the real runtime dispatcher.
// No private function runs; a reserved empty table block is restored on all exits.
#include "ps2_runtime_macros.h"
#include "runtime/ee_scheduler.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
enum Role : unsigned { Entry, Callee, Tail, Owner, Alias, Deferred, Final, Spare, RoleCount };
using Vector = std::array<uint32_t,4>;
constexpr Vector argument{0x01234567u,0x89abcdefu,0x80000000u,0xffffffffu};
constexpr Vector saved{0x10203040u,0x50607080u,0x90a0b0c0u,0xd0e0f000u};
constexpr Vector guard{0xffeeddccu,0xbbaa9988u,0x77665544u,0x33221100u};
constexpr Vector returnOne{0x13579bdfu,0x2468ace0u,0xabcdef01u,0x80000001u};
constexpr Vector returnTwo{0xdeadbeefu,0x8000ffffu,0xfedcba98u,0x76543210u};
constexpr uint64_t returnUpper=0xface0123badc4567ull;
constexpr uint64_t guestStack=0x01e00000ull;
__m128i asVector(const Vector &v) { __m128i out; std::memcpy(&out,v.data(),16); return out; }
bool matches(__m128i value,const Vector &expected) { Vector out; std::memcpy(out.data(),&value,16); return out==expected; }

struct TableBlock {
    uint32_t first=0;
    bool valid=false;
    TableBlock() {
        unsigned empty=0;
        for(uint32_t i=0;i<g_ps2RecompiledFunctionTableSlotCount;++i) {
            const uint64_t pc=uint64_t(g_ps2RecompiledFunctionTableBase)+uint64_t(i)*4u;
            if(pc==0 || pc>=g_ps2RecompiledFunctionTableEnd || g_ps2RecompiledFunctionTable[i]) empty=0;
            else if(++empty==RoleCount) { first=i+1-RoleCount; valid=true; break; }
        }
    }
    uint32_t pc(Role role) const { return g_ps2RecompiledFunctionTableBase+(first+unsigned(role))*4u; }
    void restore() { if(valid) for(unsigned i=0;i<RoleCount;++i) g_ps2RecompiledFunctionTable[first+i]=nullptr; }
    ~TableBlock() { restore(); }
};
struct Probe {
    std::array<uint32_t,RoleCount> pc{};
    std::array<unsigned,12> trace{};
    unsigned traceSize=0,ownerEntries=0,aliasEntries=0;
    bool slow=false,continuation=false,bad=false,unexpectedInline=false;
    uint8_t *ram=nullptr;
    PS2Runtime *runtime=nullptr;
};
thread_local Probe *active=nullptr;
void note(unsigned event,Role role,uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    Probe &p=*active;
    p.bad |= ram!=p.ram || runtime!=p.runtime || ctx->pc!=p.pc[role] ||
        !runtime->eeScheduler().isExecutingGuest() || !matches(ctx->r[4],argument) ||
        !matches(ctx->r[16],saved) || !matches(ctx->r[30],guard) || GPR_U64(ctx,29)!=guestStack;
    uint64_t ra[2]; std::memcpy(ra,&ctx->r[31],16);
    p.bad |= ra[1]!=returnUpper;
    if(p.traceSize<p.trace.size()) p.trace[p.traceSize++]=event;
    else p.bad=true;
}
bool branch(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime,uint32_t target,uint32_t fallthrough,
            PS2Runtime::GuestBranchKind kind,const char *name) {
    const uint32_t source=ctx->pc;
    if(active->slow) return runtime->dispatchGuestBranchSlow(ram,ctx,target,source,fallthrough,kind,name);
    return runtime->dispatchGuestBranch(ram,ctx,target,source,fallthrough,kind,name);
}
void PS2_GUEST_CC finalEntry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(active->continuation?4u:7u,Final,ram,ctx,runtime);
    active->bad |= GPR_U32(ctx,31)!=active->pc[Final] || !matches(ctx->r[2],returnTwo);
    ctx->pc=0;
    runtime->eeScheduler().requestStop();
}
void PS2_GUEST_CC deferredEntry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(3,Deferred,ram,ctx,runtime);
    active->bad |= !active->continuation || GPR_U32(ctx,31)!=active->pc[Final] ||
        !matches(ctx->r[2],returnTwo);
    const bool continued=branch(ram,ctx,runtime,GPR_U32(ctx,31),0,PS2Runtime::GuestBranchKind::Return,"test deferred JR ra");
    active->bad |= continued;
}
// One owner function has two exact table entries; its incoming guest PC selects
// the owner prologue or a resumable interior entry. No C++ function is called
// directly by another test callback.
void PS2_GUEST_CC ownerEntry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    if(ctx->pc==active->pc[Alias]) {
        note(2,Alias,ram,ctx,runtime); ++active->aliasEntries;
        active->bad |= !active->continuation || GPR_U32(ctx,31)!=active->pc[Final];
        ctx->r[2]=asVector(returnTwo);
        const uint32_t target=GPR_U32(ctx,8);
        const bool continued=branch(ram,ctx,runtime,target,0,PS2Runtime::GuestBranchKind::IndirectJump,"test alias JR");
        active->bad |= continued || target!=active->pc[Deferred];
        return;
    }
    note(5,Owner,ram,ctx,runtime); ++active->ownerEntries;
    active->bad |= active->continuation || GPR_U32(ctx,31)!=active->pc[Alias] || !matches(ctx->r[2],returnOne);
    ctx->r[2]=asVector(returnTwo);
    // The ordinary generated JR-ra path stores PC then returns to its caller;
    // dispatchGuestBranch must recognize that caller's exact fallthrough.
    ctx->pc=GPR_U32(ctx,31);
}
void PS2_GUEST_CC tailEntry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(4,Tail,ram,ctx,runtime);
    active->bad |= active->continuation || GPR_U32(ctx,31)!=active->pc[Final] || !matches(ctx->r[2],returnOne);
    const uint32_t savedRa=GPR_U32(ctx,31);
    SET_GPR_U32(ctx,31,active->pc[Alias]);
    SET_GPR_U32(ctx,8,active->pc[Owner]);
    if(!branch(ram,ctx,runtime,GPR_U32(ctx,8),active->pc[Alias],PS2Runtime::GuestBranchKind::IndirectCall,"test JALR")) {
        active->bad=true; return;
    }
    note(6,Alias,ram,ctx,runtime);
    active->bad |= !matches(ctx->r[2],returnTwo);
    SET_GPR_U32(ctx,31,savedRa);
    const bool continued=branch(ram,ctx,runtime,savedRa,0,PS2Runtime::GuestBranchKind::Return,"test strict JR ra");
    active->bad |= continued;
}
void PS2_GUEST_CC calleeEntry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(2,Callee,ram,ctx,runtime);
    active->bad |= active->continuation || GPR_U32(ctx,31)!=active->pc[Deferred];
    ctx->r[2]=asVector(returnOne);
    ctx->pc=GPR_U32(ctx,31);
}
void PS2_GUEST_CC entry(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime) {
    note(1,Entry,ram,ctx,runtime);
    active->bad |= GPR_U32(ctx,31)!=active->pc[Final];
    if(active->continuation) {
        SET_GPR_U32(ctx,8,active->pc[Deferred]);
        SET_GPR_U32(ctx,9,active->pc[Alias]);
        if(!branch(ram,ctx,runtime,GPR_U32(ctx,9),active->pc[Final],PS2Runtime::GuestBranchKind::IndirectCall,"test interior JALR")) return;
        active->unexpectedInline=true;
        ctx->pc=0; runtime->eeScheduler().requestStop(); return;
    }
    const uint32_t savedRa=GPR_U32(ctx,31);
    SET_GPR_U32(ctx,31,active->pc[Deferred]);
    if(!branch(ram,ctx,runtime,active->pc[Callee],active->pc[Deferred],PS2Runtime::GuestBranchKind::DirectCall,"test JAL")) {
        active->bad=true; return;
    }
    note(3,Deferred,ram,ctx,runtime);
    active->bad |= !matches(ctx->r[2],returnOne);
    SET_GPR_U32(ctx,31,savedRa);
    const bool continued=branch(ram,ctx,runtime,active->pc[Tail],0,PS2Runtime::GuestBranchKind::DirectJump,"test tail J");
    active->bad |= continued;
}
static_assert(std::is_same_v<decltype(&entry),PS2Runtime::RecompiledFunction>);
static_assert(std::is_same_v<decltype(&calleeEntry),PS2Runtime::RecompiledFunction>);
static_assert(std::is_same_v<decltype(&tailEntry),PS2Runtime::RecompiledFunction>);
static_assert(std::is_same_v<decltype(&ownerEntry),PS2Runtime::RecompiledFunction>);
static_assert(std::is_same_v<decltype(&deferredEntry),PS2Runtime::RecompiledFunction>);
static_assert(std::is_same_v<decltype(&finalEntry),PS2Runtime::RecompiledFunction>);
}

int runGuestDispatchTests() {
    if(active) { std::puts("FAIL guest dispatch: nested test invocation"); return 1; }
    TableBlock slots;
    if(!slots.valid) { std::puts("FAIL guest dispatch: no empty synthetic block"); return 2; }
    auto runtime=std::make_unique<PS2Runtime>();
    std::vector<uint8_t> ram(PS2_RAM_SIZE,0);
    Probe probe; active=&probe;
    struct Clear { ~Clear() { active=nullptr; } } clear;
    for(unsigned i=0;i<RoleCount;++i) probe.pc[i]=slots.pc(Role(i));
    const std::array<PS2Runtime::RecompiledFunction,RoleCount> callbacks{
        entry,calleeEntry,tailEntry,ownerEntry,ownerEntry,deferredEntry,finalEntry,nullptr};
    for(unsigned i=0;i<Spare;++i) if(!runtime->registerFunction(probe.pc[i],callbacks[i])) return 3;
    if(runtime->lookupFunction(probe.pc[Owner])!=runtime->lookupFunction(probe.pc[Alias]) ||
       runtime->hasFunction(probe.pc[Spare]) || runtime->hasFunction(probe.pc[Owner]+1)) return 4;
    if(g_ps2RecompiledFunctionTableEnd<0x80000000u && runtime->hasFunction(probe.pc[Owner]|0x80000000u)) return 5;
    auto &ee=runtime->eeScheduler();
    for(unsigned slow=0;slow<2;++slow) for(unsigned continuation=0;continuation<2;++continuation) {
        const auto pc=probe.pc;
        probe=Probe{}; probe.pc=pc; probe.slow=slow; probe.continuation=continuation; probe.ram=ram.data(); probe.runtime=runtime.get();
        R5900Context context{};
        context.pc=pc[Entry]; context.r[4]=asVector(argument); context.r[16]=asVector(saved); context.r[30]=asVector(guard);
        uint64_t ra[2]={pc[Final],returnUpper}; std::memcpy(&context.r[31],ra,16);
        SET_GPR_U64((&context),29,guestStack);
        ee.reset(ram.data(),context);
        std::mutex mutex; std::condition_variable condition; bool finished=false,timedOut=false;
        std::thread watchdog([&] {
            std::unique_lock lock(mutex);
            if(!condition.wait_for(lock,std::chrono::seconds(2),[&]{return finished;})) { timedOut=true; ee.requestStop(); }
        });
        try { ee.run(); }
        catch(...) {
            { std::lock_guard lock(mutex); finished=true; }
            condition.notify_one(); watchdog.join(); throw;
        }
        { std::lock_guard lock(mutex); finished=true; }
        condition.notify_one(); watchdog.join();
        const unsigned expectedCount=continuation?4u:7u;
        bool traceGood=probe.traceSize==expectedCount;
        for(unsigned i=0;i<expectedCount;++i) traceGood &= probe.trace[i]==i+1;
        const uint64_t cycles=ee.snapshot().eeCycle;
        const uint64_t expectedCycles=(continuation?6u:7u)*EeScheduler::kGuestDispatchCycles;
        std::printf("[guest-dispatch] slow=%u continuation=%u trace=",slow,continuation);
        for(unsigned i=0;i<probe.traceSize;++i) std::printf("%u",probe.trace[i]);
        std::printf(" cycles=%llu owner=%u alias=%u timeout=%d\n",static_cast<unsigned long long>(cycles),probe.ownerEntries,probe.aliasEntries,timedOut);
        if(probe.bad || probe.unexpectedInline || timedOut || !traceGood || cycles!=expectedCycles ||
           probe.ownerEntries!=(continuation?0u:1u) || probe.aliasEntries!=(continuation?1u:0u) || ee.isExecutingGuest()) return 6;
    }
    slots.restore();
    for(unsigned i=0;i<RoleCount;++i) if(g_ps2RecompiledFunctionTable[slots.first+i]!=nullptr) return 7;
    std::puts("PASS guest dispatch: direct/indirect calls, tail/return, exact owner aliases, deferred continuation, 128-bit registers; empty slots restored");
    return 0;
}
