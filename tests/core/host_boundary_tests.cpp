// Public ABI/layout/concurrency checks. No generated game code or game data.
#include "ps2_runtime_macros.h"
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>

namespace {
struct Layout { const char *name; size_t actual; size_t native; };
// Values are emitted from the isolated pinned x86 Clang reference, not ARM.
static constexpr Layout layouts[] = {
    {"R5900Context.size",sizeof(R5900Context),1648},
    {"R5900Context.align",alignof(R5900Context),16},
    {"R5900Context.r",offsetof(R5900Context,r),0},
    {"R5900Context.pc",offsetof(R5900Context,pc),512},
    {"R5900Context.insn_count",offsetof(R5900Context,insn_count),520},
    {"R5900Context.hi",offsetof(R5900Context,hi),528},
    {"R5900Context.lo1",offsetof(R5900Context,lo1),552},
    {"R5900Context.sa",offsetof(R5900Context,sa),560},
    {"R5900Context.vu0_vf",offsetof(R5900Context,vu0_vf),576},
    {"R5900Context.vi",offsetof(R5900Context,vi),1088},
    {"R5900Context.vu0_q",offsetof(R5900Context,vu0_q),1120},
    {"R5900Context.vu0_r",offsetof(R5900Context,vu0_r),1136},
    {"R5900Context.vu0_acc",offsetof(R5900Context,vu0_acc),1152},
    {"R5900Context.vu0_status",offsetof(R5900Context,vu0_status),1168},
    {"R5900Context.cop0_status",offsetof(R5900Context,cop0_status),1320},
    {"R5900Context.llbit",offsetof(R5900Context,llbit),1364},
    {"R5900Context.in_delay_slot",offsetof(R5900Context,in_delay_slot),1372},
    {"R5900Context.branch_pc",offsetof(R5900Context,branch_pc),1376},
    {"R5900Context.cop2_ccr",offsetof(R5900Context,cop2_ccr),1380},
    {"R5900Context.f",offsetof(R5900Context,f),1508},
    {"R5900Context.f_acc",offsetof(R5900Context,f_acc),1636},
    {"R5900Context.fcr31",offsetof(R5900Context,fcr31),1640},
    {"VU1State.size",sizeof(VU1State),664},
    {"VU1State.align",alignof(VU1State),8},
    {"VU1State.vf",offsetof(VU1State,vf),0},
    {"VU1State.vi",offsetof(VU1State,vi),512},
    {"VU1State.acc",offsetof(VU1State,acc),576},
    {"VU1State.q",offsetof(VU1State,q),592},
    {"VU1State.r",offsetof(VU1State,r),604},
    {"VU1State.pc",offsetof(VU1State,pc),608},
    {"VU1State.status",offsetof(VU1State,status),620},
    {"VU1State.cycles",offsetof(VU1State,cycles),624},
    {"VU1State.ebit",offsetof(VU1State,ebit),632},
    {"VU1State.top",offsetof(VU1State,top),640},
    {"VU1State.branchPending",offsetof(VU1State,branchPending),648},
    {"VU1State.branchTarget",offsetof(VU1State,branchTarget),652},
    {"VU1State.branchDelay",offsetof(VU1State,branchDelay),656},
    {"guest.integer_vector.size",sizeof(__m128i),16},
    {"guest.integer_vector.align",alignof(__m128i),16},
    {"guest.float_vector.size",sizeof(__m128),16},
    {"guest.float_vector.align",alignof(__m128),16},
    {"guest.callback.size",sizeof(PS2Runtime::RecompiledFunction),8},
};
static_assert(std::is_standard_layout_v<R5900Context>);
static_assert(std::is_standard_layout_v<VU1State>);
static_assert(std::is_trivially_copyable_v<__m128i>);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
#ifndef NX_HOST_EMIT_LAYOUT
static_assert([] {
    for(const auto &row:layouts) if(row.actual!=row.native) return false;
    return true;
}(),"Guest context/vector layout differs from the recorded native reference");
#endif
using Clock = std::chrono::steady_clock;
thread_local uint64_t tlsCookie = 0;

struct GuestCallState {
    uint8_t *ram;
    R5900Context *context;
    PS2Runtime *runtime;
    uint64_t calls = 0;
    bool failure = false;
};
GuestCallState callState{};
// Function pointers carry exactly the same convention as generated entries.
__attribute__((noinline)) void PS2_GUEST_CC guestBoundary(
    uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime)
{
    alignas(16) volatile uint64_t stackGuard[2] = {0x123456789abcdef0ull,0xfedcba9876543210ull};
    // The frame address reflects the stack pointer the caller delivered; alignas alone cannot fail.
    if(ram != callState.ram || ctx != callState.context || runtime != callState.runtime ||
        (reinterpret_cast<uintptr_t>(__builtin_frame_address(0)) & 15u) != 0u ||
        (reinterpret_cast<uintptr_t>(&stackGuard) & 15u) != 0u) {
        callState.failure=true; return;
    }
    const uint64_t n=++callState.calls;
    ctx->r[4]=_mm_set_epi64x(static_cast<int64_t>(~n),static_cast<int64_t>(n));
    ctx->pc=static_cast<uint32_t>(n*4u);
    std::memcpy(ram+3,&n,sizeof(n)); // deliberately unaligned, alias-safe
    if(stackGuard[0] != 0x123456789abcdef0ull || stackGuard[1] != 0xfedcba9876543210ull)
        callState.failure=true;
}
static_assert(std::is_same_v<decltype(&guestBoundary),PS2Runtime::RecompiledFunction>);

bool valueBoundaries()
{
    if(std::endian::native != std::endian::little || char(-1)>=0 ||
        sizeof(void *)!=8 || sizeof(uint64_t)!=8) return false;
    const std::array<uint8_t,16> bytes{0x80,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                                      0xff,0xfe,0xfd,0xfc,0xfb,0xfa,0xf9,0xf8};
    const __m128i raw=std::bit_cast<__m128i>(bytes);
    const __m128 floating=_mm_castsi128_ps(raw);
    if(std::bit_cast<std::array<uint8_t,16>>(_mm_castps_si128(floating))!=bytes) return false;
    uint64_t lo=0,hi=0; std::memcpy(&lo,bytes.data(),8); std::memcpy(&hi,bytes.data()+8,8);
    if(lo!=0x0706050403020180ull || hi!=0xf8f9fafbfcfdfeffull ||
        static_cast<uint64_t>(Ps2ExtractEpi64(raw,0))!=lo ||
        static_cast<uint64_t>(Ps2ExtractEpi64(raw,1))!=hi) return false;
    R5900Context ctx;
    const uint32_t words[]{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu};
    for(uint32_t word:words) {
        ctx.r[7]=raw;
        const uint64_t signedWord = (word&0x80000000u) ? (0xffffffff00000000ull|word) : word;
        SET_GPR_S32((&ctx),7,std::bit_cast<int32_t>(word));
        if(GPR_U64((&ctx),7)!=signedWord ||
            static_cast<uint64_t>(Ps2ExtractEpi64(ctx.r[7],1))!=hi) return false;
        SET_GPR_ZE32((&ctx),7,word);
        if(GPR_U64((&ctx),7)!=word ||
            static_cast<uint64_t>(Ps2ExtractEpi64(ctx.r[7],1))!=hi) return false;
        SET_GPR_U32((&ctx),7,word); // the runtime's ABI word writes sign-extend
        if(GPR_U64((&ctx),7)!=signedWord) return false;
        SET_GPR_S64((&ctx),7,std::bit_cast<int64_t>(lo));
        if(GPR_U64((&ctx),7)!=lo || GPR_U32((&ctx),7)!=uint32_t(lo)) return false;
    }
    SET_GPR_U64((&ctx),0,~uint64_t(0));
    if(GPR_U64((&ctx),0)!=0) return false;
    alignas(16) std::array<uint8_t,32> ram{};
    uint64_t runtimeToken=0; // opaque pointer identity only; never dereferenced
    callState={ram.data(),&ctx,reinterpret_cast<PS2Runtime *>(&runtimeToken)};
    PS2Runtime::RecompiledFunction volatile entry=&guestBoundary;
    for(uint64_t n=1;n<=4096;++n) {
        entry(ram.data(),&ctx,callState.runtime);
        uint64_t stored=0; std::memcpy(&stored,ram.data()+3,8);
        if(stored!=n || ctx.pc!=n*4u || GPR_U64((&ctx),4)!=n ||
            static_cast<uint64_t>(Ps2ExtractEpi64(ctx.r[4],1))!=~n) return false;
    }
    return !callState.failure && callState.calls==4096;
}

bool exceptionsAndAlignment()
{
    struct Unwind { int &count; ~Unwind() { ++count; } };
    struct Marker { uint64_t cookie; };
    int destroyed=0; bool caught=false;
    try {
        Unwind outer{destroyed};
        try { Unwind inner{destroyed}; throw Marker{0x123456789abcdef0ull}; }
        catch(const Marker &e) { if(e.cookie!=0x123456789abcdef0ull || destroyed!=1) return false; throw; }
    } catch(const Marker &e) { caught=e.cookie==0x123456789abcdef0ull && destroyed==2; }
    if(!caught) return false;
    for(size_t alignment : {16u,32u,64u,256u,4096u}) {
        void *p=::operator new(257,std::align_val_t(alignment),std::nothrow);
        if(!p) return false;
        const bool aligned=(reinterpret_cast<uintptr_t>(p)&(alignment-1u))==0;
        std::memset(p,0x5a,257);
        const bool written=static_cast<uint8_t *>(p)[256]==0x5a;
        ::operator delete(p,std::align_val_t(alignment));
        if(!aligned || !written) return false;
    }
    return true;
}

bool producerConsumer()
{
    struct Payload { uint64_t sequence,complement,product; } payload{};
    std::atomic<uint32_t> ready{0},ack{0};
    std::atomic<bool> abort{false},valid{true};
    const auto deadline=Clock::now()+std::chrono::seconds(5);
    const auto await=[&](std::atomic<uint32_t> &value,uint32_t target) {
        while(value.load(std::memory_order_acquire)!=target) {
            if(abort.load(std::memory_order_relaxed) || Clock::now()>=deadline) {
                abort.store(true,std::memory_order_relaxed); return false;
            }
            std::this_thread::yield();
        }
        return true;
    };
    tlsCookie=0xaaaabbbbccccddddull;
    std::thread producer,consumer;
    try {
        producer=std::thread([&] {
            if(tlsCookie!=0) valid.store(false);
            tlsCookie=0x1111222233334444ull;
            for(uint32_t n=1;n<=20000;++n) {
                if(!await(ack,n-1)) return;
                payload={n,~uint64_t(n),uint64_t(n)*0x9e3779b9u};
                ready.store(n,std::memory_order_release);
            }
            if(tlsCookie!=0x1111222233334444ull) valid.store(false);
        });
        consumer=std::thread([&] {
            if(tlsCookie!=0) valid.store(false);
            tlsCookie=0x5555666677778888ull;
            for(uint32_t n=1;n<=20000;++n) {
                if(!await(ready,n)) return;
                if(payload.sequence!=n || payload.complement!=~uint64_t(n) ||
                    payload.product!=uint64_t(n)*0x9e3779b9u) valid.store(false);
                ack.store(n,std::memory_order_release);
            }
            if(tlsCookie!=0x5555666677778888ull) valid.store(false);
        });
    } catch(const std::exception &) { abort.store(true); valid.store(false); }
    if(producer.joinable()) producer.join();
    if(consumer.joinable()) consumer.join();
    return valid.load() && !abort.load() && ack.load()==20000 &&
           tlsCookie==0xaaaabbbbccccddddull;
}

bool mutexAndWaits()
{
    std::mutex mutex; std::condition_variable cv;
    bool released=false; std::atomic<bool> waiting{false},observed{false};
    std::thread worker([&] {
        try {
            std::unique_lock<std::mutex> lock(mutex);
            waiting.store(true,std::memory_order_release);
            observed.store(cv.wait_for(lock,std::chrono::seconds(2),[&]{return released;}));
        } catch(const std::exception &) { observed.store(false); }
    });
    const auto deadline=Clock::now()+std::chrono::seconds(1);
    while(!waiting.load(std::memory_order_acquire) && Clock::now()<deadline)
        std::this_thread::yield();
    { std::lock_guard<std::mutex> lock(mutex); released=true; }
    cv.notify_one(); worker.join();
    if(!observed.load()) return false;
    std::unique_lock<std::mutex> lock(mutex);
    const auto start=Clock::now();
    const bool impossible=cv.wait_for(lock,std::chrono::milliseconds(5),[]{return false;});
    const auto elapsed=Clock::now()-start;
    return !impossible && elapsed>=std::chrono::milliseconds(4) && elapsed<std::chrono::seconds(2);
}
}

#ifdef NX_HOST_EMIT_LAYOUT
#if defined(__aarch64__)
#error "Emit layout values only from the native x86 reference"
#endif
int main() {
    for(const auto &row:layouts) std::printf("%s=%zu\n",row.name,row.actual);
    return 0;
}
#else
int runHostBoundaryTests()
{
    for(const auto &row:layouts) {
        if(row.actual!=row.native) {
            std::printf("FAIL layout %s native=%zu actual=%zu\n",row.name,row.native,row.actual); return 1;
        }
    }
    if(!valueBoundaries()) { std::puts("FAIL register/endian/bitcast/guest-call boundaries"); return 2; }
    if(!exceptionsAndAlignment()) { std::puts("FAIL exception/unwind/aligned-new boundaries"); return 3; }
    if(!producerConsumer()) { std::puts("FAIL acquire/release producer-consumer or TLS isolation"); return 4; }
    try {
        if(!mutexAndWaits()) { std::puts("FAIL mutex/condition-variable signal or timeout"); return 5; }
    } catch(const std::exception &e) {
        std::printf("FAIL host wait thread creation: %s\n",e.what()); return 6;
    }
    std::printf("PASS host boundaries: layouts=%zu guest_calls=4096 messages=20000 workers_peak=2 TLS/unwind/aligned-new/waits\n",std::size(layouts));
    return 0;
}
#ifndef NX_CORE_TEST_SUITE
int main() { return runHostBoundaryTests(); }
#endif
#endif
