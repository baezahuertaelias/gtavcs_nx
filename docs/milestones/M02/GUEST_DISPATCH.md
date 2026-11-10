# M02 guest dispatch boundary tests

Implementation: `tests/core/guest_dispatch_tests.cpp`, entry point
`int runGuestDispatchTests()`. This adds targeted M02-08 evidence beside the
existing kernel suites and `scheduler_workload.cpp`; it is not an independent
milestone review or a hardware result.

## What runs

Every guest callback has the actual `PS2_GUEST_CC` declaration and a compile-time
check against `PS2Runtime::RecompiledFunction`. The callbacks enter through
`EeScheduler::run` and `PS2Runtime::dispatchGuestBranch` or its explicit slow
counterpart. No callback calls another callback as a substitute for dispatch.

Two workloads each run through both API paths:

| Workload | Required trace | Accounted EE cycles |
|---|---|---|
| Direct call and normal return, inline fallthrough, tail jump, indirect call and normal return, explicit return to scheduler | `1234567` | 56 |
| Indirect call into an owner's secondary entry, indirect jump to deferred continuation, scheduler return to caller continuation | `1234` | 48 |

The first workload checks the branch helper's successful fallthrough result,
preserved caller return address around both calls, and the false return required
for tail/return transfers. The second verifies that a non-fallthrough target
unwinds the caller's inline continuation. The same owner function is registered
at two exact table addresses; entry PC selects its owner prologue or secondary
entry. The continuation workload must execute the secondary entry once and the
owner prologue zero times.

Callbacks check complete 128-bit argument, saved-register and guard values;
complete vector return values; guest stack pointer; return-address value and
preserved upper 64 bits; RDRAM/runtime pointer identity; and the scheduler's
guest-executing flag. Exact trace order and cycles are checked, not merely
printed. A two-second watchdog bounds each scheduler run and is joined on both
normal and exception paths.

## Table ownership and private-core compatibility

The fixture locates eight contiguous, nonzero, currently empty entries inside
the real declared table bounds. It modifies seven entries and leaves one empty.
It never overwrites a generated function. Its table guard restores the original
empty block on success, failure return or exception. Success also verifies the
block is empty before printing PASS.

This works with the public synthetic table and is designed for the private
generated table; see HARDWARE_RESULTS.md for the private-table run. The test
also verifies that an unregistered neighboring slot and an unaligned address do
not resolve, and that this low-address table does not silently accept a KSEG
address alias. Those are exact function-registration contracts; memory address
aliasing is a separate subsystem.

## Observed proof

Native build/run used the current overlaid real runtime, IOP and null-host static
libraries under `build/m02/native-runtime`, plus the existing public test table
object. It used Clang23.1.4, C++20, O2, x86-64-v3, strict contraction/rounding/
aliasing flags, static linkage and the documented platform-ABI test configuration
`PS2X_GUEST_CC_DEFAULT=1`. Data paths were isolated below
`build/m02/guest-dispatch-work`; no game or save data was loaded.

| Check | Observed result | Private log under `build/evidence/M02/` |
|---|---|---|
| Real native runtime linkage | Passed | `guest-dispatch-native-build.log` |
| Native execution, `PS2X_FAST_DISPATCH=1` | Four workloads passed, exact traces/cycles, no timeout | `guest-dispatch-native-fast-1.log` |
| Native execution, `PS2X_FAST_DISPATCH=0` | Identical four results | `guest-dispatch-native-fast-0.log` |
| Clang preserve-none declarations | Callback/function-pointer compile checks passed without `PS2X_GUEST_CC_DEFAULT` | `guest-dispatch-preserve-none-build.log` |
| ARM64 cross compile | devkitA64 GCC15.2 produced ELF64 little-endian AArch64 object | `guest-dispatch-arm64-build.log` |

The preserve-none check compiles the actual test source; it does not link it
against libraries built with a different calling convention. Its result is
declaration/type proof, not native preserve-none runtime execution.

## Integration and remaining proof

Add this source to the public runtime-test and private-core harness targets and
call `runGuestDispatchTests()` after the existing scheduler workload. No new
dependency or header search path is needed beyond those targets' existing runtime
includes and CPU options.

Switch proof, identities and table restoration are recorded in
[HARDWARE_RESULTS.md](HARDWARE_RESULTS.md). This suite does not execute
game functions, prove every generated entrypoint, replace the existing sleep/
wake/failure workload, or establish game/menu loading.
