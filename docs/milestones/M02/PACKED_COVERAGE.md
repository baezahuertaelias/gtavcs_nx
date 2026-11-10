# M02 packed CPU validation

This is implementation evidence for M02-06, not milestone approval. The Switch
run is recorded in HARDWARE_RESULTS.md. The test inputs and scalar models are public synthetic
data; this document contains no generated game code or disc payloads.

## Sources and ownership

Upstream base: `ecf150e53f87e4656dfe81eacd922839d952b282`.
The runtime boundary is
`third_party/PS2Recomp/ps2xRuntime/include/ps2_runtime_macros.h`; the owning
generator is `ps2xRecomp/src/lib/mmi_translation_helpers.cpp`.

The scalar byte/lane models in `tests/core/packed_integer_tests.cpp` check the
actual helpers/intrinsics. They do not obtain their expected values from the
same SIMD operations. The precise SSE2NEON options, C++20, signed-char,
`-ffp-contract=off`, `-frounding-math` and `-fno-strict-aliasing` remain enabled.

## Coverage

| Family | Explicit checks |
|---|---|
| Arithmetic/comparison | Existing 42 wrapping, saturating, signed/unsigned comparison, multiply, min/max and bitwise variants; 4,096 edge/seeded input pairs each |
| Lane movement | Lower/upper byte, halfword and word interleave; low/high doubleword copies; byte shuffles including high-bit zero selection |
| Immediate operations | Every immediate byte 0..255 for 16/32/64-bit left/logical-right shifts, 16/32-bit arithmetic-right shifts, byte shifts, all word/halfword shuffles, raw-bit float shuffles/blends |
| Selection and extraction | Byte/sign-bit blends, sign-bit float blends with raw word models, movemask, every byte-insert position, all word/halfword/doubleword extract positions, masked variable indices including negative values |
| Products/packing | Unsigned even-word products, signed halfword multiply/add pairs, signed word-to-halfword saturation boundaries, even-lane PPAC helpers |
| Guest register boundaries | Scalar sign/zero extension, preserved upper 64 bits, full-vector writes and ignored writes to register zero |
| HI/LO | HI/LO word composition/sign extension; five corrected four-bank PMFHL variants and PMTHL preserved-upper-word behavior |
| Other guest boundaries | Funnel shifts with the actual generator's seven-bit SA mask; leading-sign count; unaligned/wrapping 128-bit RDRAM accesses |

`NX_PMFHL_ARCHITECTURAL=1` selects the maintained overlay's architectural
regressions. It is required for corrected target builds. Without it, the same
file can still test the pristine reference's explicitly labeled legacy helper
contracts. Passing that legacy mode is not architectural PMFHL/shuffle proof.

The count is **273,039 check groups**, including scalar assertions and vector
comparisons. It must not be described as 273,039 distinct instructions or all
R5900 instruction semantics. The first hardware run covered the earlier
173,066-vector suite only; it is not proof of these additions.

## Corrections exposed by independent models

The eight focused regressions fail against the staged `0001` boundary and pass
after `0002-packed-architectural-semantics.patch`. Expected failures are retained
in `build/evidence/M02/packed-eight-before-fix.log` (exit 8), rather than removed
or converted into accepted skips.

| Operation | Corrected contract |
|---|---|
| PINTH | Alternate lower rt halfwords with upper rs halfwords |
| PINTEH | Interleave even halfwords from both inputs |
| PEXEH / PEXEW | Exchange even lanes, preserving odd lanes |
| PROT3W | Rotate the low three words, preserving word 3 |
| PSLLVW / PSRLVW / PSRAVW | rs supplies masked counts; rt supplies even data words; sign-extend each result to 64 bits |

Primary comparison source: the
[PCSX2 MMI interpreter at 4223d03](https://github.com/PCSX2/pcsx2/blob/4223d03c58f8fd1787da7af28fe80c2140e1a22a/pcsx2/MMI.cpp).
The helper/generator corrections are maintained in the root overlay with
`[gta-recomp]` markers and upstream documentation. No emulator implementation
file was copied into the port.

The runtime macros fix PINTH/PINTEH, PEXEH/PEXEW/PROT3W and variable packed word
shifts. PEXEH and PROT3W previously emitted stale inline shuffles, so their
generator owners now invoke the corrected macros. Existing rs/rt ordering at
the variable-shift call sites is preserved; the helper now interprets it
correctly. Generic four-word intrinsic helpers remain separate.

The new PMFHL models also exercise both accumulator banks rather than repeating
the old two-argument helper results. That four-bank implementation is supplied
by `0001`; `0002` does not replace or duplicate it.

## Observed validation

All raw logs below remain ignored, under `build/evidence/M02/`.

| Check | Observed result | Log |
|---|---|---|
| Pristine legacy helper counterpart | Native Clang23 passed 272,519 groups before the eight architectural regression groups were added | `packed-integer-extended-native.log` |
| Eight architectural regressions before correction | All eight report mismatches; exit 8 | `packed-eight-before-fix.log` |
| Corrected architectural suite | Native Clang23.1.4 passed 273,039 groups | `packed-architecture-fixed-native.log` |
| Independent compiler / undefined-behavior instrumentation | Native Debian GCC12.2, C++20, UBSan with non-recovering errors passed 273,039 groups | `packed-architecture-fixed-ubsan.log` |
| ARM64 compile | devkitA64 GCC15.2 compiled the corrected suite; ELF64 little-endian AArch64 object inspected | `packed-architecture-fixed-arm64-build.log` |
| Generator owner compile | Native Clang23 compiled the changed translation unit with the pinned host dependency includes | `packed-generator-native-build.log` |
| Patch round trip | `0002` applies after staged `0001`; reverse restores an empty unstaged diff | Disposable `build/m02/packed-roundtrip` |

The native standalone command uses `-std=c++20 -O2 -march=x86-64-v3`, the strict
numeric/aliasing flags above, `-static`, the overlaid runtime/IOP includes and
`-DNX_PMFHL_ARCHITECTURAL=1`. The ARM compile additionally uses the official
devkitA64 compiler, `-fPIE`, `-march=armv8-a+crc+crypto`, the locked SSE2NEON
header and the port's checked allocation boundary. It does not execute ARM code
on the host.

A separate signed-left-shift investigation is retained as
`packed-signed-shift-language-probe.log`: the negative-input probe passes UBSan
under the project's required C++20 and fails under C++17. That does not establish
a C++20 defect and was not used to justify an unrelated runtime edit.

## Integration and remaining proof

Root must append `0002` to the maintained series, prepare/reverse/reapply the full
series, and rebuild the automatic CPU-test NRO with the architectural definition.
The patch SHA-256 is
`079e214444a2908867f941e81dde8757fda8c02d1968bec0cbb1bae10f9b1839`.
It was generated from the author's **unstaged** diff, so it contains only changes
beyond staged `0001`. The registered upstream submodule was not edited.

A metadata-only search of all private generated EE sources found zero calls to
PMFHL and the eight affected named helpers, and zero instances of the original
PMTHL assignment shape. The direct PEXEH/PROT3W intrinsic shapes also do not
appear in the recorded private intrinsic inventory. This limits the immediate
generated-code impact; it does not waive the synthetic foundation checks.

The Switch run of the 273,039-group suite is recorded in HARDWARE_RESULTS.md. Full
scheduler execution, compiled VU comparison, generator-registration
admission and game loading belong to the other M02 tasks. This helper suite does
not substitute for those tasks or prove all possible packed multiply/divide
instruction behavior.
