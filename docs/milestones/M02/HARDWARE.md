# M02 consolidated Switch acceptance — pending

Complete implementation, native comparisons and source/build checks first.
The user will provide the Switch again at that point. Do not repeatedly request
intermediate launches. Then run this matrix; only after its required results
are evidenced may the milestone gate and independent review proceed.

The prior device was a Switch V2 with Atmosphere, using Sphaira in application
mode. Its previous address was 192.168.1.107, with anonymous FTP on port 5000.
Confirm the current address/listener when the session begins. An old address or
successful upload does not prove which payload executed or that a test passed.
No Pro Controller, dock, button sequence or game assets are required here.

## Final payloads

| Payload | Scope | Required SD log |
|---|---|---|
| `gtavcs_runtime_tests.nro` | Public runtime, integer/FP/layout/thread/dispatcher and synthetic compiled-VU checks | `sdmc:/switch/gtavcs_nx/runtime-tests-M02.log` |
| `gtavcs_private_core.nro` | Full private generated EE/VU linkage/registration plus public synthetic guest-dispatch workloads | `sdmc:/switch/gtavcs_nx/private-core-M02.log` |

The first comes from the inspected public build bundle. The second is produced
locally with the explicit private archive input; never put it, its ELF/map or
game-generated source in Git, public CI artifacts or releases. The standalone
public CPU NRO is also built for diagnosis, but the consolidated runtime payload
already includes those same suites. A third redundant launch is unnecessary.

Both require the existing checked `gamefolder` root and full application memory.
No game function or game/menu boot is attempted. A black/unchanged display while
CPU checks run is expected; they log to SD and return automatically. Runtime
file fixtures use only the dedicated `test-work-M02/tmp` tree, separate from
actual game assets and saves. Do not rename or remove the user's asset tree.

## Session sequence

1. Preserve the final candidate, overlay/dependency/package-lock identities and
   inspected NRO hashes. Archive any previous logs before another run.
2. With Sphaira FTP available, retain a baseline crash-report filename listing
   and verify the expected SD root. Do not infer absence of crashes from a
   failed listing or connection.
3. Enable nxlink/netloader, send the public runtime NRO to its own SD filename
   using the pinned nxlink tool, and let it return automatically. Re-enable
   Sphaira FTP if needed, retrieve the complete log and independently hash the
   SD NRO. Compare it with the exact local payload.
4. Enable nxlink/netloader again for the private core NRO, then retrieve its log
   and SD payload hash in the same way. Keep all private outputs local.
5. Compare the crash-report listing with the baseline and retain any new report
   privately. A missing/incomplete log, failed suite, hash mismatch, unexpected
   exit or new crash remains an unresolved result to diagnose and fix.

nxlink uses the recorded container's `/opt/devkitpro/tools/bin/nxlink` with
`-a <current-IP> -r 2 -p /switch/gtavcs_nx/<payload>.nro <local-payload>`.
These harnesses use SD stdout, so no nxlink stdout server or `-s` is needed.

## Required observations

- Public runtime: all 300 reconciled upstream cases pass; packed/FP/layout,
  allocation/concurrency and compiled-VU suites pass with their stated counts.
  Expected upstream numeric limitations stay explicitly identified, not counted
  as exact PS2 hardware matches. Final aggregate and worker result must be zero.
- Scheduler: three trace-326/24-cycle/result-0x1e38b878 runs, one sleep/wake each,
  clean stop and failure-unwind/reset. Guest dispatch must retain its exact
  call/tail/return/alias/continuation traces and 128-bit register/RA checks.
- Compiled VU: strict compiled execution is actually selected; cycles, state,
  flags, data/GIF output and budget resumptions match; changed uploaded bytes
  invoke the interpreter and restoring them reselects compiled code.
- Private core: table bounds hold and 236,304 slots are assigned before synthetic
  probes; probes restore their empty slots and finish without executing game
  owner bodies. Later game boot still owns unresolved game reachability checks.
- Resources: record HOS/applet mode, actual stack region, 256 MiB allocator
  limit/arena, rejection of an oversized allocation, allocator observations and
  worker budget. Kernel-mapped loader reservation is not live allocation usage.
  The public suite uses an explicit 8 MiB worker stack; temporary test threads
  are joined before log shutdown. Record actual readings, not requested sizes.
- Both launches return cleanly, logs close completely, SD payload hashes match,
  and no new crash report is produced.

Keep every failed attempt and its source/payload identity alongside the fix and
final passing run. These CPU results do not establish game/menu loading,
controller/docking acceptance, rendering, audible game audio or gameplay speed.
