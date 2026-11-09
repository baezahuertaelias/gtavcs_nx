// Public runtime operations checked against scalar, byte-addressed models.
// This contains synthetic test inputs only; no game code or disc payloads.
#include "ps2_runtime_macros.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

using Bytes = std::array<uint8_t, 16>;
enum class Kind { Add, Sub, Eq, Gt, SAdd, SSub, UAdd, USub, Abs, MulLo, MulHiSigned,
                  MulHiUnsigned, MinSigned, MaxSigned, And, Or, Xor, AndNot, Shl, Shr, Sar };
struct Operation { const char *name; Kind kind; unsigned width; };
static constexpr Operation operations[] = {
    {"add8",Kind::Add,1},{"add16",Kind::Add,2},{"add32",Kind::Add,4},
    {"sub8",Kind::Sub,1},{"sub16",Kind::Sub,2},{"sub32",Kind::Sub,4},
    {"equal8",Kind::Eq,1},{"equal16",Kind::Eq,2},{"equal32",Kind::Eq,4},
    {"signed_gt8",Kind::Gt,1},{"signed_gt16",Kind::Gt,2},{"signed_gt32",Kind::Gt,4},
    {"signed_saturating_add8",Kind::SAdd,1},{"signed_saturating_add16",Kind::SAdd,2},
    {"signed_saturating_add32",Kind::SAdd,4},{"signed_saturating_sub8",Kind::SSub,1},
    {"signed_saturating_sub16",Kind::SSub,2},{"signed_saturating_sub32",Kind::SSub,4},
    {"unsigned_saturating_add8",Kind::UAdd,1},{"unsigned_saturating_add16",Kind::UAdd,2},
    {"unsigned_saturating_add32",Kind::UAdd,4},{"unsigned_saturating_sub8",Kind::USub,1},
    {"unsigned_saturating_sub16",Kind::USub,2},{"unsigned_saturating_sub32",Kind::USub,4},
    {"vector_absolute8",Kind::Abs,1},{"vector_absolute16",Kind::Abs,2},{"vector_absolute32",Kind::Abs,4},
    {"multiply_low16",Kind::MulLo,2},{"multiply_low32",Kind::MulLo,4},
    {"multiply_high_signed16",Kind::MulHiSigned,2},{"multiply_high_unsigned16",Kind::MulHiUnsigned,2},
    {"minimum_signed16",Kind::MinSigned,2},{"minimum_signed32",Kind::MinSigned,4},
    {"maximum_signed16",Kind::MaxSigned,2},{"maximum_signed32",Kind::MaxSigned,4},
    {"and",Kind::And,4},{"or",Kind::Or,4},{"xor",Kind::Xor,4},{"and_not",Kind::AndNot,4},
    {"variable_left32",Kind::Shl,4},{"variable_logical_right32",Kind::Shr,4},
    {"variable_arithmetic_right32",Kind::Sar,4},
};

template<class T> T lane(const Bytes &bytes, unsigned index)
{
    T value; std::memcpy(&value, bytes.data() + index * sizeof(T), sizeof(T)); return value;
}
template<class T> void put(Bytes &bytes, unsigned index, T value)
{
    std::memcpy(bytes.data() + index * sizeof(T), &value, sizeof(T));
}
static __m128i vectorOf(const Bytes &bytes)
{
    __m128i value; std::memcpy(&value, bytes.data(), 16); return value;
}
static Bytes bytesOf(__m128i value)
{
    Bytes bytes; std::memcpy(bytes.data(), &value, 16); return bytes;
}
template<class U> static Bytes scalar(const Operation &op, const Bytes &a, const Bytes &b)
{
    Bytes out{};
#ifdef NX_PMFHL_ARCHITECTURAL
    if(op.kind==Kind::Shl || op.kind==Kind::Shr || op.kind==Kind::Sar) {
        // Guest rs supplies the counts; rt supplies even source words. Each
        // resulting word is sign-extended independently into a 64-bit lane.
        for(unsigned bank=0;bank<2;++bank) {
            const unsigned n=lane<uint32_t>(a,bank*2)&31u;
            const uint32_t x=lane<uint32_t>(b,bank*2);
            uint32_t y=op.kind==Kind::Shl ? x<<n : x>>n;
            if(op.kind==Kind::Sar && n && (x&0x80000000u)) y|=UINT32_MAX<<(32-n);
            put(out,bank,uint64_t(int64_t(std::bit_cast<int32_t>(y))));
        }
        return out;
    }
#endif
    for (unsigned i = 0; i < 16 / sizeof(U); ++i) {
        const U x = lane<U>(a,i), y = lane<U>(b,i);
        U result = 0;
        using S = std::make_signed_t<U>;
        switch (op.kind) {
        case Kind::Add: result = static_cast<U>(x + y); break;
        case Kind::Sub: result = static_cast<U>(x - y); break;
        case Kind::Eq: result = x == y ? static_cast<U>(~U{0}) : 0; break;
        case Kind::Gt: result = std::bit_cast<S>(x) > std::bit_cast<S>(y) ? static_cast<U>(~U{0}) : 0; break;
        case Kind::SAdd: case Kind::SSub: {
            const int64_t left = std::bit_cast<S>(x);
            const int64_t right = std::bit_cast<S>(y);
            const int64_t value = op.kind == Kind::SAdd ? left + right : left - right;
            result = static_cast<U>(std::clamp<int64_t>(value, std::numeric_limits<S>::min(), std::numeric_limits<S>::max())); break;
        }
        case Kind::UAdd: result = static_cast<U>(std::min<uint64_t>(uint64_t(x) + y, std::numeric_limits<U>::max())); break;
        case Kind::USub: result = x >= y ? static_cast<U>(x - y) : 0; break;
        case Kind::Abs: result = std::bit_cast<S>(x) < 0 ? static_cast<U>(U{0} - x) : x; break;
        case Kind::MulLo: result = static_cast<U>(uint64_t(x) * y); break;
        case Kind::MulHiSigned: {
            const int64_t product = int64_t(std::bit_cast<S>(x)) * std::bit_cast<S>(y);
            result = static_cast<U>(static_cast<uint64_t>(product) >> (8*sizeof(U))); break;
        }
        case Kind::MulHiUnsigned: result = static_cast<U>((uint64_t(x) * y) >> (8*sizeof(U))); break;
        case Kind::MinSigned: result = std::bit_cast<S>(x) < std::bit_cast<S>(y) ? x : y; break;
        case Kind::MaxSigned: result = std::bit_cast<S>(x) > std::bit_cast<S>(y) ? x : y; break;
        case Kind::And: result = x & y; break;
        case Kind::Or: result = x | y; break;
        case Kind::Xor: result = x ^ y; break;
        case Kind::AndNot: result = static_cast<U>(~x) & y; break;
        case Kind::Shl: result = static_cast<U>(uint32_t(x) << (y & 31u)); break;
        case Kind::Shr: result = static_cast<U>(uint32_t(x) >> (y & 31u)); break;
        case Kind::Sar: {
            const unsigned count = y & 31u;
            uint32_t value = uint32_t(x) >> count;
            if (count && (uint32_t(x) & 0x80000000u)) value |= UINT32_MAX << (32 - count);
            result = static_cast<U>(value); break;
        }
        }
        put(out,i,result);
    }
    return out;
}

static __m128i actual(const Operation &op, __m128i a, __m128i b)
{
    switch (op.kind) {
    case Kind::Add: return op.width == 1 ? _mm_add_epi8(a,b) : op.width == 2 ? _mm_add_epi16(a,b) : _mm_add_epi32(a,b);
    case Kind::Sub: return op.width == 1 ? _mm_sub_epi8(a,b) : op.width == 2 ? _mm_sub_epi16(a,b) : _mm_sub_epi32(a,b);
    case Kind::Eq: return op.width == 1 ? PS2_PCEQB(a,b) : op.width == 2 ? PS2_PCEQH(a,b) : PS2_PCEQW(a,b);
    case Kind::Gt: return op.width == 1 ? PS2_PCGTB(a,b) : op.width == 2 ? PS2_PCGTH(a,b) : PS2_PCGTW(a,b);
    case Kind::SAdd: return op.width == 1 ? _mm_adds_epi8(a,b) : op.width == 2 ? _mm_adds_epi16(a,b) : PS2_PADDSW(a,b);
    case Kind::SSub: return op.width == 1 ? _mm_subs_epi8(a,b) : op.width == 2 ? _mm_subs_epi16(a,b) : PS2_PSUBSW(a,b);
    case Kind::UAdd: return op.width == 1 ? _mm_adds_epu8(a,b) : op.width == 2 ? _mm_adds_epu16(a,b) : ps2_paddu32(a,b);
    case Kind::USub: return op.width == 1 ? _mm_subs_epu8(a,b) : op.width == 2 ? _mm_subs_epu16(a,b) : ps2_psubu32(a,b);
    case Kind::Abs: return op.width == 1 ? _mm_abs_epi8(a) : op.width == 2 ? _mm_abs_epi16(a) : _mm_abs_epi32(a);
    case Kind::MulLo: return op.width == 2 ? _mm_mullo_epi16(a,b) : _mm_mullo_epi32(a,b);
    case Kind::MulHiSigned: return _mm_mulhi_epi16(a,b);
    case Kind::MulHiUnsigned: return _mm_mulhi_epu16(a,b);
    case Kind::MinSigned: return op.width == 2 ? _mm_min_epi16(a,b) : _mm_min_epi32(a,b);
    case Kind::MaxSigned: return op.width == 2 ? _mm_max_epi16(a,b) : _mm_max_epi32(a,b);
    case Kind::And: return _mm_and_si128(a,b);
    case Kind::Or: return _mm_or_si128(a,b);
    case Kind::Xor: return _mm_xor_si128(a,b);
    case Kind::AndNot: return _mm_andnot_si128(a,b);
    case Kind::Shl: return PS2_PSLLVW(a,b);
    case Kind::Shr: return PS2_PSRLVW(a,b);
    case Kind::Sar: return PS2_PSRAVW(a,b);
    }
    return a;
}

static uint32_t randomWord(uint32_t &state)
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state;
}
static bool equal(const char *operation, unsigned index, const Bytes &expected, const Bytes &got)
{
    if (expected == got) return true;
    std::printf("FAIL %s case=%u expected=", operation, index);
    for (uint8_t byte : expected) std::printf("%02x", unsigned(byte));
    std::printf(" got=");
    for (uint8_t byte : got) std::printf("%02x", unsigned(byte));
    std::printf("\n");
    return false;
}

// Model lane movement by byte indices, independently of the host shuffle API.
static Bytes interleave(const Bytes &a, const Bytes &b, unsigned width, bool high)
{
    Bytes out{};
    const unsigned first = high ? 8 : 0;
    for (unsigned lane = 0; lane < 8 / width; ++lane) {
        std::memcpy(out.data() + lane*2*width,b.data() + first + lane*width,width);
        std::memcpy(out.data() + (lane*2+1)*width,a.data() + first + lane*width,width);
    }
    return out;
}
static Bytes permute(const Bytes &a, unsigned width, const unsigned *indices)
{
    Bytes out{};
    for (unsigned lane = 0; lane < 16 / width; ++lane)
        std::memcpy(out.data() + lane*width,a.data() + indices[lane]*width,width);
    return out;
}
static Bytes laneShift(const Bytes &a, unsigned width, unsigned count, Kind kind)
{
    Bytes out{};
    const unsigned bits = width*8;
    const uint64_t mask = bits == 64 ? UINT64_MAX : (uint64_t{1} << bits)-1;
    for (unsigned i = 0; i < 16 / width; ++i) {
        uint64_t x = 0; std::memcpy(&x,a.data()+i*width,width);
        uint64_t y = 0;
        if (kind == Kind::Shl) y = count < bits ? (x << count)&mask : 0;
        else if (kind == Kind::Shr) y = count < bits ? x >> count : 0;
        else {
            const bool negative = (x & (uint64_t{1} << (bits-1))) != 0;
            y = count < bits ? x >> count : 0;
            if (negative && count) y |= count >= bits ? mask : mask ^ (mask >> count);
        }
        std::memcpy(out.data()+i*width,&y,width);
    }
    return out;
}
template<unsigned N> static bool checkImmediate(const Bytes &a, const Bytes &b, unsigned &count)
{
    const __m128i av = vectorOf(a), bv = vectorOf(b);
    const auto check = [&](const char *name, const Bytes &expected, __m128i got) {
        ++count; return equal(name,N,expected,bytesOf(got));
    };
    if (!check("slli16_imm8",laneShift(a,2,N,Kind::Shl),_mm_slli_epi16(av,N)) ||
        !check("slli32_imm8",laneShift(a,4,N,Kind::Shl),_mm_slli_epi32(av,N)) ||
        !check("slli64_imm8",laneShift(a,8,N,Kind::Shl),_mm_slli_epi64(av,N)) ||
        !check("srli16_imm8",laneShift(a,2,N,Kind::Shr),_mm_srli_epi16(av,N)) ||
        !check("srli32_imm8",laneShift(a,4,N,Kind::Shr),_mm_srli_epi32(av,N)) ||
        !check("srli64_imm8",laneShift(a,8,N,Kind::Shr),_mm_srli_epi64(av,N)) ||
        !check("srai16_imm8",laneShift(a,2,N,Kind::Sar),_mm_srai_epi16(av,N)) ||
        !check("srai32_imm8",laneShift(a,4,N,Kind::Sar),_mm_srai_epi32(av,N))) return false;
    Bytes out{};
    for (unsigned i=0; i<16; ++i) if (i+N<16) out[i]=a[i+N];
    if (!check("srli128_bytes",out,_mm_srli_si128(av,N))) return false;
    const unsigned words[] = {(N>>0)&3u,(N>>2)&3u,(N>>4)&3u,(N>>6)&3u};
    if (!check("shuffle32_all_controls",permute(a,4,words),_mm_shuffle_epi32(av,N))) return false;
    unsigned halfwords[8];
    for(unsigned i=0;i<8;++i) halfwords[i]=i<4 ? (N>>(2*i))&3u : i;
    if (!check("shuffle_low16_all_controls",permute(a,2,halfwords),_mm_shufflelo_epi16(av,N))) return false;
    for(unsigned i=0;i<8;++i) halfwords[i]=i<4 ? i : 4+((N>>(2*(i-4)))&3u);
    if (!check("shuffle_high16_all_controls",permute(a,2,halfwords),_mm_shufflehi_epi16(av,N))) return false;
    for(unsigned i=0;i<4;++i) {
        const Bytes &source = i<2 ? a : b;
        std::memcpy(out.data()+i*4,source.data()+((N>>(i*2))&3u)*4,4);
    }
    if (!check("shuffle_ps_raw_bits",out,_mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(av),_mm_castsi128_ps(bv),N)))) return false;
    for(unsigned i=0;i<4;++i) std::memcpy(out.data()+4*i,((N&(1u<<i))?b:a).data()+4*i,4);
    if (!check("blend_ps_raw_bits",out,_mm_castps_si128(_mm_blend_ps(_mm_castsi128_ps(av),_mm_castsi128_ps(bv),N&15)))) return false;
    out=a; out[N&15]=uint8_t(0xa5u);
    if (!check("insert_byte_positions",out,_mm_insert_epi8(av,0xa5,N&15))) return false;
    if (uint32_t(_mm_extract_epi16(av,N&7)) != lane<uint16_t>(a,N&7) ||
        uint32_t(_mm_extract_epi32(av,N&3)) != lane<uint32_t>(a,N&3) ||
        uint64_t(_mm_extract_epi64(av,N&1)) != lane<uint64_t>(a,N&1)) {
        std::printf("FAIL extract positions immediate=%u\n",N); return false;
    }
    count+=3;
    return true;
}
template<size_t... N> static bool checkImmediates(const Bytes &a, const Bytes &b, unsigned &count, std::index_sequence<N...>)
{
    return (checkImmediate<N>(a,b,count) && ...);
}
#ifdef NX_PMFHL_ARCHITECTURAL
static bool checkPackedArchitecturalRegressions(unsigned &count)
{
    // Distinct lane labels expose the original shuffles even without random
    // inputs. Run all regressions before returning, retaining every mismatch.
    Bytes a{},b{},out{};
    for(unsigned i=0;i<8;++i) { put(a,i,uint16_t(0x100+i)); put(b,i,uint16_t(0x200+i)); }
    bool ok=true;
    const auto check=[&](const char *name,const Bytes &expected,__m128i actual) {
        ++count; ok=equal(name,0,expected,bytesOf(actual)) && ok;
    };
    for(unsigned i=0;i<4;++i) { put(out,2*i,lane<uint16_t>(b,i)); put(out,2*i+1,lane<uint16_t>(a,i+4)); }
    check("regression_pinth",out,PS2_PINTH(vectorOf(a),vectorOf(b)));
    for(unsigned i=0;i<4;++i) { put(out,2*i,lane<uint16_t>(b,2*i)); put(out,2*i+1,lane<uint16_t>(a,2*i)); }
    check("regression_pinteh",out,PS2_PINTEH(vectorOf(a),vectorOf(b)));
    const unsigned h[]={2,1,0,3,6,5,4,7},w[]={2,1,0,3},r[]={1,2,0,3};
    check("regression_pexeh",permute(a,2,h),PS2_PEXEH(vectorOf(a)));
    check("regression_pexew",permute(a,4,w),PS2_PEXEW(vectorOf(a)));
    check("regression_prot3w",permute(a,4,r),PS2_PROT3W(vectorOf(a)));
    put(a,0,uint32_t{32}); put(a,1,uint32_t{0xdeadcafe}); put(a,2,uint32_t{33}); put(a,3,uint32_t{0x12345678});
    put(b,0,uint32_t{0x80000001}); put(b,1,uint32_t{0xdeadbeef}); put(b,2,uint32_t{0x80000001}); put(b,3,uint32_t{0x76543210});
    for(const auto &operation : operations) if(operation.kind==Kind::Shl || operation.kind==Kind::Shr || operation.kind==Kind::Sar)
        check(operation.kind==Kind::Shl?"regression_psllvw":operation.kind==Kind::Shr?"regression_psrlvw":"regression_psravw",
            scalar<uint32_t>(operation,a,b),actual(operation,vectorOf(a),vectorOf(b)));
    return ok;
}
#endif
static bool checkMovementAndHiLo(unsigned &count)
{
    uint32_t state=0x1a2b3c4du;
    const auto check=[&](const char *name,unsigned i,const Bytes &expected,__m128i got) {
        ++count; return equal(name,i,expected,bytesOf(got));
    };
    for(unsigned i=0;i<512;++i) {
        Bytes a{},b{},out{},mask{};
        for(unsigned j=0;j<4;++j) { put(a,j,randomWord(state)); put(b,j,randomWord(state)); put(mask,j,randomWord(state)); }
        static constexpr uint32_t packedEdges[]={0,1,0xffffffffu,32767,32768,
            0xffff8000u,0xffff7fffu,0x7fffffffu,0x80000000u,0x80008000u,
            127,128,0xffffff80u,0xffffff7fu,65535,65536};
        if(i<256) for(unsigned j=0;j<4;++j) {
            put(a,j,packedEdges[(i+j)%16]); put(b,j,packedEdges[(i/16+j)%16]);
        }
        const __m128i av=vectorOf(a),bv=vectorOf(b);
        if(!check("pextlb",i,interleave(a,b,1,false),PS2_PEXTLB(av,bv)) ||
           !check("pextub",i,interleave(a,b,1,true),PS2_PEXTUB(av,bv)) ||
           !check("pextlh",i,interleave(a,b,2,false),PS2_PEXTLH(av,bv)) ||
           !check("pextuh",i,interleave(a,b,2,true),PS2_PEXTUH(av,bv)) ||
           !check("pextlw",i,interleave(a,b,4,false),PS2_PEXTLW(av,bv)) ||
           !check("pextuw",i,interleave(a,b,4,true),PS2_PEXTUW(av,bv)) ||
           !check("pcpyld",i,interleave(a,b,8,false),PS2_PCPYLD(av,bv))) return false;
        if(!check("unpack_high64_rs_rt",i,interleave(b,a,8,true),_mm_unpackhi_epi64(av,bv))) return false;
#ifdef NX_PMFHL_ARCHITECTURAL
        for(unsigned j=0;j<4;++j) { put(out,2*j,lane<uint16_t>(b,j)); put(out,2*j+1,lane<uint16_t>(a,j+4)); }
        if(!check("pinth_guest_lanes",i,out,PS2_PINTH(av,bv))) return false;
        for(unsigned j=0;j<4;++j) { put(out,2*j,lane<uint16_t>(b,2*j)); put(out,2*j+1,lane<uint16_t>(a,2*j)); }
        if(!check("pinteh_guest_even_lanes",i,out,PS2_PINTEH(av,bv))) return false;
        const unsigned exeh[]={2,1,0,3,6,5,4,7},exew[]={2,1,0,3},rot[]={1,2,0,3};
#else
        if(!check("pinth_native_contract",i,interleave(a,b,2,false),PS2_PINTH(av,bv)) ||
           !check("pinteh_native_contract",i,interleave(a,b,2,true),PS2_PINTEH(av,bv))) return false;
        const unsigned exeh[]={1,0,3,2,5,4,7,6}, exew[]={1,0,3,2}, rot[]={1,2,3,0};
#endif
        if(!check("pexeh_native_contract",i,permute(a,2,exeh),PS2_PEXEH(av)) ||
           !check("pexew",i,permute(a,4,exew),PS2_PEXEW(av)) ||
           !check("prot3w",i,permute(a,4,rot),PS2_PROT3W(av))) return false;
        for(unsigned j=0;j<16;++j) out[j]=(mask[j]&128u) ? 0 : a[mask[j]&15u];
        if(!check("shuffle_bytes_high_bit_zero",i,out,_mm_shuffle_epi8(av,vectorOf(mask)))) return false;
        unsigned bitmask=0;
        for(unsigned j=0;j<16;++j) { bitmask|=unsigned((a[j]>>7)&1u)<<j; out[j]=(mask[j]&128u)?b[j]:a[j]; }
        if(unsigned(_mm_movemask_epi8(av))!=bitmask) return false;
        ++count;
        if(!check("blendv_bytes_sign_bit",i,out,_mm_blendv_epi8(av,bv,vectorOf(mask)))) return false;
        for(unsigned j=0;j<4;++j) {
            const uint32_t source=lane<uint32_t>((lane<uint32_t>(mask,j)&0x80000000u)?b:a,j);
            put(out,j,source);
        }
        if(!check("blendv_ps_sign_bit_raw",i,out,_mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(av),_mm_castsi128_ps(bv),_mm_castsi128_ps(vectorOf(mask)))))) return false;
        for(unsigned j=0;j<2;++j) put(out,j,uint64_t(lane<uint32_t>(a,2*j))*lane<uint32_t>(b,2*j));
        if(!check("multiply_unsigned_even_words",i,out,_mm_mul_epu32(av,bv))) return false;
        for(unsigned j=0;j<4;++j) {
            const int64_t p0=int64_t(lane<int16_t>(a,2*j))*lane<int16_t>(b,2*j);
            const int64_t p1=int64_t(lane<int16_t>(a,2*j+1))*lane<int16_t>(b,2*j+1);
            put(out,j,uint32_t(p0+p1));
        }
        if(!check("multiply_add_signed_halfword_pairs",i,out,_mm_madd_epi16(av,bv))) return false;
        for(unsigned j=0;j<4;++j) {
            const unsigned other=(j+2)%4;
            const uint64_t product=uint64_t(lane<uint32_t>(a,j))*lane<uint32_t>(b,j);
            const uint64_t second=uint64_t(lane<uint32_t>(a,other))*lane<uint32_t>(b,other);
            put(out,j,uint32_t(product+second));
        }
        if(!check("pmaddw_macro_native_contract",i,out,PS2_PMADDW(av,bv))) return false;
        for(unsigned j=0;j<8;++j) {
            const int32_t word=lane<int32_t>(j<4?a:b,j%4);
            put(out,j,int16_t(std::clamp<int64_t>(word,INT16_MIN,INT16_MAX)));
        }
        if(!check("pack_signed32_to16_boundary",i,out,_mm_packs_epi32(av,bv))) return false;
        const uint64_t hi=lane<uint64_t>(a,0),lo=lane<uint64_t>(b,0);
        const uint32_t hw=uint32_t(hi),lw=uint32_t(lo);
        if(Ps2HiLoToU64(hi,lo)!=(uint64_t(hw)<<32|lw) ||
           Ps2SignExt32ToU64(lw)!=uint64_t(int64_t(std::bit_cast<int32_t>(lw)))) return false;
        count+=2;
#ifdef NX_PMFHL_ARCHITECTURAL
        const uint64_t hi1=lane<uint64_t>(a,1),lo1=lane<uint64_t>(b,1);
        put(out,0,uint32_t(lo)); put(out,1,uint32_t(hi));
        put(out,2,uint32_t(lo1)); put(out,3,uint32_t(hi1));
        if(!check("pmfhl_lw_two_banks",i,out,PS2_PMFHL_LW(hi,lo,hi1,lo1))) return false;
        put(out,0,uint32_t(lo>>32)); put(out,1,uint32_t(hi>>32));
        put(out,2,uint32_t(lo1>>32)); put(out,3,uint32_t(hi1>>32));
        if(!check("pmfhl_uw_two_banks",i,out,PS2_PMFHL_UW(hi,lo,hi1,lo1))) return false;
        for(unsigned bank=0;bank<2;++bank) {
            const uint64_t combined=(uint64_t(uint32_t(bank?hi1:hi))<<32)|uint32_t(bank?lo1:lo);
            const int64_t accumulator=std::bit_cast<int64_t>(combined);
            put(out,bank,uint64_t(std::clamp<int64_t>(accumulator,INT32_MIN,INT32_MAX)));
        }
        if(!check("pmfhl_slw_saturate_signed64_to32",i,out,PS2_PMFHL_SLW(hi,lo,hi1,lo1))) return false;
        const uint64_t banks[]={lo,hi,lo1,hi1};
        for(unsigned j=0;j<8;++j) put(out,j,uint16_t(banks[j/2]>>(32*(j%2))));
        if(!check("pmfhl_lh_select_low_halfwords",i,out,PS2_PMFHL_LH(hi,lo,hi1,lo1))) return false;
        for(unsigned j=0;j<8;++j) {
            const int32_t word=std::bit_cast<int32_t>(uint32_t(banks[j/2]>>(32*(j%2))));
            put(out,j,int16_t(std::clamp<int64_t>(word,INT16_MIN,INT16_MAX)));
        }
        if(!check("pmfhl_sh_saturate_signed32_to16",i,out,PS2_PMFHL_SH(hi,lo,hi1,lo1))) return false;
        R5900Context ctx{};
        ctx.hi=hi; ctx.lo=lo; ctx.hi1=hi1; ctx.lo1=lo1;
        ps2_pmthl_lw(&ctx,av);
        const uint64_t preserve=0xffffffff00000000ull;
        if(ctx.lo!=((lo&preserve)|lane<uint32_t>(a,0)) ||
           ctx.hi!=((hi&preserve)|lane<uint32_t>(a,1)) ||
           ctx.lo1!=((lo1&preserve)|lane<uint32_t>(a,2)) ||
           ctx.hi1!=((hi1&preserve)|lane<uint32_t>(a,3))) {
            std::printf("FAIL pmthl_lw_two_banks_preserve_upper32 case=%u\n",i); return false;
        }
        ++count;
#else
        // Deliberately test the unchanged native helper contract only. The
        // original two-argument helpers omit hi1/lo1 and are not architectural
        // PMFHL conformance. The maintained overlay selects the branch above.
        put(out,0,lo); put(out,1,hi);
        if(!check("pmfhl_lw_current_u64_contract",i,out,PS2_PMFHL_LW(hi,lo)) ||
           !check("pmfhl_uw_current_u64_contract",i,out,PS2_PMFHL_UW(hi,lo))) return false;
        Bytes packed{};
        for(unsigned j=0;j<8;++j) {
            const uint64_t source=j<4?lo:hi;
            const int32_t word=std::bit_cast<int32_t>(uint32_t(source>>(32*(j&1))));
            put(packed,j,int16_t(std::clamp<int64_t>(word,INT16_MIN,INT16_MAX)));
        }
        if(!check("pmfhl_slw_current_u64_contract",i,packed,PS2_PMFHL_SLW(hi,lo))) return false;
        const unsigned lh[]={0,1,4,5,2,3,6,7},sh[]={0,2,1,3,4,6,5,7};
        if(!check("pmfhl_lh_current_u64_contract",i,permute(packed,2,lh),PS2_PMFHL_LH(hi,lo)) ||
           !check("pmfhl_sh_current_u64_contract",i,permute(packed,2,sh),PS2_PMFHL_SH(hi,lo))) return false;
#endif
        for(int index : {-9,-1,0,1,2,3,4,7,32,INT32_MAX,INT32_MIN}) {
            if(uint32_t(PS2_EXTRACT_EPI32(av,index))!=lane<uint32_t>(a,unsigned(index)&3u) ||
               uint64_t(PS2_EXTRACT_EPI64(av,index))!=lane<uint64_t>(a,unsigned(index)&1u)) {
                std::printf("FAIL masked_extract case=%u index=%d\n",i,index); return false;
            }
            count+=2;
        }
        if(i<16 && !checkImmediates(a,b,count,std::make_index_sequence<256>{})) return false;
    }
    return true;
}
static bool checkRegisterWrites(unsigned &count)
{
    // Scalar writes replace the low 64 bits and preserve the upper 64 bits;
    // the zero register cannot be written, including by full vector writes.
    static constexpr uint32_t words[]={0,1,0x7fffffff,0x80000000,0xffffffff};
    for(unsigned index : {0u,1u,17u,31u}) for(uint32_t word : words) {
        R5900Context ctx{};
        R5900Context *context=&ctx; // generated SET_GPR_VEC takes a pointer identifier
        Bytes initial{}; put(initial,0,uint64_t{0x0123456789abcdef}); put(initial,1,uint64_t{0xfedcba9876543210});
        const auto check=[&](const char *name,uint64_t low) {
            Bytes expected=initial;
            if(index) put(expected,0,low);
            ++count; return equal(name,index,expected,bytesOf(ctx.r[index]));
        };
        const uint64_t signExtended=uint64_t(int64_t(std::bit_cast<int32_t>(word)));
        ctx.r[index]=vectorOf(initial); SET_GPR_U32(&ctx,index,word);
        if(!check("gpr_u32_sign_ext_preserve_hi64",signExtended)) return false;
        ctx.r[index]=vectorOf(initial); SET_GPR_S32(&ctx,index,std::bit_cast<int32_t>(word));
        if(!check("gpr_s32_preserve_hi64",signExtended)) return false;
        ctx.r[index]=vectorOf(initial); SET_GPR_ZE32(&ctx,index,word);
        if(!check("gpr_ze32_preserve_hi64",word)) return false;
        const uint64_t wide=uint64_t(word)<<32|uint32_t(~word);
        ctx.r[index]=vectorOf(initial); SET_GPR_U64(&ctx,index,wide);
        if(!check("gpr_u64_preserve_hi64",wide)) return false;
        ctx.r[index]=vectorOf(initial); SET_GPR_S64(&ctx,index,std::bit_cast<int64_t>(wide));
        if(!check("gpr_s64_preserve_hi64",wide)) return false;
        Bytes full{}; put(full,0,wide); put(full,1,~wide);
        ctx.r[index]=vectorOf(initial); SET_GPR_VEC(context,index,vectorOf(full));
        ++count;
        if(!equal("gpr_vector_and_zero_register",index,index?full:initial,bytesOf(ctx.r[index]))) return false;
    }
    for(uint32_t word : words) {
        const unsigned sign=word>>31;
        unsigned leading=0;
        for(unsigned bit=31;bit>0;--bit) {
            if(((word>>(bit-1))&1u)!=sign) break;
            ++leading;
        }
        if(ps2_plzcw32(word)!=leading) { std::printf("FAIL PLZCW word=%08x\n",word); return false; }
        ++count;
    }
    return true;
}

int runPackedIntegerTests()
{
    static_assert(sizeof(__m128i) == 16 && alignof(__m128i) == 16);
    static constexpr uint32_t edges[] = {0,1,31,32,0x7f,0x80,0x7fff,0x8000,
        0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0xaaaaaaaa};
    unsigned comparisons = 0;
#ifdef NX_PMFHL_ARCHITECTURAL
    if (!checkPackedArchitecturalRegressions(comparisons)) return 8;
#endif
    if (!checkMovementAndHiLo(comparisons)) return 6;
    if (!checkRegisterWrites(comparisons)) return 7;
    for (const auto &operation : operations) {
        uint32_t state = 0xc001d00du;
        for (unsigned i = 0; i < 4096; ++i) {
            Bytes a{}, b{};
            for (unsigned j = 0; j < 4; ++j) {
                put(a,j,i < 196 ? edges[(i + j) % 14] : randomWord(state));
                put(b,j,i < 196 ? edges[(i / 14 + j) % 14] : randomWord(state));
            }
            Bytes expected = operation.width == 1 ? scalar<uint8_t>(operation,a,b) :
                             operation.width == 2 ? scalar<uint16_t>(operation,a,b) : scalar<uint32_t>(operation,a,b);
            if (!equal(operation.name,i,expected,bytesOf(actual(operation,vectorOf(a),vectorOf(b))))) return 1;
            ++comparisons;
        }
    }
    for (unsigned i = 0; i < 256; ++i) {
        Bytes a{}, b{}, packed{};
        for (unsigned j = 0; j < 16; ++j) { a[j] = uint8_t(i + 2*j); b[j] = uint8_t(i + 2*j + 1); }
        for (unsigned width : {1u,2u,4u}) {
            const unsigned half = 8 / width;
            for (unsigned j = 0; j < half; ++j) {
                std::memcpy(packed.data() + j*width,b.data() + 2*j*width,width);
                std::memcpy(packed.data() + 8 + j*width,a.data() + 2*j*width,width);
            }
            const __m128i got = width == 1 ? PS2_PPACB(vectorOf(a),vectorOf(b)) :
                                  width == 2 ? PS2_PPACH(vectorOf(a),vectorOf(b)) : PS2_PPACW(vectorOf(a),vectorOf(b));
            if (!equal("pack_even_lanes",i,packed,bytesOf(got))) return 2;
            ++comparisons;
        }
        Bytes shifted{};
        for (unsigned bit = 0; bit < 128; ++bit) {
            const unsigned source = bit + (i & 127u);
            const uint8_t value = source < 128 ? b[source/8] : a[(source-128)/8];
            shifted[bit/8] |= uint8_t(((value >> (source%8)) & 1u) << (bit%8));
        }
        // Match the real generator boundary: translateQFSRV passes ctx->sa & 0x7f.
        if (!equal("qfsrv_guest_sa_masked",i,shifted,bytesOf(PS2_QFSRV(vectorOf(a),vectorOf(b),i & 127u)))) return 3;
        ++comparisons;
    }
    std::vector<uint8_t> ram(PS2_RAM_SIZE,0);
    const uint32_t addresses[] = {0,1,2,3,7,15,16,PS2_RAM_SIZE-1,PS2_RAM_SIZE-7,PS2_RAM_SIZE-15};
    for (uint32_t address : addresses) {
        Bytes pattern{};
        for (unsigned i = 0; i < 16; ++i) pattern[i] = uint8_t(address + i*7);
        Ps2FastWrite128(ram.data(),address,vectorOf(pattern));
        for (unsigned i = 0; i < 16; ++i)
            if (ram[(address+i)&PS2_RAM_MASK] != pattern[i]) return 4;
        if (!equal("unaligned_wrapping_memory",address,pattern,bytesOf(Ps2FastRead128(ram.data(),address)))) return 5;
        ++comparisons;
    }
    std::printf("PASS packed integers: %u checks (vector/scalar groups), saturation/lanes/shifts/unaligned wrapping accesses\n", comparisons);
    return 0;
}

#ifndef NX_CORE_TEST_SUITE
int main() { return runPackedIntegerTests(); }
#endif
