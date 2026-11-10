# M02 compiled VU differential proof

This is implementation evidence, not an independent review or hardware PASS.
The public fixture contains only deliberately authored synthetic instruction
pairs. No disc, game ELF, extracted upload or private game microcode is read.

## Generator and fixture identity

`tools/generate_vu_test_fixture.py` creates a minimal synthetic MIPS ELF and an
upload list in a temporary directory below build/. It invokes the actual
upstream `tools/vu1_recomp.py` emission pipeline. The wrapper checks:

- Upstream base commit `ecf150e53f87e4656dfe81eacd922839d952b282`.
- Pristine generator SHA-256
  `9b9f8fa3b0da2b451940ff561373da3c13d630b705e8c9a11c11f191dd9a9725`.
- Maintained 0003 policy patch SHA-256
  `304d5810dde7960671dae5b2245ce52ae296ed5376b288a1bd2ed4552afe2910`.
- Effective strict generator SHA-256
  `fea83a1165ce07a00a30de806f1ee80497a0fb6318cd085d44f7cbeac637de31`.

An explicitly supplied `--generator` path must have that exact audited content;
the wrapper does not select a rolling/downloaded generator or search for inputs.
The generated `tests/core/synthetic_vu_fixture.inc` retains GPL provenance and
both generator identities. `--check` regenerates and compares it exactly.

```powershell
python tools/generate_vu_test_fixture.py --generator build/m02/vu-author/tools/vu1_recomp.py --check
```

`vu-author` is the disposable authoring checkout for 0003. After the root
integrator applies that patch, the same check can explicitly point at the
maintained `build/m02/switch-source/tools/vu1_recomp.py` snapshot. The fixture's
instruction bodies, shapes and both function variants come from the real
generator. Wrapper changes add entry/pair counters, public provenance/names and
an explicit registration function that retains the generator's exact policy.
They do not substitute a callback that writes expected results.

## Defects exposed and strict baseline policy

The first real native differential run failed on MADD with `a=FLT_MAX`, `b=2`
and `ACC=-1`: fast interpreter result `0x7f7fffff`, compiled throughput result
`0x7f7ffffe`. The generator's `_t` variant uses plain float MADD/MSUB, while the
fast interpreter applies the exact-double range rule to decide overflow,
underflow and exact zero. A second isolated diagnostic, changing only that
overflow lane in an ignored temporary test source to expose further behavior,
found 10 compiled cycles versus 15 fast-interpreter cycles. The throughput
variant deliberately omits VF/VI/ACC stall/ready/settle bookkeeping.

`0003-vu-compiled-numeric-parity.patch` makes generated registration select the
existing full bookkeeping/exact-range variant when `__SWITCH__` or
`PS2X_VU1_COMPILED_PRECISE` is defined. This covers vector and accumulator
MADD/MSUB forms. The desktop's original selection remains unchanged. The VU
interpreter still has fast mode enabled, making actual compiled dispatch
eligible; selecting `PS2X_VU1_FAST=0` would disable that dispatch and would not
prove compiled execution.

The native fixture defines `PS2X_VU1_COMPILED_PRECISE=1` before its generated
include, testing the same policy used automatically on Switch. No overflow
input, active-state comparison or cycle check was relaxed. The strict policy
is a correctness baseline; no performance claim is made. The private generated
VU file must also be regenerated from the overlaid generator so its registrar
contains this policy. Recompiling an older generated file alone is insufficient.

Preserved raw failures:
`build/evidence/M02/compiled-vu-native.log` (overflow) and
`compiled-vu-diagnostic.log` (throughput timing, ignored diagnostic source).

## Actual workloads

`tests/core/compiled_vu_tests.cpp` exports `int runCompiledVuTests()`. It includes
the public generated fixture itself; define `NX_CORE_TEST_SUITE` when linking
with the runtime runner. It requires the real runtime/IOP/null-host libraries.
No generated game-function table is needed; the standalone check links the
existing public test table.

Six synthetic programs cover:

1. Vector MADD/MSUB, MADDA/MSUBA accumulator writes, MUL/ADD, signed zero,
   denormal and exponent-255 operand normalization, large overflow inputs,
   MIN/MAX, nonzero CLIP flags and VU stores. The non-fused/toward-zero witness
   must produce `0xb3800000` in both the combined MADD and separate MUL/ADD route.
2. A three-iteration conditional loop, its delay slots, and JR consuming a
   just-written VI plus its delay slot. Explicit witnesses verify loop count
   and the one JR-delay execution.
3. DIV/WAITQ, MULq, ESADD/WAITP, MFP and data stores, with exact Q=1.5/P=25
   witnesses. Unsupported specialized EFU emission remains an explicit real
   interpreter helper call within the generated function; it is not relabelled
   as independent compiled EFU arithmetic.
4. A queued VU store followed by XGKICK; a captured 32-byte PATH1 packet must
   contain the stored payload and match the interpreter's bytes exactly.
5. Code-identity mutation through tracked VU memory writes on the same live VU
   object: the original program selects compiled code, changing an immediate
   invalidates the piece and executes the changed result with zero generated
   calls, and restoring the original bytes reselects compiled code.
6. Divide-by-zero Q saturation and current/sticky FDIV status, explicitly
   checking Q=`0x7f7fffff` and status mask=`0x820`.

Each program runs with nearest and toward-zero caller rounding. The runtime
must restore the caller's environment. Five additional one-cycle arithmetic
execute/resume slices compare state/cycles across budget exits and stalls.
A slice wholly consumed by a real pipeline stall may issue zero instruction
pairs, but must still select the generated entry and match the interpreter's
state and elapsed cycles. Complete runs must count positive generated pairs.

Comparisons include all VF/VI/ACC values bit-for-bit, Q/P/I, R, active PC/branch
state, CLIP/MAC/status, execution flags, cycles, all 16 KiB VU data memory and
all GIF packets. Completed branches can leave different unused cached targets;
the target/delay are compared whenever a branch is pending. Reused objects keep
their actual cumulative private cycle clock across execute calls; identity
fallback expectations preserve that clock rather than pretending it resets.

## Observed result

Native Clang23.1.4, x86-64-v3, O2, signed char, FP contraction off,
rounding-math, no strict aliasing, platform guest ABI consistent with the linked
native runtime libraries, 8 MiB executable stack, isolated CPU/null-host test:

```text
PASS compiled VU: public_programs=6 cases=20 generated_calls=19 generated_pairs=124 identity_fallback_calls=0 rounding/budgets/branches/Q/P/XGKICK
```

Build output and the passing trace are retained in
`build/evidence/M02/compiled-vu-native-strict-build.log` and
`compiled-vu-native-strict.log`. The test's object also compiles as ELF64
little-endian AArch64 with devkitA64 GCC15.2.0, pinned SSE2NEON and the official
target CPU/TLS/PIE/strict numeric options; see compiled-vu-arm64-build.log.
Target execution of this suite is recorded in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md).

This checks the listed real generated fast-mode paths. Cycle-exact interpreter
pipeline/hazard suites, private game uploads and ingame behavior remain distinct
evidence; six public programs do not establish every possible VU program or
architectural opcode.
