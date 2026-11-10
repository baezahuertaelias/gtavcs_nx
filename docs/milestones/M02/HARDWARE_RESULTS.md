# M02 real-Switch results

Final session on 2026-11-09, on the user's Switch V2 (HOS 22.5.0, `hosversionGet` =
`00160500`, Atmosphere), launched through Sphaira nxlink in application mode
(`applet=0`). Procedure: [HARDWARE.md](HARDWARE.md). No emulator result is used.

These results cover the CPU core, runtime and synthetic workloads only. No generated
game function ran. They say nothing about game boot, rendering, audio, controllers,
docking or gameplay speed.

## Payload identity

Both final payloads were built from clean commit
`b67ec6d2a25c3c09a6c6fd60c724d34ddbadfc52`. The public manifest records
`root_dirty=false`. Later commits change only this ledger. Each SD copy was read back
over FTP and hashed after its run. Records are retained outside Git in
`build/evidence/M02/hardware/session2/`; `transcript.txt` holds every read-back hash,
nxlink send and listing digest in order.

| Payload | SHA-256 | Bytes | SD read-back matches |
|---|---|---:|---|
| `gtavcs_runtime_tests.nro` | `1cb791e8c94404ebacc5c40d023fa122ccf34353b7d3d4612e2f507b66f8d865` | 3,270,853 | yes |
| `gtavcs_private_core.nro` (local only) | `c4d16f97215deea8e80c0af5d69a0a4fdcfc71d095a8ac0b50a453140640fe37` | 45,975,749 | yes |

| Final log | SHA-256 |
|---|---|
| `runtime-tests-M02.log` | `8e0d94f84591cee2d0d05ea76660b341e53242d2c3f0567f88a72210c69a2c69` |
| `private-core-M02.log` | `11124a5a6bae304de6480b9ec193ec13db3198ece133334ba3bd5687295295b5` |

### Private build inputs

`private-manifest.json` holds hashes and counts only. Every one of the 287 public
upstream inputs in the private build's source tree equals the public manifest of the
same commit: 0 missing, 0 changed.

| Input | SHA-256 |
|---|---|
| Generated archive (1,768,078,598 bytes) | `c41d8e6aeabcb26374b992133b0cb54ad45311d6cb28941be51488f2f18513b0` |
| Recompile stamp | `7db0d35ac22d29cdcb83df7f8afa39d28ec51519e53d93d05af91a24f8149c72` |
| Generated registration source | `271eb441c4b7ce6e469b923b14ef36d3975e6d525336bda30643daa6c67a988e` |
| Private runtime library | `27edc31db90be87cab04facf291a92fc88f8caa7ef90bda8df4a4a9b0c3a8fc3` |

## Public runtime payload

```text
Total Tests: 337  Passed: 337  Failed: 0
PASS scheduler workload: repeated run/yield/wake/resume/stop; failure unwind/reset; table slots restored
PASS guest dispatch: direct/indirect calls, tail/return, exact owner aliases, deferred continuation, 128-bit registers; empty slots restored
PASS compiled VU: public_programs=6 cases=20 generated_calls=19 generated_pairs=124 identity_fallback_calls=0 rounding/budgets/branches/Q/P/XGKICK
PASS IOP directory bounds: 38 checked sizing/encoding/capacity/overflow cases; no large allocations
PASS packed integers: 273039 checks (vector/scalar groups), saturation/lanes/shifts/unaligned wrapping accesses
PASS float suite: native_rows=174 hardware_exact=163 inherited_one_ulp=10 inherited_denormal=1 conversions=15 rounding/non-fused/vector-lanes
PASS host boundaries: layouts=42 guest_calls=4096 messages=20000 workers_peak=2 TLS/unwind/aligned-new/waits
PASS Switch allocation contract: 16 alignment cases, write/read/free, null/zero size
M02 runtime suites finished failures=0
[runtime-tests] worker_stack_bytes=8388608 launch_result=00000000 result=0
```

**What the 337 are.** The 300 cases of the earlier runner, plus 37 runtime cases from
upstream's `ps2_runtime_expansion_tests.cpp` and `ps2_float_semantics_tests.cpp`.
Six of the 337 have expectations revised by overlay patch 0001, to match upstream's
changed contracts: async stack pool, IOP heap floor, batched IOP progress, VIF IMAGE
continuation and JR visibility. VERIFICATION.md records the reasons.

**What is not run on the Switch.** Three cases assert text emitted by the host
`CodeGenerator` and are compiled out by patch 0006. The `code_generator`,
`r5900_decoder`, `elf_analyzer` and `ps2_recompiler` suites test the host recompiler.
The generator stays a host tool, so none of these belong on the target, and M02 did
not run them separately on the host. Generated output is covered instead by the
audit in [TOOLCHAIN_DECISION.md](TOOLCHAIN_DECISION.md).

`identity_fallback_calls=0` counts generated calls during the changed-bytes run:
changed code took the interpreter, and restoring the bytes reselected compiled code.

The float suite's 10 one-ULP rows and 1 denormal row are the upstream numeric
limitations already identified in the x86 reference. They are not exact PS2 matches.

Scheduler, three runs: `steps=3 trace=326 yields=1 wakes=1 cycles=24
result=1e38b878 timeout=0`. Guest dispatch, four configurations: `trace=1234567
cycles=56` for owner entries and `trace=1234 cycles=48` for alias continuations.

The stack-alignment check now tests the frame address delivered at the guest
boundary. It previously tested a local declared `alignas(16)`, which could not fail.

## Private core payload

```text
[core-table] base=00100000 end=0048053c slots=917839 assigned=236304 bounds=1
[core-table] digest=ee732423aadfb7f6
[private-core] linkage/constructor/synthetic-workload check PASS result=0
```

The same scheduler, guest-dispatch and IOP-directory checks passed against the full
generated table. Synthetic probes restored their empty slots. No game owner body ran.

**Table check.** `tools/audit_private_core.py` reads every assignment in the generated
registration source and resolves its target in the linked ELF: 236,304 slots, 10,383
distinct targets, 0 unresolved, 0 ambiguous, 0 index/address mismatches. It computes
digest `ee732423aadfb7f6` over slot index and target offset. The console computed the
same digest from its live table. So no aliased or resume slot is missing or redirected
compared with the generator's list. Whether that list is complete for the running
game is M04-12.

## Native comparison

The Switch log was compared with `runtime-native-final.log`, built from the same six
patches and sources as the candidate by Clang 23.1.4 on x86-64. After removing colour
codes, stderr text sharing a `[Run]` line and unique temp names, both logs were sorted
and compared. All 337 test names and results, every counter, and the scheduler and
guest-dispatch lines are identical. The only remaining difference is the Switch-only
allocation suite. Switch-only HOS and `[resources]` lines and stderr ordering were
outside the comparison.

## Resources (actual Switch readings)

| Item | Runtime suites | Private core |
|---|---:|---:|
| Allocator limit | 268,435,456 | 268,435,456 |
| Allocator arena after libnx argv block | 268,398,592 | 268,398,592 |
| Allocator in use, start | 8,935,856 | 548,080 |
| Allocator in use, end | 21,405,440 | 661,808 |
| Allocator obtained from sbrk, end | 103,198,720 | 41,537,536 |
| Stack region | 8,388,608 (explicit worker) | 1,048,576 (loader main) |
| Temporary workers, declared by the harness | 3 | 1 |
| Over-budget allocation rejected | yes | yes |

Kernel-mapped readings were `mapped_total=3343908864` and `mapped_used=3339763712`
in both payloads. That is the loader's reservation, not live allocation.

`allocator_obtained` is the end-of-run malloc arena. It is the best available
high-water reading, not a sampled peak. Budgets are in
[TOOLCHAIN_DECISION.md](TOOLCHAIN_DECISION.md).

## Crash reports and SD state

`atmosphere/crash_reports` was listed before the first payload and after each one.
All listings are identical, 130 entries. `gamefolder` was listed the same way and
held only the existing `gtavcs.log` every time.

## First session, for the record

Retained in `build/evidence/M02/hardware/session-2026-11-09/`, at or before commit
47c1eef. It is not evidence for the candidate above.

1. **Attempt 1, commit 6698432, `result=1` before any suite ran.** The resource check
   required `arena == budget`. libnx `argvSetup` takes exactly `0x9000` bytes from the
   heap start when the loader passes arguments (seen in the disassembly of `argv.o`).
   The check now accepts that exact reservation only when `envHasArgv()`.
2. **Attempt 2, 290 of 300, 10 file-path failures.** The fixture set `TMPDIR=sdmc:/...`.
   `std::filesystem` under newlib treats a device prefix as relative, so paths became
   `sdmc:/.../gamefolder/sdmc:/...`. The fixture now uses a rooted path.
3. **Attempt 2 side effect.** Memory-card tests default to `cwd/mc0`, so empty `mc0` and
   `mc1` appeared in `gamefolder`. They were removed, and the suites now run from
   `test-work-M02`. The first session kept no listing of this; the second session's
   listings show the fix holds.
4. **Attempt 3, public 300 of 300.** The private payload run then was two launches of
   one binary with byte-identical logs. Independent review found that binary linked a
   runtime without patch 0005, so both payloads were rebuilt and rerun.

## Carried into later milestones

- `std::filesystem` does not understand `sdmc:` as a root: M03-13.
- Reachability of unhandled-instruction events and indirect-fallback owners in code
  that actually executes: M04-12.
- First execution of generated game code: M04.
