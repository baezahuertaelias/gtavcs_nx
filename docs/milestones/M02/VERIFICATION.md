# M02 CPU foundation — in progress

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
