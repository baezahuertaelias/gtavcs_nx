// Public upstream PS2-autotest vectors plus explicit port boundary checks.
#include "ps2_runtime_macros.h"
#include <bit>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>

struct HardwareVector { const char *op; uint32_t fs, ft, result; int known; };
static const HardwareVector vectors[] = {
#include "ps2_float_hw_vectors.inc"
};
static float value(uint32_t bits) { return std::bit_cast<float>(bits); }
static uint32_t bits(float number) { return std::bit_cast<uint32_t>(number); }
static uint32_t evaluate(const HardwareVector &v)
{
    volatile float a = value(v.fs), b = value(v.ft);
    if (std::strcmp(v.op,"add") == 0) return bits(FPU_ADD_S(a,b));
    if (std::strcmp(v.op,"sub") == 0) return bits(FPU_SUB_S(a,b));
    if (std::strcmp(v.op,"mul") == 0) return bits(FPU_MUL_S(a,b));
    if (std::strcmp(v.op,"div") == 0) return bits(FPU_DIV_S(a,b));
    if (std::strcmp(v.op,"sqrt") == 0) return bits(FPU_SQRT_S(a));
    return bits(FPU_RSQRT_S(a,b));
}

#ifdef NX_FLOAT_EMIT_REFERENCE
#if defined(__aarch64__)
#error "Native x86 reference data must not be regenerated from the ARM target"
#endif
int main()
{
    if (std::fesetround(FE_TONEAREST) != 0) return 1;
    std::puts("// Native Clang23.1.4, pinned upstream ecf150e53f87e4656dfe81eacd922839d952b282.");
    std::puts("// Public CPU test values only; not game code/data. Generated explicitly on x86.");
    for (const auto &v : vectors) std::printf("0x%08xu,\n",evaluate(v));
    return 0;
}
#else
static const uint32_t expected_native[] = {
#include "float_reference.inc"
};
static_assert(std::size(expected_native) == std::size(vectors));

int runFloatTests()
{
    const int original_rounding = std::fegetround();
    if (original_rounding < 0 || std::fesetround(FE_TONEAREST) != 0) return 1;
    unsigned exact = 0, known_one_ulp = 0, known_denormal = 0;
    int result = 0;
    for (unsigned i = 0; i < std::size(vectors); ++i) {
        const auto &v = vectors[i];
        const uint32_t got = evaluate(v);
        // All rows, including upstream's known differences, must remain bit
        // identical to the native reference. Known differences are not passes
        // against the original PS2 hardware; their categories remain explicit.
        if (got != expected_native[i]) {
            std::printf("FAIL native FP row=%u op=%s expected=%08x got=%08x\n",i,v.op,expected_native[i],got);
            result = 2; break;
        }
        const uint32_t distance = got > v.result ? got-v.result : v.result-got;
        if (v.known == 0) {
            if (got != v.result) {
                std::printf("FAIL exact hardware FP row=%u expected=%08x got=%08x\n",i,v.result,got);
                result = 3; break;
            }
            ++exact;
        } else if (v.known == 1) {
            if (distance != 1u) { result = 4; break; }
            ++known_one_ulp;
        } else if (v.known == 2) {
            ++known_denormal;
        } else { result = 5; break; }
    }
    struct Conversion { uint32_t input; int32_t expected; };
    const Conversion conversions[] = {
        {0,0},{0x80000000,0},{0x00000001,0},{0x80000001,0},
        {0x3ff33333,1},{0xbff33333,-1},{0x4effffff,2147483520},
        {0x4f000000,INT32_MAX},{0xcf000000,INT32_MIN},{0xcf000001,INT32_MIN},
        {0x7f800000,INT32_MAX},{0x7fc00000,INT32_MAX},{0x7fffffff,INT32_MAX},
        {0xff800000,INT32_MIN},{0xffffffff,INT32_MIN},
    };
    for (const auto &c : conversions) {
        volatile float input = value(c.input);
        if (FPU_CVT_W_S(input) != c.expected) { result = 6; break; }
    }
    if (result == 0) {
        for (int mode : {FE_TONEAREST,FE_TOWARDZERO}) {
            if (std::fesetround(mode) != 0 || std::fegetround() != mode) { result = 7; break; }
            volatile float a = value(0x3f800001), b = value(0x3f7ffffe), c = -1.0f;
            const float separate = FPU_ADD_S(FPU_MUL_S(a,b),c);
            const uint32_t expected = mode == FE_TONEAREST ? 0u : 0xb3800000u;
            if (bits(separate) != expected) {
                std::printf("FAIL non-fused/rounding mode=%d expected=%08x got=%08x\n",mode,expected,bits(separate));
                result = 8; break;
            }
            // VU's same scalar rules must apply to every vector lane.
            const __m128 left = _mm_set_ps(1.0f,value(0x7f800000),value(0xffffffff),value(0x7fffffff));
            const __m128 right = _mm_set_ps(2.0f,1.0f,value(0xffffffff),value(0x7fffffff));
            const __m128 sum = PS2_VADD(left,right);
            float l[4], r[4], out[4];
            _mm_storeu_ps(l,left); _mm_storeu_ps(r,right); _mm_storeu_ps(out,sum);
            for (unsigned lane = 0; lane < 4; ++lane)
                if (bits(out[lane]) != bits(FPU_ADD_S(l[lane],r[lane]))) result = 9;
        }
    }
    if (std::fesetround(original_rounding) != 0) result = 10;
    if (result) {
        std::printf("FAIL float suite result=%d\n",result);
        return result;
    }
    std::printf("PASS float suite: native_rows=%zu hardware_exact=%u inherited_one_ulp=%u inherited_denormal=%u conversions=%zu rounding/non-fused/vector-lanes\n",
        std::size(vectors),exact,known_one_ulp,known_denormal,std::size(conversions));
    return 0;
}

#ifndef NX_CORE_TEST_SUITE
int main() { return runFloatTests(); }
#endif
#endif
