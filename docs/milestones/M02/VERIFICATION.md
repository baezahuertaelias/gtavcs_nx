# M02 CPU foundation

Branch `milestone/m02-arm64-core`, based on independently reviewed M01 merge
`d8103d9f6477fa6c0c2cd9863e73483f2b9eeb64`. Upstream remains pinned and pristine
at `ecf150e53f87e4656dfe81eacd922839d952b282`. No M02 reviewer or game-port
acceptance is claimed. Root `gtavcs.nro` remains the M01 platform stub.

## M02-01: isolated desktop reference

Commands (2026-11-08, Windows): clone the pinned submodule into ignored
`build/m02/reference`, then run its unchanged tools:

```powershell
python build/m02/reference/tools/build.py --recompile 'C:/Users/Gamer/Desktop/gtavcs_nx/GTA Vice City Stories Esp.iso' --quick --verbose
$env:VCS_DATA_DIR='C:/Users/Gamer/Desktop/gtavcs_nx/build/m02/reference-userdata'
$env:PS2X_GS='hw'
$env:VCS_SKIP_MOVIES='0'
python build/m02/reference/tools/run.py 30 m02-reference --iso 'C:/Users/Gamer/Desktop/gtavcs_nx/GTA Vice City Stories Esp.iso' --hidden
```

The explicit disc check passed as SLUS-21590 USA v1.00. The executable SHA-1 is
`d1832508caf0990c2291c0f255c96a889cce9f88`. No other disc or user saves were
searched or changed. The new userdata directory contains isolated mc0/mc1/cache.
Upstream provisioned its checksum-pinned llvm-mingw 20261020 toolchain: Clang
23.1.4, x86_64-w64-windows-gnu, native Ninja 1.13.2/CMake 4.4.4. AVX2, LTO OFF,
FP contraction OFF and the original generated guest calling convention were
retained. `build_info.json` records source/config identity.

The 806-step desktop build passed in its reported 3m31s compile stage. The
30-second hidden run returned the expected runner timeout 124 after 261 log
lines. It reported D3D11 on NVIDIA GeForce RTX 4060, original 30 Hz cap, guest
ELF entry 0x1dd6a0, native sound/IOP-service initialization, real disc sector
lookups and STINGS.PSS playback (321 pictures, no dropped pictures reported).
This reference baseline covers startup/intro, not gameplay or a main-menu
acceptance claim. The timeout intentionally killed the reference process; it is
not target orderly-shutdown evidence. No PS2X_GS=gpu run was attempted.

Private logs:
`build/evidence/M02/desktop-build.log` SHA-256
`174bafed09bc70193cbc4d2c0a209c52944d69340f5d7661a307ede4f6c67836`;
`build/m02/reference/build/run/m02-reference.log` SHA-256
`a93a5b542f9d1d9185316d1f229dbb1812a66819cefeb2dd83ac7761682ac9a9`.

## M02-02/03: native tools and private generation

The unchanged upstream pipeline built and executed `ps2_recomp.exe` on Windows.
`python build/m02/reference/tools/build.py --target ps2_analyzer --quick --verbose`
also passed (32 build steps, reported 11s). The metadata audit verifies both
tools and the desktop program are AMD64 PE binaries. Target compilation is in
a separate Docker/official Switch-toolchain project; it never executes an ARM
recompiler on the host. No generator or game inputs are put in CI.

`python tools/audit_core.py --reference build/m02/reference` observed:

- 10,383 discovered/generated functions; 10,210 recompiled and 173 configured
  stubs in the raw upstream report, no decode failures.
- 226,519 additional resumable entries; 236,304 registration assignments to
  10,383 unique owner symbols. These are different counts, not interchangeable.
- 1,196 indirect-control-flow warnings and 561 unhandled-instruction error events;
  all owners/first diagnostic addresses are retained in generation_metadata.json.
  Secondary indirect addresses/messages remain in the private raw recomp.log.
  This records unresolved generation diagnostics; it does not declare them safe.
- 50 generated VU programs and 3,032 compiled instruction pairs reported by the
  native tool. There are 10,385 EE+VU C++ files in the private generated tree.
- Stamp inputs `765d3a881157f3188aacc3e06cae68d76fece227`, source commit equal to
  the pinned upstream. Native executable hashes, generated construct/intrinsic
  counts and every warning/error owner are in generation_metadata.json.

Private inputs/output remain in ignored build/m02/reference; tracked metadata
contains no ELF/game/generated bodies or instruction bytes. Upstream's VU output
comment still names a legacy SLUS_206.39; the actual input was the validated
SLUS_215.90, verified by ELF hash and explicit invocation. That stale comment is
not used as input identity. All code-generation diagnostics still need their
owning compatibility/scheduler/numeric tasks before M02 can pass.

## M02-15: maintained overlay prerequisite

`tools/upstream_overlay.py build/m02/switch-source` creates a separate detached
checkout, verifies the base and every ordered patch checksum, and records its
expected diff outside the checkout. `--reverse` restores exactly the pinned
tracked tree. It rejects destinations outside build/, changed patch contents,
unowned tracked/untracked files and edits inconsistent with its recorded diff.
It never edits or commits inside the registered submodule.

Four focused Git-fixture tests passed: real modified/new-file application and
reversal, repeat application, preservation/rejection of a user edit, bad patch
checksum and path escape. Actual pinned checkout prepare/reverse/prepare also
passed with the initial empty series. Port patches will be added to
patches/upstream/series.json as required; no runtime changes are yet claimed by
that empty series. Logs/test output were observed locally, not invented CI runs.

## Initial ARM64 evidence — remaining tasks stay open

The pinned M01 container's devkitA64 GCC 15.2.0 preprocesses plain char as unsigned,
64-bit long/pointers and little endian. The existing USE_SSE2NEON header path was
tested with pinned SSE2NEON v1.9.1/commit 92f6de174717aef09033ad21568d5bb9e5470404.
The first compile failed because newlib does not declare or provide the
posix_memalign symbol used by SSE2NEON's default allocation helper. Source/header
and libc/libnx symbol inspection established the mechanism; adding a feature
define alone would not supply the missing symbol.

The port-owned switch_sse2neon_alloc.h uses the supported memalign/free pair,
preserving small-alignment normalization and rejecting invalid alignments. It
changes allocation helpers only, not vector arithmetic. A 16-case host allocation
contract test passed (aligned allocation, write/read/free, invalid alignment,
null/zero size). The actual strict C++20 EE macro/runtime/VU header probe then
compiled to AArch64 ELF with 16-byte vector size/alignment assertions. This is
compile-only proof: full generated/runtime linking and target execution are open.

Raw probe logs: build/evidence/M02/arm64-header-probe.log (original failure),
arm64-header-probe-alloc.log (passing ELF header, SHA-256
`3b1fd92ac16cbbe35f3dbdf5a59c3e574f84bc595b5eec721f146b26b0ebc8b9`),
alloc-contract-host.log. The private generated ARM build runs separately with
strict FP/contraction/aliasing flags and precise SSE2NEON options. Its initial
missing Kernel/Stubs include was preserved in failure logs and corrected using
the owning upstream CMake include path. No claim of full ARM compatibility yet.

simd_candidates.json maps all 661 original lexical SIMD sites to operation
families and parsed owning functions; null owners/manual families explicitly
retain parser uncertainty. Numeric, unaligned-access, rounding, scheduler and
ABI equivalence still require M02-04 through M02-14 evidence.

## Packed integer and allocation execution on Switch

The public-source scalar-reference suite covers 42 arithmetic/comparison/bitwise
operation variants, 4,096 edge/seeded input pairs per variant, packing/QFSRV
cases, and unaligned/wrapping RDRAM reads/writes: 173,066 vector comparisons.
The reference was built with the pinned native Clang and strict FP/aliasing
flags, and passed. An initial test incorrectly omitted the real generator's
`ctx->sa & 0x7f` boundary for QFSRV; inspecting translateQFSRV corrected the test
invocation rather than changing guest arithmetic. The initial output is retained
as packed-integer-initial-sa-contract.log.

The separate public CPU-test NRO contains these synthetic tests and allocation
checks only; it does not link generated game code or boot the runtime/game.
It uses the official Switch CMake toolchain, locked SSE2NEON content, the same
strict CPU flags and a checked gamefolder chdir. It returns automatically and
closes its SD log before process exit; no button/dock test is involved.

After the user confirmed the Switch was available, nxlink delivered that NRO to
the separate gtavcs_cpu_tests.nro path. The actual Switch (HOS 22.5.0, applet=0)
passed all 173,066 vector comparisons and the 16 allocation cases. It reported
char_signed=1, a 1,048,576-byte stack region and zero additional workers. Both
test results were zero, and no new Atmosphere crash-report entry appeared.

FTP retrieval independently hashed the SD NRO:
`433eaa9421fa1f928138621f0a34248d9bbe7f896b220cd486c5027053f2f7a1`,
matching local build/m02/cpu-tests/gtavcs_cpu_tests.nro. Retained log
build/evidence/M02/hardware/cpu-tests-M02.log has SHA-256
`e38c60a9d2f299ba025e9b594bcfd7c1470d0a89dbf0417e528704a2500c9c97`;
cpu-tests-result.json records the payload/log identities and empty new-crash list.
The 30,467,864 ns suite timing is a synthetic test trace, not game performance.
Remaining M02-05/M02-06 operation/layout coverage is tracked explicitly in TASKS.md.

## Heap-override and build-partition follow-up

The first CPU NRO requested a 128 MiB weak __nx_heap_size but reported
3,343,908,864 total / 3,339,763,712 used bytes from the kernel. This was not a
verified 128 MiB allocator budget. Inspection of pinned libnx v4.12.0 init.c and
env.h confirms loader-provided heap overrides take precedence over that weak
size symbol. Kernel-mapped reservation must not be reported as live allocations.

The prepared correction uses the documented __libnx_initheap hook and bounds
fake_heap_start/end to a 128 MiB arena within the loader-provided region. It also
adds an over-budget allocation rejection check and labels mapped/allocator
measurements separately. This source builds, but its second nxlink connection
was refused; it has not yet been validated on hardware. The earlier integer/
allocation PASS must not be used to claim the new arena limit passed.

The original private generated-code build reached 324/326 steps; the remaining
unity unit combined the 236k-assignment registration constructor with the large
VU source. A process snapshot showed over 16 minutes of active 99.8%-CPU
compilation and approximately 2.3 GiB RSS on that single unit. The process was
alive, not presumed hung. That specific attempt was explicitly cancelled to
change the source partition; its log is retained as
generated-arm64-large-unity-cancelled.log and large-unity-process.log.

The revised build keeps registration and VU as separate O2 translation units,
matching upstream's existing separation of VU from EE unity batches, and omits
unused DWARF only on those two large sources. No functions, operations or numeric
checks were removed and no FP flags relaxed. It reuses unaffected objects and
completed in the project-specific Linux Docker volume. The archive contains all
327 AArch64 members, is 1,769,713,766 bytes and has SHA-256
`7549019d95a5fb2c0ee488867fcc62efaaf4e7f8b11a3995b0f961d79648d74a` at this point.
Later refreshes supersede it; the final identity is in HARDWARE_RESULTS.md.
`generated-arm64-build.log`, `generated-arm64-archive-headers.log` and
`generated-arm64-members.txt` retain completion/member proof. This establishes
strict C++20 GCC compilation of the full generated source, not runtime/numeric
or guest-ABI equivalence. M02-04 stays open for those remaining checks.

## Floating-point test preparation and consolidated hardware session

The new public CPU suite uses all 174 upstream hardware-vector rows and compares
every result with an explicitly emitted x86 Clang23 reference. The native run
passed: 163 rows match hardware exactly, 10 retain upstream's one-ULP differences
and one retains its denormal limitation. The latter 11 rows are not claimed as
hardware-exact. Fifteen exceptional/truncating conversions, rounding-mode changes,
non-fused multiply/add and four vector lanes are also checked. These checks do
not replace compiled VU/interpreter pipeline and scheduler tests.

The combined allocation/integer/FP NRO builds successfully. The corrected heap
arena and floating-point results remain untested on Switch. The user will make
the Switch available after all M02 implementation is ready for a consolidated
test session; do not request repeated intermediate device launches.

## Real runtime compilation — partial M02-10 evidence

The separate `cmake/runtime` project inventories the pinned real runtime sources
and all kernel files, plus the existing IOP library. It disables FFmpeg and the
desktop debug/graphics backends, preserves precise SSE2NEON and strict FP flags,
and leaves the desktop project's Clang gate unchanged.

The initial compile exposed undeclared fseeko, missing INADDR_LOOPBACK and missing
CLOCK_THREAD_CPUTIME_ID. Newlib libc contains fseeko; requesting POSIX.1-2008
declarations fixes that boundary without narrowing offsets. The maintained
overlay parses the same loopback address with inet_pton, reports unavailable
optional CPU-spike timing where no thread clock exists, uses the existing
USE_SSE2NEON render-worker boundary, and provides explicit PS2X_HEADLESS UI
exclusion while preserving perf counters/CSV. Both actual runtime and IOP static
libraries then compiled for ARM64 (`runtime-arm64-portability-v2.log`).

The patch is kept under `patches/upstream`, includes upstream-required markers
and documentation, and was applied/reversed/reapplied in the disposable pinned
checkout. The registered submodule remains pristine. Compilation emitted an IOP
virtual-ISO directory-record warning that remains to be investigated; no runtime
PASS is inferred from the archive. The linked public runtime suites pass on the
native host; target results are in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md).

## Native runtime, guest dispatch and numeric matrix

The original public runtime suites were first compiled with the native Clang
and the documented platform guest-ABI configuration. Their unannotated test
callbacks do not type-check with preserve_none; mismatched function-pointer
casts were not added. Both the pristine pin and initial overlay failed the same
six cases with accurate VU timing: their fixtures still expected the old async
stack pool, old IOP heap floor, immediate rather than batched IOP progress,
unframed VIF IMAGE continuation, and old JR visibility behavior. Existing source
and PS2RECOMP_PATCHES documentation establish the changed contracts. The revised
fixtures retain their substantive data/stack/control assertions and all 300
cases pass. Original failures and pristine comparison remain in private logs.

The consolidated native run additionally passes three deterministic
sleep/wake/resume/stop repetitions (trace 326, 24 cycles, result 0x1e38b878),
dispatcher failure/unwind/reset, four direct/indirect/tail/return/alias cases,
273,039 packed scalar-reference groups, 174 FP vectors and 15 conversions,
42 layout values, 4,096 correctly typed guest calls, and 20,000 threaded
release/acquire handoffs plus TLS/unwind/aligned allocation/timed waits.
`runtime-native-complete.log` records the integrated PASS. This is native CPU
proof, not a Switch or game-loading result. Dedicated boundary compilations also
passed the original Clang preserve_none configuration.

Details: [PACKED_COVERAGE.md](PACKED_COVERAGE.md),
[GUEST_DISPATCH.md](GUEST_DISPATCH.md),
[HOST_BOUNDARIES.md](HOST_BOUNDARIES.md) and
[CORE_AUDIT.md](CORE_AUDIT.md). The full source audit accounts for every original
SIMD/parser site and expanded SDK declaration; reserved diagnostic selectors are
not relabeled as proven unreachable game code.

## Compiled VU correctness policy and generation refresh

The real generator-produced public fixture found two inherited throughput
differences: an overflow result and early completion relative to the fast
interpreter. The Switch now selects the already-emitted strict compiled variant
with exact-range MADD/MSUB handling and full ready/stall/settle bookkeeping.
Desktop default selection is preserved. Six public synthetic programs and 20
cases pass native compiled/interpreter comparisons with 19 actual generated
entries and 124 generated pairs. They compare cycles, flags, data/GIF output,
rounding, branches, Q/P and budget resumptions; changed uploaded bytes fall back
without a generated call, and restoring bytes reselects compiled execution.
No user-game microcode is present in the public fixture.

[COMPILED_VU.md](COMPILED_VU.md) records the failures, strict-policy fix and exact
reproduction command. The overlaid host recompiler was rebuilt and the private
EE/VU output refreshed. All 10,386 EE files are byte-identical to the original
reference; the VU registrar gained the strict selection. Metadata only is kept in
[overlay_generation_update.json](overlay_generation_update.json). The updated
private ARM archive and NRO were rebuilt; see HARDWARE_RESULTS.md for the target run.
