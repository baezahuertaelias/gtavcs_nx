# M02 host, layout and concurrency checks

Implementation evidence for parts of M02-05, M02-08 and M02-09. This is not a
milestone review or a target-runtime acceptance record. All inputs are synthetic
public CPU values; no game code/data is included.

## Test entry and integration

`tests/core/host_boundary_tests.cpp` exports `int runHostBoundaryTests()`.
Define `NX_CORE_TEST_SUITE` when linking it with another main. Without that
definition it has a standalone main. It requires the same runtime/IOP include
paths and Switch SSE2NEON/FP options as the existing packed suite. It needs the
standard C++ threading/unwind support, not a PS2Runtime library or game boot.

The suite creates at most two temporary `std::thread` workers and joins them.
They exchange 20,000 plain-data payloads using release/acquire sequence and
acknowledgement atomics in both directions. A five-second deadline reports
failure rather than leaving a spin loop unbounded. This checks the ordering
contract with real threads; running only on x86 does not prove ARM ordering.

Each worker checks its zero-initialized TLS independently, sets a different
64-bit value, and checks it again before returning. The main thread's different
TLS value must survive. Later, a single waiter checks mutex-protected predicate
publication/notification and a five-millisecond false-predicate timed wait. The
timeout allows broad scheduling overhead; it is not a latency benchmark.

Nested exception throw/rethrow checks typed catches and both RAII destructors.
Aligned C++ new/delete checks 16, 32, 64, 256 and 4096-byte alignments with a
257-byte writable allocation. This complements the separate SSE allocation
adapter tests; it does not substitute for them. The compiler also asserts that
32/64-bit standard atomics are always lock-free on the selected target.

## Layout and ABI evidence

The native `NX_HOST_EMIT_LAYOUT` mode printed 42 sizeof/alignof/offsetof values
from the pinned headers using the recorded x86 Clang23.1.4 compiler. Those
observed numbers are embedded in the test file. ARM cannot emit replacement
values: the emission mode has an explicit AArch64 compile-time error. The normal
build statically asserts every observed layout value, then also checks/reports
them at execution time.

- R5900Context: 1648 bytes, alignment 16. Critical GPR, PC, instruction count,
  HI/LO, VU0, COP0, LL/SC, delay-slot, COP2 and FPU field offsets are covered.
- VU1State: 664 bytes, alignment 8. VF/VI, ACC, Q/R, PC/status, cycles, flags,
  TOP and branch fields are covered.
- Guest integer/float vector types: 16 bytes/alignment 16. Guest callback
  pointer: 8 bytes.

Both native and AArch64 GCC compilation pass these assertions. This establishes
the named layout properties; it does not compare every private interpreter
implementation field or the host STL-containing PS2Runtime class. Windows
`long` and Switch `long` deliberately have different ABIs and are not used as
guest layout types in these checks.

Byte and register checks cover little endian, 64/128-bit bit casts and lane
order, unaligned memcpy, signed/zero-extended 32-bit writes, 64-bit writes,
preserved upper GPR halves and immutable architectural register zero.

The guest-call function has the exact `PS2_GUEST_CC`-annotated type required by
`PS2Runtime::RecompiledFunction`; a static type equality check rejects mismatched
function-pointer casts. A volatile function pointer makes 4096 real non-inlined
host-to-guest-shaped calls, validating all three argument identities, a local
16-byte stack alignment, written context/memory and the caller's result checks.
The opaque runtime pointer is compared only, never dereferenced.

Native execution passes both the original Clang `preserve_none` selection and
the explicit `PS2X_GUEST_CC_DEFAULT=1` platform-ABI counterpart used by the
public runtime suites. AArch64 uses the header's existing default ABI. These
are direct ABI checks; they do not establish scheduler call/tail-call/yield/
resume behavior or actual generated registration/wrapper equivalence. Those
remain separate real-runtime workloads.

## Observed verification (2026-11-08)

Native compiler:
`build/m02/reference/build/build-tools/llvm-mingw-20261020-ucrt-x86_64/bin/clang++.exe`.
Options: C++20, O2, x86-64-v3, static, signed char, FP contraction off,
rounding-math, no strict aliasing; runtime and IOP include paths from the pristine
pin. Adding `NX_HOST_EMIT_LAYOUT` selects emission; adding
`PS2X_GUEST_CC_DEFAULT=1` selects the second calling-convention run.

Both native runs report:

```text
PASS host boundaries: layouts=42 guest_calls=4096 messages=20000 workers_peak=2 TLS/unwind/aligned-new/waits
```

Raw local logs under `build/evidence/M02/`:

| File | SHA-256 |
|---|---|
| host-layout-native.log | 2409c99d9b3897fb91ade96ece99c827f9a264936d221900fefb5170fa432c04 |
| host-boundaries-native.log | 6309b68da60a9973093dc52a7d1b96c35c2cdc063842193bb57a03d53d44abe3 |
| host-boundaries-native-default.log | 6309b68da60a9973093dc52a7d1b96c35c2cdc063842193bb57a03d53d44abe3 |

The separate devkitA64 GCC15.2.0 compilation used the official toolchain's
ARMv8/Cortex-A57, soft-thread-pointer, local-exec TLS, PIE, function/data section
and strict CPU options, pinned SSE2NEON plus the scoped allocation adapter. Its
object is ELF64 little-endian AArch64. The successful compile includes all 42
layout and atomic lock-free assertions. See host-boundaries-arm64-build.log.
The standalone object is compile proof only; integrated NRO linking and actual
Switch execution remain required. No new target result is claimed.

## Runtime-suite stack issue found

The initial `src/core_tests/runtime_main.cpp` definition of
`__nx_applet_main_thread_stack_size` is not an established libnx control. Pinned
libnx4.12.0 archive symbol inspection found no definition/reference for that
name; defining it in an NRO does not prove its initial thread has an 8 MiB stack.
The earlier observed synthetic NRO main stack region was 1 MiB. The current
runtime suites intentionally place large VU/GS state in stack frames, so the
stack budget must be implemented and measured before target deployment.

The actual SDK `switch/kernel/thread.h` exposes `threadCreate` with explicit
stack pointer/size arguments. Passing a null backing pointer requests a newly
allocated stack of the specified size. A bounded dedicated CPU-suite thread,
followed by wait/close before log shutdown, is one supported way to provide the
8 MiB suite stack without inventing a main-thread override. The root runner now
uses an explicitly allocated 8 MiB stack, waits for its owner, closes the thread
and releases the stack before log shutdown. A failed join terminates the process
without freeing a possibly live stack. Resource logging measures the actual
stack region and separates allocator usage/limit from kernel reservation. Its
target measurement is in HARDWARE_RESULTS.md.
Header/spec evidence is retained in `build/evidence/M02/libnx-stack-boundary.log`.

## newlib/POSIX audit (M02-09)

Scope: every use of the APIs below in the overlaid `ps2xRuntime` and `ps2xIOP` sources
on the Switch build. Counts come from a source search of the overlaid checkout.

| API | Sites | Switch status | Handling |
|---|---:|---|---|
| `mmap`, `munmap`, `userfaultfd` | 3 | Not compiled | Write-watch code is `__linux__` only; Switch takes the portable path |
| `pthread_setname_np` | 2 | Not compiled | `__APPLE__` and `__linux__` only |
| `socket` control server | 2 | Compiled, inactive | Runs only with `PS2X_CONTROL` or hidden mode. libnx sockets are not initialized, so M04 must leave it off or initialize them |
| `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` | 1 | Fails on Switch | Patch 0005 keeps the unavailable sentinel; diagnostics only |
| `localtime_r` | 15 lines with `localtime` | Supported | Date only, no guest timing dependency tested here |
| `std::filesystem` | 241 | Partly | Device-prefixed paths are treated as relative. Fixtures use rooted paths; production handling is M03-13 |
| `std::thread`, `std::mutex`, `condition_variable` | 20 thread sites | Supported | Host-boundary and scheduler suites run them on the console |
| `getenv` | 164 | Supported | Environment is empty unless the process sets it |
| `fork`, `dlopen`, `sysconf`, `realpath`, `nanosleep`, `fsync`, `ftruncate` | 0 | Not used | None |

Two differences were found by execution, not by this search: the `sdmc:` path behavior
and libnx taking `0x9000` bytes of heap for argv. Both are recorded in
[HARDWARE_RESULTS.md](HARDWARE_RESULTS.md). This audit does not cover the later SDL2,
GL or audio layers.
