# M02 compiler decision, generated-code audit and budgets

Implementation evidence for M02-04, M02-10 and M02-13. It is not a review or a
hardware result. Counts below are metadata from the private generated tree; no
generated game code is reproduced here.

## Generated-code construct audit

Scope: all 10,388 files of the private generated tree (EE and VU), scanned in the
`gtavcs-nx-m02-core` volume.

| Construct | Occurrences | Consequence for GCC |
|---|---:|---|
| `__attribute__`, `__builtin_`, inline asm, `__int128`, `union`, `musttail` | 0 | No compiler-specific extension to port |
| Computed goto, statement expressions, `__extension__`, `[[...]]` | 0 | Plain C++ control flow |
| `#pragma float_control(precise, on, push)` and its `pop` | 1 pair, VU only | Clang-specific. GCC ignores it, so `-ffp-contract=off` and `-frounding-math` must carry that guarantee on Switch |
| `#pragma once`, `alignas`, `char` | 1 each | Standard C++; signedness fixed by `-fsigned-char` |

**Signed overflow.** All 440,708 `ADD32` and 2,820 `SUB32` sites start with a
`GPR_U32` operand, so they evaluate as unsigned. `-fwrapv` is not needed for them.

**Shifts.** `SLLV`, `SRLV`, `SRAV` and the 64-bit forms are masked by the generator
(`special_translator.cpp`, `& 0x1F` and `& 0x3F`). Right shifts of signed values
are defined as arithmetic in C++20.

**Division.** The generator guards a zero divisor and `INT32_MIN / -1`. This matters
on ARM, where `sdiv` does not trap as x86 `idiv` does.

**Aliasing.** Guest memory is read through casts, so `-fno-strict-aliasing` stays
required for all generated and runtime code.

**Float to integer.** `FPU_CVT_W_S` goes through the `ps2FpuCvtWS` helper, covered by
the conversion cases of the float suite. A C-style cast is not relied on for it.

**Warnings.** The generated ARM build and the private rebuild logs contain zero
compiler warnings. No `-Wall` is used, so this covers only GCC's default warnings.

## Compiler decision

Use devkitA64 GCC 15.2.0, C++20, with `-O2 -fPIE -ffp-contract=off -frounding-math
-fno-strict-aliasing -fsigned-char` and the precise SSE2NEON options. It builds the
full generated core, the real runtime and the IOP library. No function was dropped.

Upstream's Clang 19+ gate is not removed. Switch uses the separate `cmake/core` and
`cmake/runtime` projects, so the desktop project keeps the gate and its compiler.

**Status: decided and proven on hardware, 2026-11-09.** The GCC build ran the float,
packed, compiled-VU, ABI and scheduler suites on a real Switch with results identical
to the x86 reference. See [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md).

**Fused-multiply scan.** A disassembly of the final linked private ELF holds 36
`fmadd`, `fmsub`, `fnmadd`, `fnmsub`, `fmla` or `fmls` instructions, all in libc
`cosf`, `sinf`, `_dtoa_r` and `_strtod_l`. None is in generated game code.

## ReleaseMode

`ReleaseMode.cmake` is not included by either Switch project, so none of its x86
assumptions apply there:

| ReleaseMode item | Switch handling |
|---|---|
| `-march=x86-64-v3`, `/arch:AVX2` | Not used. Official Switch toolchain CPU flags apply |
| `-fcf-protection=none`, `/GS-`, `/Qspectre-` | Not used; x86 or MSVC specific |
| ThinLTO, full LTO, PGO | Not used. The large generated link stays non-LTO |
| `-ffp-contract=off`, `-fno-strict-aliasing` | Kept, through `cmake/CPUOptions.cmake` and `cmake/core` |
| `-O2 -DNDEBUG` | Kept, through `CMAKE_BUILD_TYPE=Release` and explicit `-O2` |
| `-fno-math-errno` | Not applied. It affects speed, not results; revisit in M10 with measurements |

The desktop configuration is unchanged.

## Budgets

| Item | Value | Source |
|---|---:|---|
| Guest RDRAM | 32 MiB | `PS2_RAM_SIZE` |
| IOP RAM | 2 MiB | `ps2_memory.cpp` |
| Private core text and data | 43.8 MiB (45,942,078 bytes) | `size` on the final ELF |
| Private core BSS, including the table | 9.8 MiB (10,317,608 bytes) | `size` on the final ELF |
| Generated function table | 7.0 MiB (917,839 slots of 8 bytes) | `[core-table]` log |
| Runtime test image BSS (runtime state and fixtures) | 66.8 MiB (70,086,232 bytes) | `size` on the final ELF |
| Suite worker stack | 8 MiB | `runtime_main.cpp` |
| Native peak commit, full runtime suites | 188.8 MiB | `runtime-native-peak-memory.log`, sampled Windows x86 |
| Test allocator arena | 256 MiB | `src/core_tests/heap.cpp` |
| Switch allocator obtained, runtime suites | 98.4 MiB | `[resources]` log, end of run |
| Switch allocator obtained, private core | 39.6 MiB | `[resources]` log, end of run |
| Temporary workers, declared by the harness | 3 | `runtime_main.cpp`; the host-boundary suite measured `workers_peak=2` |

The arena was 128 MiB until the native peak was measured on 2026-11-09. At that size
the suites would have been expected to run out of memory on the console. The
native peak includes a 6 MiB image and differs from the Switch allocator. The Switch
rows come from the `[resources]` log lines. Headroom in the test arena is 157.6 MiB
over the runtime suites and 216.4 MiB over the private core. These budgets cover the
CPU harness only; M04 must measure the running game.
