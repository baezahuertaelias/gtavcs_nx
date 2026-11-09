# M02 source ownership and diagnostic audit

This is implementation evidence, not independent review or target-execution
approval. The registered upstream remains at
`ecf150e53f87e4656dfe81eacd922839d952b282`. The metadata audit checks that pin,
the original source-file hashes, supported ELF identity, generation stamp and
every original diagnostic event. It emits no game body or instruction word.

## All SIMD sites have an explicit owner

[core_simd_scope.json](core_simd_scope.json) retains every original lexical site:

| Scope | Sites | Acceptance owner |
|---|---:|---|
| Target runtime/generated-code dependency | 333 | M02-05/06/07/10 |
| Native host generator emitting target expressions | 285 | M02-02/03; generated expressions still need target numeric proof |
| Game world/RSL, game-specific VU0 hook, desktop debug UI | 41 | M04-12, M06-01/03, M09-01/02 as recorded per site |
| Optional paraLLEl-GS backend | 2 | Excluded from the agreed SDL2/OpenGL baseline |
| **Total** | **661** | No omitted sites |

The 90 null function owners in the syntax-only index are resolved from their
actual source lines: 67 macro definitions, nine include boundaries, six function
declarations, five register/member declarations, two type aliases and one
comment-only match. They are not silently invented function definitions.
The remaining 571 records retain their detected function owner. Custom variable
shifts, byte shifts, wait hints and AVX256 families now have explicit operation
classes. This ownership resolution does not certify the arithmetic.

The compiler probes include the runtime's macro, scheduler, GS and compiled-VU
headers. Native `-march=x86-64-v3` enables AVX2 in this isolated probe; ARM does
not define `__AVX2__` and its preprocessed probe contains zero `_mm256_*` tokens.
The target therefore selects the supplied scalar fallback in guarded VU helpers.
This is a branch-selection observation, not a claim that fallback numerics have
already run on Switch or that the unchanged desktop baseline used these flags.

## Parser gaps and macro-generated declarations

[core_parser_scope.json](core_parser_scope.json) accounts for all 667 original
syntax-parser gap records: 535 target headers, seven target source locations,
six host-generator locations, one excluded desktop runner, 115 later-milestone
locations and three optional Vulkan-backend locations. The header compiler
probes include every gap-bearing runtime header. Full runtime/IOP compilation
provides compiler evidence for the seven target source locations; the native
host-tool build covers its six. The original parser index remains a syntactic
inventory, not a completed semantic call graph.

[core_sdk_catalog.json](core_sdk_catalog.json) records all source macro names,
native/ARM preprocessed names and object/preprocessor hashes. Real Clang23
AMD64-COFF and devkitA64 GCC15 AArch64-ELF compilation both passed static type
assertions for every expanded SDK host callback:
`void(uint8_t*, R5900Context*, PS2Runtime*)`.

There are 121 syscall entries and 546 SDK-stub expansions representing 545 unique
stub names. Upstream lists `sceSifLoadElfPart` twice; repeated declarations of the
same function are permitted, and the duplicate is retained in the report. The
first probe incorrectly defined a unique variable for each expansion and failed
on this duplicate. The corrected probe uses legal repeated extern declarations
and still type-checks each expansion; its original failure log is retained.
The 173 configured guest stubs each have a distinct registered wrapper at the
configured address. Guest wrappers use `PS2_GUEST_CC`; SDK callbacks use the host
convention. Declaration compatibility does not establish all SDK behaviors.

## Every generation diagnostic is classified without waiving execution

[core_diagnostic_scope.json](core_diagnostic_scope.json) preserves all 561 error
events and 1,196 indirect-control-flow warning events. Repeated reports remain
visible: the warnings represent 598 distinct owner/first-address pairs, each
reported twice; errors represent 553 such pairs in 43 owner functions.

All first diagnostic addresses lie in executable ELF `.text`. Of the error
addresses, 156 events have a directly registered table slot and 405 do not.
An absent direct slot does not prove an address cannot be reached by fallthrough
or local control flow. Section flags and table presence do not prove execution,
embedded data or dead code.

The error selectors were independently compared with PCSX2's R5900 dispatch
tables at commit `4223d03c58f8fd1787da7af28fe80c2140e1a22a`:
499 SPECIAL, 45 COP2-format, 15 COP0-CO and two COP0-format events all select an
`Unknown` entry. No recognized missing legal EE instruction family was found in
this comparison. In particular, adding generic MIPS64 DMULT/DDIV semantics for
SPECIAL selectors reserved by EE would be an incorrect response. The reference
file SHA-256 is
`80562704233e7d7f68d54878f4cf00cad2f63d1ba8362b76ea09634c5093ec1f`.
This classification uses the actual
[pinned PCSX2 opcode tables](https://github.com/PCSX2/pcsx2/blob/4223d03c58f8fd1787da7af28fe80c2140e1a22a/pcsx2/R5900OpcodeTables.cpp#L454).

Reserved encodings are consistent with over-broad function/data discovery, but
that remains an inference. They are not asserted to be padding, data or
unreachable. The generator's owning `CodeGenerator::emitUnhandledInstruction`
path explicitly emits `std::runtime_error`; those failure paths are retained.
An illegal input must not gain invented successful behavior merely to suppress
diagnostics. The warning records preserve each listed indirect site and promoted
fallback count; registration provides a dispatch path, not proof of actual
runtime targets or safe alias/resume behavior.

M02's public synthetic workloads install public test callbacks and do not invoke
these game owner bodies. Their target scheduler/CPU tests and full-core linking
must still pass. Later real-game boot and route traces must investigate any
reached reserved throw or missing indirect target under M04-12 and M06; unknown
game reachability is not converted into a successful test. This audit can support
closing the source-classification work only after the milestone's required
target workload evidence exists. It does not close any target-execution task.

## Reproduction and retained proof

The tool does not download dependencies. Place the public pinned PCSX2 source
above at `build/evidence/M02/coverage/coverage-pcsx2-opcodes.cpp`; its exact hash
is verified before the dispatch tables are used. With the recorded native tools,
locked SSE2NEON checkout and pinned project container available:

```powershell
python tools/audit_core_coverage.py --reference build/m02/reference --compile-probes
```

Observed result: `PASS metadata audit: 661 SIMD sites, 1757 diagnostics,
121 syscalls/545 SDK stubs; 173 configured guest wrappers`.
The audit can rerun without `--compile-probes` to check the retained private
objects/preprocessor output. Those artifacts and logs stay under
`build/evidence/M02/coverage`; private ELF/generated source stay in the isolated
reference checkout. `python -m py_compile tools/audit_core_coverage.py` also
passed. No Switch run, reviewer creation, commit or ledger closure was performed
by this audit subtask.
