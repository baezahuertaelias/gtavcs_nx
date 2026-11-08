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

int runPackedIntegerTests()
{
    static_assert(sizeof(__m128i) == 16 && alignof(__m128i) == 16);
    static constexpr uint32_t edges[] = {0,1,31,32,0x7f,0x80,0x7fff,0x8000,
        0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0xaaaaaaaa};
    unsigned comparisons = 0;
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
    std::printf("PASS packed integers: %u vector comparisons, saturation/lanes/shifts/unaligned wrapping accesses\n", comparisons);
    return 0;
}

#ifndef NX_CORE_TEST_SUITE
int main() { return runPackedIntegerTests(); }
#endif
