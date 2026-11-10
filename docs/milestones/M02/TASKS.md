# M02 execution checklist

This expands the authoritative [milestone ledger](../../MILESTONES.md) into
concrete work and proof. A parent task may close only after its required open
steps below have observed evidence. This checklist is manual; the milestone gate
checks the parent ledger/evidence records, and the independent reviewer must
also inspect these steps. No reviewer is created during partial implementation.

Parent task status lives in [docs/MILESTONES.md](../../MILESTONES.md). See
[VERIFICATION.md](VERIFICATION.md) and [evidence.json](evidence.json) for
observations, commands and limitations.

## M02-01 — Desktop reference baseline

- [x] Verify the supplied ISO as SLUS-21590 USA v1.00 and verify executable SHA-1.
- [x] Clone the exact upstream pin into an ignored reference checkout; preserve the registered submodule.
- [x] Record native compiler/tool versions, CPU flags, LTO and FP-contraction settings.
- [x] Generate/build the unchanged desktop reference with the pinned upstream tooling.
- [x] Use separate userdata/memory cards and a hidden test window; never use the user's saves.
- [x] Retain the 30-second D3D11 startup/movie baseline and binary hashes; label its timeout and non-gameplay scope.

## M02-02 — Host-only generator and analyzer

- [x] Build the native Windows recompiler and analyzer independently of target compilation.
- [x] Verify both binaries are AMD64 PE, not AArch64 tools accidentally run on the host.
- [x] Record executable hashes and the host build/run commands.
- [x] Keep generator/ELF/game inputs out of source-only CI and public artifacts.

## M02-03 — Private EE/VU generation and diagnostics

- [x] Run unchanged recompile.py against the explicitly selected supported ELF.
- [x] Preserve the source/config stamp and ELF identity privately.
- [x] Record 10,383 owner functions, 236,304 registration assignments and 226,519 additional entrypoints distinctly.
- [x] Record generated VU program/pair counts and private generated-file counts.
- [x] Retain every warning/error owner and first diagnostic address as metadata; retain full diagnostics privately.
- [x] Explain the legacy VU banner's wrong executable name using actual input/hash evidence, without treating the banner as identity.
- [x] Keep generated C++, instruction bytes and extracted ELF out of commits/releases.

Recording diagnostics does not resolve the 1,196 indirect-control-flow warnings
or 561 unhandled-instruction events. Their compatibility/coverage consequences
belong to the open tasks below.

## M02-04 — Compiler and generated-code compatibility

- [x] Test the existing USE_SSE2NEON header path with devkitA64 GCC; retain the initial failure.
- [x] Establish that this newlib lacks posix_memalign, rather than hiding a missing symbol with a declaration.
- [x] Implement the Switch allocation boundary using supported memalign/free and validate its host contract.
- [x] Compile strict C++20 EE/VU headers to AArch64 with vector size/alignment assertions.
- [x] Create a separate compile-only generated-code project using the official toolchain and pinned SSE2NEON content.
- [x] Identify and retain the missing Kernel/Stubs include failure; use the owning upstream CMake include path.
- [x] Identify the oversized final unity unit from actual compiler CPU/memory/process evidence.
- [x] Complete the full generated EE+VU archive build with all functions retained, O2 and strict numeric flags.
- [x] Verify the separated registration/VU compilation units finish and retain the cancelled original-unit evidence accurately.
- [x] Inspect every archive member's target architecture and all compiler/include paths.
- [x] Audit remaining generated compiler extensions, conversions, aliasing and unsupported constructs.
- [x] Decide and document the GCC-compatible configuration from full build/numeric/ABI proof; do not merely remove upstream's Clang gate.

## M02-05 — SIMD inventory and guest layout

- [x] Read all scoped discovery inventories and index all 661 original lexical SIMD sites by file/function/operation family.
- [x] Resolve manual operation families and parser-uncertain owners relevant to the compiled core.
- [x] Assign every site to the core port, host generator, optional backend, or a named later milestone; do not silently omit sites.
- [x] Compile-check 16-byte vector size/alignment on ARM64.
- [x] Exercise unaligned and wrapping 128-bit RDRAM accesses against a scalar byte model on host and Switch.
- [x] Compare full R5900Context/VU/guest-register sizes, alignments and critical offsets across the native and ARM layouts.
- [x] Verify 64/128-bit bit casts, sign extension, endian lane order and memory aliases beyond the current fast-RDRAM cases.
- [x] Run the relevant existing full memory/scratchpad/MMIO tests with the real runtime linked.

Per-site source metadata: [simd_candidates.json](simd_candidates.json).
The index is an audit worklist, not a numeric-conformance result.

## M02-06 — Packed integer semantics

- [x] Build a scalar byte/lane reference suite with deterministic edge cases and seeded inputs.
- [x] Validate 8/16/32-bit wrapping arithmetic, signed/unsigned saturation and comparisons on the native reference compiler.
- [x] Validate vector absolute, low/high multiply, signed min/max and bitwise results against scalar models.
- [x] Validate variable shifts, even-lane packing and QFSRV's actual generator-side SA mask.
- [x] Validate the first 173,066 integer/memory vector comparisons on the actual Switch.
- [x] Verify the SD payload hash and retain the target PASS log with no new crash report.
- [x] Extend/check coverage for all core-relevant shuffle/interleave/extract/insert and HI/LO operations in the source inventory.
- [x] Cover remaining immediate-shift boundaries, lane masks, conversions and guest-specific packed-operation edge cases.
- [x] Compare the completed native and ARM suites; retain and fix every unexplained difference at its owning layer.

The suite covers the families listed in [PACKED_COVERAGE.md](PACKED_COVERAGE.md); it
does not establish all packed guest instructions or all SIMD sites by itself.

## M02-07 — FPU/VU numerics and compiled programs

- [x] Reuse the existing public PS2 hardware-vector corpus and distinguish exact rows from the upstream's documented known differences.
- [x] Check signed zero, exponent-255 PS2 values, exceptional conversions, underflow/denormals, saturation and division/square root.
- [x] Compare scalar and vector helpers on native x86 and ARM64, including NaN/min/max compatibility where the host SIMD boundary requires it.
- [x] Verify FE_TOWARDZERO/FPCR behavior and save/restore of the rounding environment on ARM.
- [x] Preserve -ffp-contract=off in the current generated and CPU-test build commands.
- [x] Prove multiply/add results are not fused on compiled paths: flags on every generated unit, a fused-instruction scan of the generated archive, and float and synthetic compiled-VU witnesses. Generated EE bodies first execute in M04.
- [x] Build/run existing relevant VU tests with the real interpreter and compare compiled VU paths.
- [x] Cover Q/P pipelines, clip/status/MAC flags, branches/delay slots, hazards/stalls, XGKICK and cycle budgets.
- [x] Verify uploaded-code identity checks and interpreter fallback selection instead of assuming compiled VU always applies.
- [x] Record any inherited reference limitations explicitly; do not describe skipped/known-different rows as exact hardware matches.

## M02-08 — Calling convention and scheduler boundaries

- [x] Inspect ps2_guest_cc.h and record its existing non-x86 platform-ABI fallback.
- [x] Compile-check consistent declarations and function-pointer types across generated entries, wrappers and hooks.
- [ ] Link and inspect the complete generated registration table, aliases, slot bounds and resumable entries.
- [ ] Verify stack alignment and argument/register integrity at real host/guest boundaries on ARM.
- [x] Exercise call, tail-call, yield and resume transitions without assuming a generated guest call returns to its wrapper.
- [x] Verify guest return-address preservation and repeated scheduler stop/restart behavior.
- [x] Compare deterministic scheduler traces/results with the native reference path.

## M02-09 — ABI, allocation, threads and host assumptions

- [x] Observe devkitA64's unsigned default char, little endian, 64-bit long/pointers.
- [x] Preserve native signed-char behavior in the CPU foundation flags; verify char_signed=1 on Switch.
- [x] Validate 16 allocation/alignment cases on host and Switch, including invalid alignment and null/zero-size handling.
- [x] Observe that the NRO loader heap override bypasses the requested weak heap-size setting.
- [x] Validate the custom libnx heap hook really bounds the allocator, including rejection of an over-budget allocation.
- [x] Keep kernel-mapped reservation separate from usable allocator arena/live allocation accounting in reports.
- [x] Test atomics and memory ordering with real producer/consumer synchronization on ARM.
- [x] Test TLS, exceptions/unwind assumptions, aligned allocation, host waits and condition-variable behavior.
- [x] Audit applicable newlib/POSIX API differences; replace only unsupported owning paths and prove the replacement.

## M02-10 — Real runtime build and null host boundary

- [x] Pin SSE2NEON commit/header hash/license; avoid a rolling target dependency.
- [x] Keep host tools and target compilation separate.
- [x] Build the real runtime/IOP libraries with target flags and required portable adaptations.
- [x] Adapt x86-specific ReleaseMode assumptions without changing the desktop reference configuration.
- [x] Provide the CPU/null-renderer host boundary required by the runtime; distinguish it from the synthetic test NRO.
- [x] Explicitly disable unsupported optional desktop/debug/JIT/backends rather than silently using incorrect substitutes.
- [x] Link the full private generated core into the null harness with no Linux/Windows SDL3, graphics, audio or FFmpeg libraries.
- [x] Inspect ELF/NRO relocations, map, PIE and section permissions; fix read-only relocation problems at the source/compile layer.
- [x] Record runtime edits with required markers/documentation in maintained overlay patches.

## M02-11 — Parser gaps, registrations and unresolved diagnostics

- [x] Preserve the original parser-gap and macro inventory limits instead of claiming a complete semantic index.
- [x] Preprocess/compile the actual core paths and resolve relevant macro/parser gaps with compiler evidence.
- [x] Catalogue generated SDK/syscall registrations and compare native/target prototypes and counts.
- [x] Classify the 561 unhandled-instruction events and all indirect-fallback owner functions using section/map evidence. Runtime reachability needs game execution and moves to M04-12.
- [x] Confirm no diagnostic blocks the M02 synthetic workloads. Separating data or unreached metadata from executed game code needs game execution and moves to M04-12.
- [ ] Verify no required guest registration or resume target is missing or silently redirected.

## M02-12 — Host and ARM validation matrix

- [x] Add public-source synthetic allocation and packed-integer checks, separate from private game generation.
- [x] Run their first native and Switch cases; retain successful log/payload hashes.
- [x] Run relevant existing EE, VU, memory, interrupt and kernel/scheduler suites on the native reference.
- [ ] Run corresponding ARM suites with the same recorded data, flags and explicit expected results.
- [ ] Run FP/VU and completed operation-family differential tests on both architectures.
- [x] Retain failing outputs as well as fixes and final passing results; do not mask errors to close a checkbox.
- [x] Add source-only CI for eligible public CPU-test code without putting private ELF/generated game code in Actions artifacts.

## M02-13 — Target harness and resource budgets

- [x] Build a separate automatic synthetic CPU-test NRO; it contains no game code/assets and does not replace the platform NRO.
- [x] Observe application mode and a 1 MiB stack region on the real Switch.
- [x] Record zero additional workers and the initial mapped-memory readings for that synthetic harness.
- [ ] Re-run with the corrected bounded allocator and record actual arena/stack/reservation limits accurately.
- [x] Build the full real-core null-renderer harness, not only the current synthetic checks.
- [ ] Establish budgets/headroom for guest memory, runtime state, generated tables, stacks and workers under application mode.
- [x] Verify no desktop dependencies, unbounded workers/queues or hidden runtime ISO requirement in the CPU-only harness.

## M02-14 — Deterministic target execution and stop

- [ ] Execute validated scheduler/guest CPU workloads through the real runtime on ARM64.
- [ ] Record deterministic progress/result/PC/yield counters and compare with the native workload.
- [ ] Exercise clean stop, repeated run and failure cleanup without invalid guest-return or resource lifetime behavior.
- [ ] Retain exact target binary/source/configuration identity and private execution logs.
- [x] Keep all user-disc ELF/generated code out of Git, CI and release artifacts.

## M02-15 — Reversible maintained patches

- [x] Create the root series manifest pinned to the upstream commit.
- [x] Prepare a separate detached checkout below build/ without modifying the registered submodule.
- [x] Verify ordered patch checksums and record the expected staged diff outside the checkout.
- [x] Prove modified/new-file apply/reverse and repeat-application behavior with actual Git fixtures.
- [x] Prove checksum/path escapes and unowned edits are rejected and preserved.
- [x] Run actual pinned-checkout prepare/reverse/prepare with the initial empty series.

The real patch series has since passed reverse and re-apply on the disposable
checkout. The fixture result alone does not claim that.

## Hardware sessions

The procedure and required observations are in [HARDWARE.md](HARDWARE.md); results
are in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md). The first independent review
reopened the target steps above until a run from an exact candidate exists.
Compilation or native execution does not close target-execution steps. The existing
M01 deferrals remain.

## Closure workflow — only after all implementation proof

- [ ] Reconcile this checklist, all parent ledger entries and committed task evidence.
- [ ] Verify clean candidate, public-source/asset policy and required source CI.
- [ ] Pass `python tools/milestone_gate.py M02` before creating the independent reviewer.
- [ ] Obtain actual exact-candidate independent review; resolve every finding on the same branch and re-review changes.
- [ ] Record approval, pass `--require-review` and all required CI, then merge with a merge commit.
- [ ] Verify remote main before creating the next milestone branch; do not implement M03 early.

## Already scheduled outside M02

- M06-13: docking/output acceptance when the real game first reaches its opening flyover (the game has no title menu on an empty card).
- M06-14: Pro Controller/formal physical connection-state acceptance at that same trigger.
- M13-03 and M13-05 through M13-07: folder-based custom textures and dev-menu enable/reload/error handling and hardware proof (moved from M09-11 through M09-13 by the 2026-11-09 re-plan).

Those user-requested items stay tracked in the main ledger. The M01 deferrals
do not automatically defer M02's CPU/runtime execution requirements.
