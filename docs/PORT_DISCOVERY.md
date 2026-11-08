# Port discovery dossier

Baseline: `gothamor/gta-vcs-recomp` commit
`ecf150e53f87e4656dfe81eacd922839d952b282`, local supported PS2 executable SHA-1
`d1832508caf0990c2291c0f255c96a889cce9f88`. This dossier is M00 discovery, not a
claim that ARM64 code, GL rendering or audio has been built or run.

## Complete scoped inventories

All emitted records are metadata, not game code/assets. They are intended for
searchable automated work; do not replace them with a hand-selected function list.

| Inventory | Scope and interpretation |
|---|---|
| [summary.json](discovery/summary.json) | Exact counts, identity, scope and limits |
| [source_files.json](discovery/source_files.json) | Every tracked C/C++ source/header in VCS src and PS2Recomp runtime, IOP and recompiler directories; LF-normalized SHA-256, byte count and parse status |
| [source_functions.json](discovery/source_functions.json) | All 4,687 syntax-detected definitions, signature/declarator, namespace/class context and file/line span |
| [source_declarators.json](discovery/source_declarators.json) | 7,488 function declarator nodes, including definitions/pointer declarators; not unique semantic functions |
| [guest_functions.json](discovery/guest_functions.json) | All 11,216 rows from functions.csv and extra_functions.csv, grouped by 10,383 unique entry addresses, with aliases/end/size and source map |
| [runtime_call_macros.json](discovery/runtime_call_macros.json) | Named X-macro SDK/syscall entries missed by ordinary definition parsing, with macro/line |
| [recompiler_config.json](discovery/recompiler_config.json) | Parsed generated configuration and maintained overrides: stubs, skipped/replaced functions, instruction patches and other supplied metadata |
| [vu_uploads.json](discovery/vu_uploads.json) | All 23 supplied VU upload records: ELF source address, VU destination and instruction-pair count |
| [includes.json](discovery/includes.json) | Every include site in scoped source; 2,315 sites, not resolved target transitive dependencies |
| [build_declarations.json](discovery/build_declarations.json) | 304 CMake declaration-start locations across tracked upstream build files; full multiline semantics remain in owning CMake files |
| [portability_candidates.json](discovery/portability_candidates.json) | Lexical candidates with exact source locations/category/tokens; comments/dead paths can match, so these are audit work, not confirmed defects |
| [parser_gaps.json](discovery/parser_gaps.json) | 667 error/missing nodes, prominently macro-heavy ps2_call_list.h; explicitly not treated as successful semantic parsing |
| [disc_manifest.json](discovery/disc_manifest.json) | All 341 ISO9660 files and 28 directories, original paths/LBA/length, supported executable identity and source image size; no asset contents |

The generator is [tools/discover_port.py](../tools/discover_port.py); pinned parser
requirements are separate host tooling. Definition parsing is syntactic, without
preprocessing/build flags or symbol resolution. Anonymous-namespace names can be
ambiguous, overloads remain distinct records, macro-generated functions and
indirect targets are incomplete. No "every possible function is identified" claim
is made. The supplied guest map is the committed recompiler map, not newly
proved complete control-flow recovery. Preserve and resolve these boundaries in
M02/M04 rather than checking off nonexistent evidence.

## Actual boot chain to preserve

Owning source: `upstream/src/main.cpp`, especially the initialization near lines
835–944; lookup all definitions in the source index when that revision changes.

1. `main` installs crash handlers/registers settings and discovers a supported ISO.
2. `vcs::IsoImage::open`, `stageElf` and executable hash validation materialize the
   ELF cache. These are desktop/ISO assumptions to replace for the folder-only port.
3. Stack-owned `MappedDisc` implements `IPS2DiscImage` and is installed with
   `ps2_disc::setImage`. Its lifetime covers the whole runtime. MAIN.SCM is also
   read to populate the trainer's mission names.
4. `PS2Runtime::getIoPaths`/`setIoPaths` route early memory-card paths BEFORE
   initialization. `PS2Runtime runtime` installs `setDebugUiCallbacks` first.
5. `runtime.initialize` sets up runtime/platform, then `runtime.loadELF` loads
   guest memory. It changes IO paths; main reapplies card/disc roots afterward.
6. `vcs::rig::install`, `vcs::trainer::install`, `vcs::input::install`,
   `vcs::volume::install`, `vcs::gameinfo::install` attach real hooks. Settings/menu
   control commands, memory watch, self-test, watchdog and profiler follow.
7. `runtime.run` owns execution. Desktop exits through `std::_Exit`, bypassing
   normal destruction. A Switch adapter must explicitly stop/join the runtime,
   IOP/audio/GPU work and unwind; copying this exit path is unsafe.

Root `gtavcs_core_boot/frame/audio/shutdown` are NEW glue interfaces only. They
do not exist upstream. Do not implement them by calling a nested desktop main,
creating two platform contexts, or advancing guest execution from audio pulls.

## Ownership matrix and concrete function work

| Subsystem | Existing functions/files | Switch action and milestone |
|---|---|---|
| Process/paths | main, exeDirectory, dataDirectory, stageElf, MappedDisc; src/main.cpp | Split boot/run/shutdown and early paths, folder disc ownership; M03/M04 |
| SDL setup | platform::init/shutdown/ready/nativeWindow/windowHidden | New platform_switch_sdl2 implementation, one SDL/window/GL owner; M01/M04 |
| Window placement | findDisplay/placeWindow/currentPlacement/notePlacement/reportPlacement/setFullscreen | Keep valid target semantics without X11/Win32 monitor persistence; M04 |
| Vulkan window callbacks | vulkanInstanceExtensions/vulkanCreateSurface | Unused in Switch GL path, retain desktop interface behavior; M04/M05 |
| Controller acquisition | openGamepad/snapshotGamepad/pumpEvents, gamepadConnected/Button/Axis/Name/Rumble | SDL2 GameController replacement plus synchronized snapshot; M04/M07 |
| Keyboard/mouse | scanKeys/keyDown/keyPressed/mouseDown/mousePosition/setMouseCaptured/takeMouseMotion | Safe unsupported-device behavior and controller menu/camera alternatives; M07 |
| UI frame | beginFrame/endUiFrame, settings/trainer draw callbacks | Existing ImGui context + SDL2 event backend + RHI renderer; M05/M09 |
| Frame clock | setFrameRate/displayRefreshHz/delayPrecise/waitNextFrame/setFrameLead | SDL2 performance counters/libnx-compatible waits with guest timing intact; M04/M10 |
| Dimensions/focus | windowSize/windowFocused | Drawable pixels, lifecycle state and resize notifications; M01/M04/M06 |
| Audio device | audioAvailable/audioOpen/audioClose/audioCallback | SDL2 stream conversion + audren-owned SDL output, bounded worker/ring; M08 |
| Diagnostics | writePng, debug crash/watchdog/profiler/rig | Preserve useful captures/timestamps/control, gate desktop facilities; M04/M10 |
| Guest pad | vcs::input::install/registerSettings/onHostFrame, ps2_pad.cpp | Existing bindings/DS2 report preserved, physical Switch mapping; M07 |
| Game sound | vcs_sound.cpp, libsd.cpp, vcs_volume.cpp, ps2_audio_out.cpp | Preserve SPU2/native timing and real PCM; M08 |
| GPU interface | rhi::createDevice, Device virtuals, null/Vulkan/D3D11 backends | Implement new OpenGL backend instead of API renaming; M05 |
| Native scene | vcs_hle_world.cpp, vcs_rsl_world.cpp, GS/VIF/VU code | Resource upload/render submission/streaming and numeric equivalence; M02/M06 |
| Presentation | presenter.cpp, vcs_display.cpp, vcs_hud.cpp | Render scale vs output, aspect/culling, HUD/text/menus; M06 |
| Movies | vcs_movie.cpp, decoder dependencies | Target decoder, PSS video/audio synchronization and lifecycle; M08 |
| Settings/saves | settings.cpp, runtime mc/file I/O, upstream backups | gamefolder-relative persistence, safe replacement/backup/order; M03/M09 |

Every detected function has a file and source span in the inventory. This table
assigns ownership families, not blanket instructions to rewrite thousands of
portable game functions. Preserve guest logic and adapt the platform boundary.

## RHI contract requiring implementation

`upstream/src/rhi/rhi.h` defines resource descriptions and all `rhi::Device`
operations. Cover ALL of these, not only drawing a triangle:

- Resource identity: `backend`, `adapterName`.
- Textures: `createTexture`, `updateTexture`, `readTexture`, `copyTexture`,
  `resolveTexture`, `generateMips`, `createTextureLevels`, `freeThreadedCreation`.
- Buffers/pipelines: `createBuffer`, `updateBuffer`, `createSampler`, `createPipeline`.
- Pass/state: `beginPass`, `endPass`, `setViewport`, `setScissor`,
  `setBlendConstant`, `setPipeline`, `setVertexBuffer`, `setIndexBuffer`,
  `setTexture`, `setConstants`.
- Submission/debug: `draw`, `drawIndexed`, `pushMarker`, `popMarker`.
- Presentation: `backbufferSize`, `resize`, `present`, `setFrameInputTime`.
- UI: `uiInit`, `uiNewFrame`, `uiRender`, `uiShutdown`.

Audit RGBA8/R32F/D32F, samples/mip levels, row pitch, depth/clip conventions,
dual-source blending and framebuffer feedback. GL resource creation is normally
context-thread-bound: do not return true for `freeThreadedCreation` without a
correct queue/shared-context ownership design and validation.

`src/render/CMakeLists.txt` currently builds DXBC/SPIR-V from HLSL. The four source
families are present, gs_draw, gs_util and ui. Variant combinations include
draw/world/lit, sampling modes, dual-source outputs and no-depth-write forms.
Build an explicit GLSL variant manifest; compare actual output before optimizing.
The generated GLAD loader exposes GL 3.3 core with no optional extensions. If a
required extension is chosen, explicitly regenerate/probe it and update the plan.

## ARM64/runtime/recompiler audit

661 lexical x86/SIMD candidate lines exist in the scoped source. These include
guest register/memory/VU operations and possibly host-only code. Audit them by
semantic operation, not a blind `_mm_*` rename. Signed/unsigned shifts, lane order,
saturation, conversion edge cases, denormals and PS2 rounding affect correctness.

Upstream's top CMake demands Clang >=19 and x86 CPU defaults. Its `ps2_guest_cc.h`
uses preserve_none only for Clang x86 and already falls back to the platform ABI
elsewhere. This does NOT prove generated code compiles with devkitA64 GCC.
Establish compatibility in M02, preserve function-pointer ABI everywhere, and
keep host generator tools separate from the target. No JIT/executable-memory
assumption is acceptable without a source-backed need in a static recompile.

Other audit counts: 80 desktop-OS candidates, 263 thread/time candidates, 650
file-I/O candidates, 152 SDL candidates and 25 TODO/FIXME/unimplemented candidates.
These are locations to inspect, not counts of confirmed bugs. Main scheduler,
IOP/native services, callbacks and every guest override must preserve yield/resume
semantics. Upstream forbids assuming code following a wrapped guest call runs
normally after a scheduler yield; retain that rule in the C++ adapter.

## Complete disc-level asset classification

The **local image is 2,183,856,128 bytes**. Upstream DISC.md describes a different
full-disc image size. The executable hash matches; do not silently reuse upstream
LBAs/whole-disc identity for this image. `disc_manifest.json` is authoritative for
the inspected local directory/LBA metadata. Full content equivalence remains M03
work; executable identity alone does not verify every archive/stream.

| Observed group | Count | What must be supported |
|---|---:|---|
| AUDIO/* | 230 | 93 VB, 68 RAW, 68 SDT, M_LOOKUP.BIN; native sound/SPU2/streaming lookup, seek/loop/cancel |
| FRONTEND/* | 48 | PSP-named menu layouts including widescreen variants; these are PS2-disc resources |
| SPLASH/* | 29 | XTX loading/legal textures |
| MODULES/* | 9 | 8 IRX plus IOPRP300.IMG; CDSTREAM/CDVDSTM/LIBSD/SDRDRV/PADMAN/MCMAN/MCSERV/SIO2MAN; decide guest/native ownership per module |
| MOVIES/* | 4 | INTRO.PSS/INTROP.PSS/STINGS.PSS/STINGSP.PSS; do not infer PAL playback behavior just from names |
| PS2/* | 3 | GAME.DTZ, MOCAPPS2.DIR, MOCAPPS2.IMG |
| ICONS/* | 1 | PS2 save icon; not a Switch NRO icon replacement |
| root executable/config | 2 | SYSTEM.CNF and verified SLUS_215.90; private source input, not distributed assets |
| root language files | 6 | AMERICAN/ENGLISH/FRENCH/GERMAN/ITALIAN/SPANISH.GXT |
| root world/common | 6 | BEACH/MAINLA/MALL IMG+LVZ pairs |
| remaining root resources | 3 | GTA3PS2.IMG, EMPHUD.XTX, MAIN.SCM |

All individual names/sizes/LBAs are listed in the full manifest. No generic TEXT/
MODELS directory is assumed: preserve this actual extracted PS2 tree, with root
archives where the disc has them. Existing AGENTS layout examples are illustrative.

Upstream source/docs identify DTZ as compressed GTAG memory image and LVZ as
compressed WRLD images; most IMG content is streamed by the guest/native world
code rather than a generic GTA3 VER2 archive reader. DO NOT guess an archive
format from its extension. M03 must enumerate format-supported members/records,
map relocation/streaming dependencies and private content hashes. M06/M08 then
verify actual model/texture/animation/mission/audio use. This M00 pass inventories
every ISO-directory entry, not every object hidden inside compressed archives.

## Dependencies and patches

Target: official devkitA64/Switch CMake + libnx + packaged SDL2 + Mesa GL/EGL/
nouveau + GLAD. Root uses cross pkg-config/static archives. Do not link stock
Vulkan/NVN or host Ubuntu SDL/FFmpeg. The game later needs target-compatible
zlib, ImGui and movie decoding dependencies; exact closure/version/license lock
is M01/M02/M08 work, not resolved by the scaffold.

Upstream SDL3/ImGui/zlib, Vulkan headers/volk/VMA and shader host tools are declared
in platform/RHI/render CMake files. Preserve desktop dependencies on desktop.
Use [build_declarations.json](discovery/build_declarations.json) to locate every
declaration; it is not an evaluated CMake dependency graph. Resolve toolchain-
specific branches with configure/compile/link inspection in M01/M02.

Maintain upstream changes as ordered patches in `patches/upstream/` against the
pinned submodule, applied to a disposable checkout by future build tooling. Root
adapters/glue remain in this repository. Never commit dirty submodule edits as
if a root gitlink recorded them; bumping its SHA requires an accessible committed
upstream/fork revision. M02 must implement and verify patch application/rollback.

## Unknowns and REA escalation

I read the installed `reverse-engineer-anything` skill. It explicitly routes
ordinary source architecture analysis to normal repository tools. Source, the
supplied function maps and the ISO directory reader answer M00's scoped discovery
questions, so no REA provider/readiness session or binary launch was necessary.

Escalate when a concrete port blocker cannot be resolved from those sources:

1. State question/address/observed symptom, exact supported ELF identity and
   source hypotheses. Extract only the user's own required executable into ignored
   `build/analysis/`; never commit it or a disassembly/decompiled game body.
2. Read the REA skill's native/runtime reference for the chosen task; discover
   actual available tool schema and call the target-specific read-only analysis.
   Do not run setup/provider diagnostics unless a real connection failure occurs.
3. Use focused function/call/value analysis for an unresolved MIPS function,
   callback/yield boundary, archive parser, or platform-produced artifact. Preserve
   evidence identity and distinguish static inference from runtime observation.
4. For runtime claims, select a supported explicit host/emulator/hardware run;
   static decompilation is not an execution test. Keep test saves isolated.
5. Record names/addresses/behavior and citations to owning source/evidence, not
   copyrighted code dumps. Add a task to the active milestone and invalidate any
   prior candidate review if source/evidence scope changes.

Explicit unresolved work: semantic call graph/indirect targets, parser gaps,
all archive-internal members, raw-sector padding consumers, GCC compatibility,
host-to-target timing, GL feature/performance limits, target decoder closure,
hardware audio/input/docking and gameplay acceptance. Each has a milestone task.
No available source inventory can honestly pre-prove all future runtime behavior.
