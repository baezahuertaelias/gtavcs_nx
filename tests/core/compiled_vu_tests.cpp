// Public synthetic programs compiled by the real pinned upstream VU generator.
// These tests neither load nor extract game microcode.
#include "ps2_runtime.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/ps2_memory.h"
// Exercise the same strict generator policy as __SWITCH__ on the native host.
#define PS2X_VU1_COMPILED_PRECISE 1
#include "synthetic_vu_fixture.inc"
#include <array>
#include <bit>
#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace {
int changeEnv(const char *key,const char *value)
{
#ifdef _WIN32
    return _putenv_s(key,value ? value : "");
#else
    return value ? setenv(key,value,1) : unsetenv(key);
#endif
}
struct ScopedEnv {
    const char *key; std::string previous; bool present; bool good;
    ScopedEnv(const char *name,const char *value):key(name),present(std::getenv(name)!=nullptr) {
        if(present) previous=std::getenv(name);
        good=changeEnv(name,value)==0;
    }
    ~ScopedEnv() { changeEnv(key,present ? previous.c_str() : nullptr); }
};
uint32_t floatBits(float v) { return std::bit_cast<uint32_t>(v); }

VU1State initialState()
{
    VU1State state{};
    state.vf[0][3]=1;
    state.q=1;
    for(unsigned lane=0;lane<4;++lane) { state.acc[lane]=-1; state.vf[6][lane]=-1; }
    const uint32_t a[]{0x3f800001u,0x7f7fffffu,0x80000001u,0xff800000u};
    const uint32_t b[]{0x3f7ffffeu,0x40000000u,0x3f800000u,0x3f000000u};
    for(unsigned lane=0;lane<4;++lane) {
        state.vf[1][lane]=std::bit_cast<float>(a[lane]);
        state.vf[2][lane]=std::bit_cast<float>(b[lane]);
    }
    const float values[]{3,4,0,1};
    std::memcpy(state.vf[10],values,sizeof(values));
    state.vf[11][0]=2;
    state.vi[7]=12;
    return state;
}

struct Result {
    VU1State state{};
    std::array<uint8_t,PS2_VU1_DATA_SIZE> data{};
    std::vector<std::vector<uint8_t>> packets;
    uint64_t calls=0,pairs=0;
    bool roundingRestored=false;
};

struct Fixture {
    PS2Memory memory;
    GS gs;
    std::unique_ptr<VU1Interpreter> vu;
    std::vector<std::vector<uint8_t>> packets;

    bool initialize(unsigned piece,bool compiled,bool mutate=false)
    {
        if(!memory.initialize()) return false;
        gs.init(memory.getGSVRAM(),static_cast<uint32_t>(PS2_GS_VRAM_SIZE),&memory.gs());
        memory.setGifPacketCallback([this](const uint8_t *data,uint32_t size) {
            packets.emplace_back(data,data+size);
        });
        const auto &program=vu1c_pieces_t[piece];
        // Use tracked writes so a later byte mismatch invalidates cached
        // decoded/compiled identity through the same real memory generation.
        for(unsigned pair=0;pair<program.pairCount;++pair) {
            uint64_t words=uint64_t(program.code[2*pair]) | uint64_t(program.code[2*pair+1])<<32;
            if(mutate && pair==0) words=(words&~uint64_t(0x7ff))|5u;
            memory.write64(PS2_VU1_CODE_BASE+program.vuAddress+pair*8,words);
        }
        const uint64_t tag=0x1000000000008001ull; // one PACKED A+D payload, EOP
        const uint64_t descriptor=0x0e;
        std::memcpy(memory.getVU1Data()+12*16,&tag,8);
        std::memcpy(memory.getVU1Data()+12*16+8,&descriptor,8);
        ScopedEnv enabled("PS2X_VU1_COMPILED",compiled ? "1" : "0");
        if(!enabled.good) return false;
        vu=std::make_unique<VU1Interpreter>();
        vu->state()=initialState();
        return true;
    }

    Result run(unsigned piece,uint32_t budget,int rounding,bool resume=false)
    {
        Result result;
        const int old=std::fegetround();
        if(old<0 || std::fesetround(rounding)!=0) return result;
        const auto beforeCalls=nxSyntheticVuCompiledCalls;
        const auto beforePairs=nxSyntheticVuCompiledPairs;
        if(resume)
            vu->resume(memory.getVU1Code(),PS2_VU1_CODE_SIZE,memory.getVU1Data(),
                PS2_VU1_DATA_SIZE,gs,&memory,0x123,0x234,budget);
        else
            vu->execute(memory.getVU1Code(),PS2_VU1_CODE_SIZE,memory.getVU1Data(),
                PS2_VU1_DATA_SIZE,gs,&memory,vu1c_pieces_t[piece].vuAddress,0x123,0x234,budget);
        result.roundingRestored=std::fegetround()==rounding;
        if(std::fesetround(old)!=0) result.roundingRestored=false;
        result.calls=nxSyntheticVuCompiledCalls-beforeCalls;
        result.pairs=nxSyntheticVuCompiledPairs-beforePairs;
        result.state=vu->state();
        std::memcpy(result.data.data(),memory.getVU1Data(),result.data.size());
        result.packets=packets;
        return result;
    }
};

bool sameState(const VU1State &a,const VU1State &b,const char *name)
{
    for(unsigned reg=0;reg<32;++reg) for(unsigned lane=0;lane<4;++lane)
        if(floatBits(a.vf[reg][lane])!=floatBits(b.vf[reg][lane])) {
            std::printf("FAIL compiled VU %s vf%u.%u ref=%08x compiled=%08x\n",name,reg,lane,
                floatBits(a.vf[reg][lane]),floatBits(b.vf[reg][lane])); return false;
        }
    for(unsigned reg=0;reg<16;++reg) if(a.vi[reg]!=b.vi[reg]) {
        std::printf("FAIL compiled VU %s vi%u ref=%d compiled=%d\n",name,reg,a.vi[reg],b.vi[reg]); return false;
    }
    for(unsigned lane=0;lane<4;++lane) if(floatBits(a.acc[lane])!=floatBits(b.acc[lane])) return false;
#define SAME(field) if(a.field!=b.field) { std::printf("FAIL compiled VU %s field=%s ref=%llu compiled=%llu\n",name,#field,static_cast<unsigned long long>(a.field),static_cast<unsigned long long>(b.field)); return false; }
    SAME(r); SAME(pc); SAME(mac); SAME(clip); SAME(status); SAME(cycles);
    SAME(ebit); SAME(haltAfterDelaySlot); SAME(dBitEnabled); SAME(tBitEnabled);
    SAME(stoppedByD); SAME(stoppedByT); SAME(top); SAME(itop);
    SAME(branchPending);
    // A completed branch leaves an unused cached target in the interpreter;
    // generated direct jumps need not retain it. Pending branches must agree.
    if(a.branchPending) { SAME(branchTarget); SAME(branchDelay); }
#undef SAME
    return floatBits(a.q)==floatBits(b.q) && floatBits(a.p)==floatBits(b.p) && floatBits(a.i)==floatBits(b.i);
}

bool compare(const Result &reference,const Result &compiled,const char *name,bool expectCompiled=true,bool allowZeroPairs=false)
{
    if(reference.calls!=0 || reference.pairs!=0 || !reference.roundingRestored || !compiled.roundingRestored)
        { std::printf("FAIL compiled VU %s reference selection/rounding\n",name); return false; }
    if(expectCompiled ? (compiled.calls==0 || (!allowZeroPairs && compiled.pairs==0)) : (compiled.calls!=0 || compiled.pairs!=0))
        { std::printf("FAIL compiled VU %s selection calls=%llu pairs=%llu\n",name,
            static_cast<unsigned long long>(compiled.calls),static_cast<unsigned long long>(compiled.pairs)); return false; }
    if(!sameState(reference.state,compiled.state,name)) return false;
    if(reference.data!=compiled.data || reference.packets!=compiled.packets)
        { std::printf("FAIL compiled VU %s data/GIF packet mismatch\n",name); return false; }
    return true;
}

bool runOne(unsigned piece,uint32_t budget,int rounding,Result &out,bool compiled,bool mutate=false)
{
    auto fx=std::make_unique<Fixture>();
    if(!fx->initialize(piece,compiled,mutate)) return false;
    out=fx->run(piece,budget,rounding);
    return true;
}
}

int runCompiledVuTests() try
{
    ScopedEnv fast("PS2X_VU1_FAST","1");
    // The runtime's optional comparison mode changes the reference model and
    // packet capture; this suite performs its own exact fast-vs-compiled check.
    ScopedEnv verify("PS2X_VU1_VERIFY",nullptr);
    if(!fast.good || !verify.good) return 1;
    static bool registered=false;
    if(!registered) { nxRegisterSyntheticVuPieces(); registered=true; }
    unsigned cases=0; uint64_t calls=0,pairs=0;
    for(unsigned piece=0;piece<std::size(vu1c_pieces_t);++piece) {
        for(int rounding : {FE_TONEAREST,FE_TOWARDZERO}) {
            Result reference,compiled;
            if(!runOne(piece,128,rounding,reference,false) ||
                !runOne(piece,128,rounding,compiled,true) ||
                !compare(reference,compiled,vu1c_pieces_t[piece].name)) return 2;
            if(piece==0 && (floatBits(compiled.state.vf[3][0])!=0xb3800000u ||
                floatBits(compiled.state.vf[5][0])!=0xb3800000u || compiled.state.clip!=5)) {
                std::puts("FAIL compiled VU non-fused/toward-zero arithmetic witness"); return 3;
            }
            if(piece==1 && (compiled.state.vi[1]!=0 || compiled.state.vi[2]!=3 || compiled.state.vi[4]!=1)) {
                std::puts("FAIL compiled VU branch/JR delay witnesses"); return 4;
            }
            if(piece==2 && (compiled.state.q!=1.5f || compiled.state.p!=25.0f || compiled.state.vf[13][0]!=25.0f)) {
                std::puts("FAIL compiled VU Q/P witnesses"); return 5;
            }
            if(piece==3 && (compiled.packets.size()!=1 || compiled.packets[0].size()!=32 ||
                std::memcmp(compiled.packets[0].data()+16,compiled.state.vf[10],16)!=0)) {
                std::puts("FAIL compiled VU XGKICK store/packet witness"); return 6;
            }
            if(piece==5 && (floatBits(compiled.state.q)!=0x7f7fffffu ||
                (compiled.state.status&0xc30u)!=0x820u)) {
                std::puts("FAIL compiled VU current/sticky FDIV flags witness"); return 14;
            }
            ++cases; calls+=compiled.calls; pairs+=compiled.pairs;
        }
    }
    // Check early budget exits and repeated resume while preserving pending
    // state. Neither side may silently run to completion past these budgets.
    auto reference=std::make_unique<Fixture>(); auto compiled=std::make_unique<Fixture>();
    if(!reference->initialize(0,false) || !compiled->initialize(0,true)) return 7;
    for(unsigned step=0;step<5;++step) {
        const auto a=reference->run(0,1,FE_TONEAREST,step!=0);
        const auto b=compiled->run(0,1,FE_TONEAREST,step!=0);
        // A one-cycle budget can be consumed entirely by a real pipeline stall.
        // The generated entry must be selected; an instruction need not issue.
        if(!compare(a,b,"budget/resume",true,true)) return 8;
        ++cases; calls+=b.calls; pairs+=b.pairs;
    }
    reference.reset(); compiled.reset();
    Result normal,changed;
    if(!runOne(4,128,FE_TONEAREST,normal,false) ||
        !runOne(4,128,FE_TONEAREST,changed,false,true) || changed.state.vi[1]!=5) return 9;
    auto identity=std::make_unique<Fixture>();
    if(!identity->initialize(4,true)) return 10;
    auto initial=identity->run(4,128,FE_TONEAREST);
    if(!compare(normal,initial,"identity baseline")) return 11;
    const auto &program=vu1c_pieces_t[4];
    const uint64_t original=uint64_t(program.code[0]) | uint64_t(program.code[1])<<32;
    identity->memory.write64(PS2_VU1_CODE_BASE+program.vuAddress,(original&~uint64_t(0x7ff))|5u);
    identity->vu->state()=initialState();
    const auto fallback=identity->run(4,128,FE_TONEAREST);
    // execute() resets instruction pipelines, but this interpreter's internal
    // cycle clock continues across programs. Preserve that real clock contract.
    auto changedAgain=changed;
    changedAgain.state.cycles+=initial.state.cycles;
    if(!compare(changedAgain,fallback,"tracked identity mismatch fallback",false)) return 12;
    identity->memory.write64(PS2_VU1_CODE_BASE+program.vuAddress,original);
    identity->vu->state()=initialState();
    const auto restored=identity->run(4,128,FE_TONEAREST);
    auto normalAgain=normal;
    normalAgain.state.cycles+=fallback.state.cycles;
    if(!compare(normalAgain,restored,"restored identity reselects compiled")) return 13;
    cases+=3; calls+=initial.calls+restored.calls; pairs+=initial.pairs+restored.pairs;
    std::printf("PASS compiled VU: public_programs=%zu cases=%u generated_calls=%llu generated_pairs=%llu identity_fallback_calls=%llu rounding/budgets/branches/Q/P/XGKICK\n",
        std::size(vu1c_pieces_t),cases,static_cast<unsigned long long>(calls),static_cast<unsigned long long>(pairs),
        static_cast<unsigned long long>(fallback.calls));
    return 0;
}
catch(const std::exception &error) {
    std::printf("FAIL compiled VU resource/runtime exception: %s\n",error.what());
    return 15;
}
catch(...) {
    std::puts("FAIL compiled VU unknown runtime exception");
    return 16;
}

#ifndef NX_CORE_TEST_SUITE
int main() { return runCompiledVuTests(); }
#endif
