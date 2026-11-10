# M02 real-Switch results

Session of 2026-11-09 on the user's Switch V2 (HOS 22.5.0, `hosversionGet` =
`00160500`, Atmosphere), launched through Sphaira nxlink in application mode
(`applet=0`). Procedure: [HARDWARE.md](HARDWARE.md). No emulator result is used.

These results cover the CPU core, runtime and synthetic workloads only. No game
function ran. They say nothing about game boot, rendering, audio, controllers,
docking or gameplay speed.

> **Status:** this describes the first session, at commit 47c1eef. Independent
> review found that the private payload was not built from the exact overlay and
> that some observations have no retained record, so this is not final evidence. A
> second session from an exact candidate replaces it.

## Payload identity

The runtime payload was built from the sources in commit 47c1eef. The private
payload linked a runtime library without overlay patch 0005.

| Payload | SHA-256 | Bytes | SD copy matches |
|---|---|---:|---|
| `gtavcs_runtime_tests.nro` | `bd0f3cd8d7f9bf57e765e881dbac063de7ee72d9e84ab24487b3360816180dbc` | 3,115,205 | yes |
| `gtavcs_private_core.nro` (local only) | `9cb7dfa407beff9b549b141e8d476e7fc14be204800a7e00db8adf36334d4280` | 45,975,749 | yes |

Logs, listings and earlier attempts are retained outside Git in
`build/evidence/M02/hardware/session-2026-11-09/`.

| Final log | SHA-256 |
|---|---|
| `runtime-tests-M02.log` | `90fd1e5004b201017b71b33f6a057f7288e75a73df294ee27f046d0d4af7ed59` |
| `private-core-M02.log` | `20a8527b64b79fa6ef32440f2e20b2a78345419286f349fa594f59a01b06b84f` |

## Public runtime payload

```text
Total Tests: 300  Passed: 300  Failed: 0
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

`identity_fallback_calls=0` counts generated calls during the changed-bytes run:
changed code took the interpreter, and restoring the bytes reselected compiled code.

The float suite's 10 one-ULP rows and 1 denormal row are the upstream numeric
limitations already identified in the x86 reference. They are not exact PS2 matches.

Scheduler, three runs: `steps=3 trace=326 yields=1 wakes=1 cycles=24
result=1e38b878 timeout=0`. Guest dispatch, four configurations: `trace=1234567
cycles=56` for owner entries and `trace=1234 cycles=48` for alias continuations.

## Private core payload

```text
[core-table] base=00100000 end=0048053c slots=917839 assigned=236304 bounds=1
[private-core] linkage/constructor/synthetic-workload check PASS result=0
```

The same scheduler, guest-dispatch and IOP-directory checks passed against the full
generated table. Synthetic probes restored their empty slots. No game owner body ran.

## Native comparison

The Switch log was compared line by line with the x86 reference
`runtime-native-peak-memory.log`, after removing colour codes and unique temp names.
All 300 test names, results, counters, scheduler lines and guest-dispatch lines are
identical. The only differences are three lines where stderr text interleaves with a
`[Run]` line, and the Switch-only allocation suite.

## Resources (actual Switch readings)

| Item | Runtime suites | Private core |
|---|---:|---:|
| Allocator limit | 268,435,456 | 268,435,456 |
| Allocator arena after libnx argv block | 268,398,592 | 268,398,592 |
| Allocator in use, start | 8,935,856 | 548,080 |
| Allocator in use, end | 20,580,864 | 661,792 |
| Allocator obtained from sbrk, end | 102,871,040 | 41,537,536 |
| Stack region | 8,388,608 (explicit worker) | 1,048,576 (loader main) |
| Temporary workers, declared by the harness | 3 | 1 |
| Over-budget allocation rejected | yes | yes |

Kernel-mapped readings were `mapped_total=3343908864` and `mapped_used=3339763712`
in both payloads. That is the loader's reservation, not live allocation.

`allocator_obtained` is the end-of-run malloc arena. It is the best available
high-water reading, not a sampled peak. It leaves 60% of the 256 MiB test arena free.

## Faults found and fixed during the session

Every attempt is retained. All three were harness or environment defects. None was
a CPU, numeric or scheduler difference.

1. **Attempt 1, commit 6698432, `result=1` before any suite ran.** The resource
   check required `arena == budget`. libnx `argvSetup` takes exactly `0x9000` bytes
   from the heap start when the loader passes arguments (seen in the disassembly of
   `argv.o`). The check now accepts that exact reservation only when `envHasArgv()`.
2. **Attempt 2, 290 of 300, 10 file-path failures.** The fixture set
   `TMPDIR=sdmc:/...`. `std::filesystem` under newlib treats a device prefix as
   relative, so paths became `sdmc:/.../gamefolder/sdmc:/...`. The fixture now uses a
   rooted path, which stays on the device selected by `chdir`.
3. **Attempt 2 side effect.** Memory-card tests default to `cwd/mc0`, so empty `mc0`
   and `mc1` appeared in `gamefolder`. They were confirmed empty and removed. The
   suites now run from `test-work-M02`. `gamefolder` was unchanged after the final
   runs of both payloads.

One nxlink send failed with `Connection to 192.168.1.107 failed` and succeeded on
immediate retry with the console reachable. No payload ran from the failed send.

## Crash reports

`atmosphere/crash_reports` was listed before the first payload and after the last.
The listings are identical. No payload produced a report.

## Carried into M03

`std::filesystem` does not understand `sdmc:` as a root. Runtime paths built from a
device-prefixed configuration value are therefore wrong without error. The M03 folder
VFS must pass rooted or cwd-relative paths into runtime code, and it must test that.
