# Milestone execution contract

This is the authoritative task ledger. Read with [PORT_DISCOVERY.md](PORT_DISCOVERY.md),
[PORTING_CHECKLIST.md](PORTING_CHECKLIST.md), [AUTOMATED_EXECUTION.md](AUTOMATED_EXECUTION.md)
and root [AGENTS.md](../AGENTS.md). M00 establishes the plan and inventories;
it does not validate a Switch executable. Every later milestone requires its
own implementation AND observed acceptance evidence.

## Mandatory workflow

1. Exactly one active milestone branch, cut from the latest reviewed `main`.
   Do not pre-create later implementation branches or start their tasks early.
2. Tasks start unchecked. Work in dependency order. A checkbox means its stated
   result was observed and a repository evidence record exists, not merely coded.
   If required hardware/tooling is missing, keep the task unchecked and report it.
3. Store each task's evidence in `docs/milestones/Mxx/evidence.json`: task ID ->
   nonempty list of `{ "path": "repository/file", "result": "observed result" }`.
   Include command, source/target commit, environment, expected/observed outcome,
   log location and limitations in the referenced report. URLs alone are insufficient.
4. Commit implementation and evidence. Run local checks and CI for that candidate.
   Only when EVERY task is complete and `python tools/milestone_gate.py Mxx`
   passes may an independent reviewer be created. No reviewer during unfinished
   implementation, no premature partial review and no self-review substitute.
5. Create a fresh reviewer agent, separate from implementation, read-only initially.
   Give exact candidate SHA, milestone scope, task ledger, evidence and commands.
   Reviewer independently checks all tasks, reproduces feasible tests, examines
   material limits, and returns `APPROVE` or `REQUEST_CHANGES` with file/line findings.
   Hardware claims require hardware evidence; an agent cannot invent a console run.
6. Any finding reopens its task; fix on the SAME branch. No merge with unresolved
   findings, including low-severity ones. Once every task is done again, request
   independent re-review of the new SHA. Do not start another milestone meanwhile.
7. Record `docs/milestones/Mxx/review.json` with milestone, exact candidate_commit,
   verdict, reviewer_id, independent=true, unresolved_findings=[], and substantive
   checks/limitations. The reviewer report is the only allowed post-review change.
   Any other change, even evidence/task edits, invalidates the approval.
8. Run `python tools/milestone_gate.py Mxx --require-review`, wait for all required
   CI checks, merge the PR with a merge commit, verify remote `main`, then cut/push
   the next milestone branch from that merged main. Never merge just to save work.
9. Repository visibility remains private. No ISO, executable, extracted assets,
   generated game C++, credentials or user saves in Git/CI artifacts. Public
   upstream source is a pinned submodule; port edits use maintained overlay patches.

GitHub controls are supplementary: see M00 evidence for actual account support.
Local/CI structural checks validate records, not the truth of behavioral claims.
The independent reviewer must examine the underlying evidence. No arbitrary task
waivers, "N/A", reduced scope or performance exceptions to force completion;
scope changes require the user's explicit decision and a revised plan.

## Dependency map and acceptance budgets

`M00 -> M01 -> M02 -> M03 -> M04 -> M05 -> M06 -> M07 -> M08 -> M09 -> M10 -> M11`.
Each milestone also reruns relevant proof from earlier milestones. A discovered
prerequisite belongs in the active milestone before review, with a new unique ID.

Beta means interactive gameplay with audio, controller-only menus, ImGui/dev menu,
safe saves, handheld/docked output, internal render scaling and a measured 25+ FPS
baseline at stock clocks. Output 720p/1080p does not imply rendering all scenes
internally at those sizes. Report the exact internal scale with every measurement;
native internal-resolution 25+ FPS is a separately measured goal, not assumed.
Never tune guest time or skip game work to inflate a frame counter.

## M00 — Private repository, exhaustive discovery and execution plan

Branch: `milestone/m00-foundation` <!-- M00 -->

Entry: initial scaffold exists, no target compilation/gameplay evidence.
Exit: a private, traceable repository and executable development governance;
all discoverable scoped metadata catalogued with explicit unknowns and next tasks.

- [x] M00-01 Create private GitHub repo, verify owner/visibility/default main, configure merge-commit-only milestone flow.
- [x] M00-02 Establish bootstrap main and isolated M00 branch; register upstream as a pinned submodule and preserve its clean tree.
- [x] M00-03 Add ignore/attribute/license policy and verify no game data, generated game code or credentials are tracked.
- [x] M00-04 Preserve the initial SDL2/GL scaffold and label its unbuilt, silent, stub-only status accurately.
- [x] M00-05 Inspect startup/platform/RHI/runtime/recompiler ownership and document exact entry functions and required replacements.
- [x] M00-06 Generate complete metadata inventories within declared source scope: files, parsed definitions/declarators, includes, build declarations and parser gaps.
- [x] M00-07 Inventory all supplied guest function-map rows, deduplicated addresses, SDK macro calls, recompiler configuration and VU uploads without game bytes.
- [x] M00-08 Verify local disc identity and catalogue every ISO9660 file/directory, size and LBA; classify formats and unresolved archive members.
- [x] M00-09 Map every subsystem, dependency and portability candidate family to owning files, milestone tasks, validation and unknowns.
- [x] M00-10 Evaluate REA need; document source-first decision and exact escalation workflow for unresolved binary/runtime questions.
- [x] M00-11 Write exhaustive ordered milestones with tasks, entry/exit criteria, commands, evidence, recovery and independent-review/merge gates.
- [x] M00-12 Implement and test fail-closed all-task/evidence review eligibility and exact-candidate approval checks; test incomplete and stale approval rejection.
- [x] M00-13 Add source-only foundation CI, milestone PR gate and packaging exclusions; exercise local checks and remote foundation CI.
- [x] M00-14 Attempt GitHub main protection; record actual result and any plan limitation without claiming unavailable enforcement.
- [x] M00-15 Record M00 verification, environment blockers and reproducibility limits; verify the final candidate is clean and eligible before creating its independent reviewer.

Commands: discovery and foundation commands in AUTOMATED_EXECUTION; then review
eligibility gate. Evidence: inventories, discovery narrative, M00 verification,
CI run and repository settings report. No target build required for this milestone.

## M01 — Reproducible toolchain and platform smoke NRO

Branch: `milestone/m01-platform-smoke` <!-- M01 -->

Entry: reviewed M00 main. Exit: clean-machine reproducible platform NRO and actual
Switch clear-screen/input/audio-device/docking/shutdown proof; no game claim.

- [ ] M01-01 Restore usable WSL Ubuntu/Docker Linux engine and record host versions/resources without changing unrelated projects.
- [ ] M01-02 Pin devkitA64 image digest and package versions, official Switch CMake, libnx, SDL2, Mesa and cross pkg-config; save lock manifest.
- [ ] M01-03 Create noninteractive Dockerfile/build command and cache strategy; fail on unavailable dependencies or accidental host libraries.
- [ ] M01-04 Build scaffold from a fresh checkout/submodule init with all generated GLAD dependencies present; inspect compiler/include/library paths.
- [ ] M01-05 Inspect AArch64 ELF, PIE/relocations/map and NACP/NRO; demonstrate incremental and clean rebuild plus failed-build artifact rejection.
- [ ] M01-06 Stage only NRO under switch/gtavcs_nx, verify missing asset-root failure and successful checked chdir; provide visible persistent diagnostics.
- [ ] M01-07 Boot on an actual Switch with full application memory; capture firmware/libnx/driver versions and GL 3.3 context/clear screen.
- [ ] M01-08 Verify paired Joy-Cons and Pro controller initial open, hotplug, disconnect-neutral, reconnect and orderly smoke exit.
- [ ] M01-09 Verify SDL2 opens audren-backed audio, streams bounded silence and closes it cleanly; record buffer/queue behavior.
- [ ] M01-10 Dock/undock repeatedly; verify drawable pixels, viewport, 720p/1080p output and correct ownership of one context/surface.
- [ ] M01-11 Exercise HOME/focus/suspend/resume and failure cleanup; eliminate invalid stream/context use and runaway loops.
- [ ] M01-12 Add source-only cross-build CI/artifact retention with game data excluded; retain reproducible hardware logs and close every M01 failure.

Proof: cross-build logs, package lock, ELF/NRO inspection, console logs/video,
controller/device matrix, docking/suspend observations. Emulator-only results are
supplementary and cannot close hardware tasks.

## M02 — Host generator and ARM64 EE/VU foundation

Branch: `milestone/m02-arm64-core` <!-- M02 -->

Exit: reproducible private generated EE/VU code compiles and executes validated
CPU/scheduler workloads on ARM64, with guest semantics preserved.

- [ ] M02-01 Capture a desktop reference baseline from the verified disc, isolated saves and recorded source/flags; preserve upstream's reference path.
- [ ] M02-02 Build recompiler/analyzer as host tools separately from target code; prove generator does not try to execute AArch64 utilities on the host.
- [ ] M02-03 Run upstream recompile.py on the exact supported ELF; record stamp, generated registration count and all missing/unresolved functions.
- [ ] M02-04 Audit generated constructs and compiler compatibility; decide documented GCC-compatible adaptation or compatible Clang cross setup without masking errors.
- [ ] M02-05 Classify every x86 SIMD candidate by operation/owner; port guest 128-bit memory/register representation and unaligned accesses.
- [ ] M02-06 Port packed EE arithmetic, saturation, shifts, shuffles, comparisons and bit casts with edge-case/reference differential evidence.
- [ ] M02-07 Port VU interpreter/compiled operations, rounding, exceptional floats and conversion semantics; retain FP contraction prohibition.
- [ ] M02-08 Verify PS2_GUEST_CC declarations, function tables, hook wrappers, stack alignment, tail/yield/resume behavior and guest return-address integrity.
- [ ] M02-09 Verify ARM char signedness, endian/layout, aliasing, atomics, TLS, allocation alignment and exceptions/unwind assumptions.
- [ ] M02-10 Adapt runtime build flags and disable unsupported x86 JIT/desktop backends explicitly; never silently use incorrect fallback math.
- [ ] M02-11 Resolve parser gaps affecting compiled core with compiler/preprocessed evidence; catalogue macro-generated SDK call registrations.
- [ ] M02-12 Run relevant existing EE/VU/memory/kernel tests plus differential suites on host and ARM64; retain failures and final passing outputs.
- [ ] M02-13 Establish memory/stack/worker budgets under full application mode and compile a null-renderer core harness with no desktop dependencies.
- [ ] M02-14 Demonstrate deterministic scheduler/guest execution progress and clean stop on target; keep generated code/ELF entirely out of Git/CI.

Proof: per-operation comparison, toolchain decision, generated identity/register
metadata, native test logs, target trace. Compilation alone does not close execution.

## M03 — Extracted assets, sector VFS and persistent paths

Branch: `milestone/m03-folder-vfs` <!-- M03 -->

Exit: gamefolder-only runtime reads preserve required ISO semantics; assets,
mods, settings and saves resolve to the intended roots without a runtime ISO.

- [ ] M03-01 Build an explicit user-disc extraction tool that verifies identity and preserves original path names and extent metadata.
- [ ] M03-02 Enumerate internal archive/directory formats (IMG/DIR, DTZ/GTAG, LVZ/WRLD, XTX, PSP frontend, SDT/VB/RAW, SCM, GXT, PSS); classify what remains guest-handled.
- [ ] M03-03 Produce private content-hash manifest and metadata-only coverage map; document unsupported members and resolve each boot-critical format gap.
- [ ] M03-04 Implement IPS2DiscImage folder backend for findFile, readBytes, totalSectors and listDirectory with deterministic directory metadata.
- [ ] M03-05 Normalize cdrom0 prefixes, slash/case and ISO version suffixes; reject path escapes, ambiguous names and invalid sizes/offsets.
- [ ] M03-06 Match original LBA mapping and reads crossing files/sectors; identify raw reads into padding/gaps and preserve actually consumed bytes privately.
- [ ] M03-07 Implement thread-safe random access and async CDVD/IOP streaming completion/cancellation; avoid shared seek-pointer races.
- [ ] M03-08 Differentially compare boot, world streaming, radio, cutscene and unaligned boundary reads against source ISO.
- [ ] M03-09 Remove runtime ISO discovery/staging dependency; resolve required ELF/data staging under gamefolder without shipping it in the NRO.
- [ ] M03-10 Route configuration/cache/test saves separately under gamefolder/userdata; preserve memory-card directory/file metadata and backups.
- [ ] M03-11 Define supported mod overlay precedence/root validation and verify missing/corrupt assets produce actionable logs with safe failure.
- [ ] M03-12 Verify SD case/path behavior, bounded file-handle/cache use and clean shutdown during pending reads on hardware.

Proof: extractor validation, metadata coverage, sampled and recorded-trace byte
comparisons, no-ISO runtime test, corrupt/missing asset and cancellation tests.

## M04 — SDL2 platform adapter and real runtime boot

Branch: `milestone/m04-runtime-boot` <!-- M04 -->

Exit: genuine recompiled runtime boots and advances through its init/front-end
states with null rendering and diagnostic evidence; stub is no longer linked.

- [ ] M04-01 Split upstream main ownership into explicit core boot/run/stop behind port C ABI; replace core_stub and avoid duplicate SDL/window startup.
- [ ] M04-02 Implement every platform.h function required by runtime in platform_switch_sdl2.cpp, including safe unsupported desktop operations.
- [ ] M04-03 Translate SDL3 init/events/timing/controller/audio interfaces correctly to SDL2; no mixed SDL2/SDL3 symbols.
- [ ] M04-04 Preserve PS2Runtime initialize/loadELF ordering, early IoPaths, mapped disc lifetime and generated function registration.
- [ ] M04-05 Install input/volume/gameinfo/trainer hooks and UI callbacks in correct lifecycle order without desktop hotkey requirements.
- [ ] M04-06 Adapt host waits, worker scheduling and SDL event owner; maintain guest yield/resume and avoid main/game-thread deadlocks.
- [ ] M04-07 Audit every desktop_os/thread_time candidate on this boot path; gate unsupported crash/profiler/control facilities explicitly.
- [ ] M04-08 Replace upstream std::_Exit with joined orderly cleanup; cancel guest/IOP/audio work before destroying owned resources.
- [ ] M04-09 Demonstrate asset reads, real game constructors, IOP/native sound initialization and front-end state progress using source-backed counters.
- [ ] M04-10 Verify stop, startup failures, HOME/focus changes and repeated launches without leaked workers or corrupted saves.
- [ ] M04-11 Expose bounded diagnostic/control commands for later scripted scenes, with user-save isolation and stable machine-readable errors.
- [ ] M04-12 Compare null-path behavior to desktop reference and resolve every runtime exception/unlisted-resume/unsupported syscall blocking boot.

Proof: candidate-linked map excludes stub, guest state logs, deterministic init
milestones, state/trace comparison and lifecycle tests. A blue screen is insufficient.

## M05 — OpenGL RHI and shader conformance

Branch: `milestone/m05-opengl-rhi` <!-- M05 -->

Exit: all required rhi::Device methods/shader variants operate correctly on Switch,
with diagnostic offscreen comparisons and resource lifetime proof.

- [ ] M05-01 Add OpenGL backend selection to RHI without modifying desktop backend behavior; keep null backend available.
- [ ] M05-02 Implement texture formats/creation/subresource update, row pitch, mip chains and sampler/filter/wrap/anisotropy behavior.
- [ ] M05-03 Implement vertex/index/constant buffers, update/discard semantics and resource bounds/alignment checks.
- [ ] M05-04 Implement immutable pipeline translation, vertex layouts, topology, blend equations/write masks and depth state.
- [ ] M05-05 Implement render passes, viewport/scissor, clears, resolves, copies, readback and framebuffer completeness checks.
- [ ] M05-06 Implement shader/constants/texture bindings and every required present, GS draw/world/lit, utility and UI GLSL variant.
- [ ] M05-07 Verify dual-source blending support/limits and implement a correct bounded fallback only where required; no visual guess.
- [ ] M05-08 Resolve clip-space depth/Y origin, handedness, texture swizzle/format and sRGB conventions against known images.
- [ ] M05-09 Implement hazards/fences/staging and deferred destruction; freeThreadedCreation must reflect actual GL owner-thread constraints.
- [ ] M05-10 Wire SDL-owned backbuffer/present/resize; no second swapchain/context or unsynchronized GL calls from workers.
- [ ] M05-11 Implement UI renderer methods through existing ImGui/RHI ownership; verify reset-render-state/scissor and texture lifetime.
- [ ] M05-12 Validate every Device virtual method and GLSL variant in deterministic resource/draw/readback scenes on hardware.
- [ ] M05-13 Compare pixel/depth results with the desktop reference, record tolerances and resolve all unexplained differences.
- [ ] M05-14 Run GL diagnostics and repeated resize/resource churn; enforce measured GPU/CPU memory budgets and no unbounded growth.

Proof: RHI method matrix, shader manifest, image/depth differences and target GL
diagnostics. Claim no ingame milestone until world integration is tested.

## M06 — World rendering and playable game route

Branch: `milestone/m06-ingame` <!-- M06 -->

Exit: visible, controllable original gameplay across representative outdoor,
interior and streaming scenes, with faithful HUD and menu/camera behavior.

- [ ] M06-01 Connect presenter/GS/VIF/VU/native-world submission to OpenGL and ensure the authentic/reference path remains selectable.
- [ ] M06-02 Validate GAME.DTZ, common models/textures and each level's LVZ/IMG streaming/relocation in real scenes.
- [ ] M06-03 Verify opaque/alpha-tested/transparent geometry, lighting, fog, texture palettes/mips and draw ordering.
- [ ] M06-04 Verify skinned characters/animations, vehicles, water, particles, shadows, cutouts and transient effects.
- [ ] M06-05 Verify cameras/culling/widescreen/HUD/radar/subtitles and front-end PSP wide-layout scaling without stretch/crop.
- [ ] M06-06 Implement internal-resolution scale independently from output dimensions; recreate targets/camera state on docking.
- [ ] M06-07 Establish scripted reproducible on-foot/driving route with state/position evidence and input replay timestamps.
- [ ] M06-08 Traverse BEACH, MAINLA and MALL/interior boundaries and verify no missing terrain, corrupt textures or stalls.
- [ ] M06-09 Compare representative screenshots and key guest state to desktop reference; resolve every unexplained material mismatch.
- [ ] M06-10 Exercise mission start/progress, pause/map/shop/interior entry and return to gameplay; distinguish menu boot from real game simulation.
- [ ] M06-11 Capture provisional timing/memory breakdown at fixed stock clocks for later optimization; no 25 FPS claim yet.
- [ ] M06-12 Verify exit/load/restart with active streaming and eliminate world resource leaks or hangs.

## M07 — Complete controllers and controller-only interaction

Branch: `milestone/m07-controllers` <!-- M07 -->

- [ ] M07-01 Confirm SDL physical-position mapping on paired Joy-Cons, handheld controls and Pro Controller with logged button identity.
- [ ] M07-02 Map every supported PS2 action, both sticks, ZL/ZR, Plus/Minus, d-pad and stick clicks through existing binding/report layers.
- [ ] M07-03 Verify active-low DualShock report, pressure/trigger expectations, signed extremes, dead zones and neutral disconnected state.
- [ ] M07-04 Preserve on-foot aim/camera, vehicle controls, lock-on, weapon cycling, swimming and contextual mission inputs.
- [ ] M07-05 Make all game front-end/pause/map/text/save menus usable without keyboard/mouse; implement required text-entry flow.
- [ ] M07-06 Verify rumble amplitude/duration/stop and reconnection without stale handles or blocking frame loops.
- [ ] M07-07 Test disconnect/reconnect and controller reassignment during gameplay, menus, HOME and resume.
- [ ] M07-08 Preserve configurable bindings/presets and safe persistence; prevent overlay/game simultaneous input.
- [ ] M07-09 Replay complete controller-only gameplay/menu route on each supported controller configuration and document unsupported layouts explicitly.
- [ ] M07-10 Remove smoke Plus-to-exit behavior and verify ordinary Start/Select semantics and safe deliberate application exit.

Exit proof: hardware controller matrix and action-by-action route, not only device
enumeration. Single sideways Joy-Con is not implicitly supported; decide via scope.

## M08 — Real sound, music, movies and synchronized streaming

Branch: `milestone/m08-audio-movies` <!-- M08 -->

- [ ] M08-01 Connect native libsd/SPU2 output to bounded PCM producer ring and SDL2 AudioStream/device consumer; remove silence stub.
- [ ] M08-02 Preserve sample rate, interpolation, stereo channel order, clipping/saturation and game volume settings against desktop reference.
- [ ] M08-03 Keep consumer timing independent of rendering; measure queue depth, latency, underruns and recovery from deliberate frame stalls.
- [ ] M08-04 Verify effects/voice/mission banks, ambient sounds, positional balance and simultaneous channels across representative scenes.
- [ ] M08-05 Verify radio/music/mission streams, seeking/looping and world transitions using AUDIO lookup/SDT/VB/RAW metadata correctly.
- [ ] M08-06 Add pinned target movie decoder dependencies or equivalent validated implementation; host FFmpeg libraries are forbidden.
- [ ] M08-07 Decode/render INTRO/STINGS PSS and gameplay cutscenes with audio/video sync, correct colors/aspect and bounded memory.
- [ ] M08-08 Validate skip/pause/seek/stop and cancellation with pending I/O/audio; no stale buffers replay after transitions.
- [ ] M08-09 Verify HOME/suspend/resume/device shutdown and error recovery without double audren ownership or callback I/O.
- [ ] M08-10 Record audible hardware playback plus PCM comparison and long streaming route; silence/nonzero PCM alone cannot close this milestone.
- [ ] M08-11 Verify no audio drift or cutscene desynchronization across variable frame rates and both output modes.

## M09 — ImGui/dev menu, settings and safe saves

Branch: `milestone/m09-ui-saves` <!-- M09 -->

- [ ] M09-01 Integrate upstream's ImGui core with SDL2 input backend and existing RHI UI renderer, one context and correct frame order.
- [ ] M09-02 Add controller overlay chord, gamepad navigation, focus/input capture and readable handheld/docked layout.
- [ ] M09-03 Expose renderer scale, timing, audio, controller settings and diagnostics without implementation clutter in ordinary game menus.
- [ ] M09-04 Persist settings beneath gamefolder and verify defaults/reset/relaunch; recover from malformed config safely.
- [ ] M09-05 Verify memory-card file layout, directory order, metadata and capacity behavior match game expectations.
- [ ] M09-06 Implement/verify save backups and atomic replacement/recovery; tests operate on disposable copies, never live user saves.
- [ ] M09-07 Complete create/save/load/overwrite/delete/full-card/error flows through controller menus and verify mission/player state restored.
- [ ] M09-08 Verify settings/menu close/reopen during gameplay, cutscene transitions and suspend without leaking input or GL state.
- [ ] M09-09 Verify trainer/dev commands use guest scheduler-safe hooks and cannot invalidate normal release save tests silently.
- [ ] M09-10 Document player controls/SD layout/troubleshooting and reproduce a full controller-only start-to-save-to-reload session.

## M10 — Stability, lifecycle and measured performance

Branch: `milestone/m10-stability-performance` <!-- M10 -->

- [ ] M10-01 Pin stock-clock hardware/firmware/build/settings and define a repeatable busy gameplay route with warm/cold run procedures.
- [ ] M10-02 Instrument CPU frame/game/worker/streaming time, GPU time, stalls, memory, cache churn and audio underruns with bounded overhead.
- [ ] M10-03 Identify bottlenecks from measurements and optimize one owning layer at a time with before/after evidence and visual/numeric regression checks.
- [ ] M10-04 Establish 25+ FPS baseline on the busy route in both modes; report median/p95 frame time and sustained slow intervals at explicit internal/output scales.
- [ ] M10-05 Attempt native 720p/1080p internal resolutions separately; report observed limits without relabelling scaled output as native rendering.
- [ ] M10-06 Verify frame pacing and simulation/mission timer/audio cadence at 25/30/variable FPS; no guest-time manipulation to meet FPS target.
- [ ] M10-07 Run 30-minute minimum continuous gameplay soak in both handheld and docked modes with streaming, combat, missions and interior transitions.
- [ ] M10-08 Repeat docking/undocking, HOME, suspend/resume, controller loss and orderly exit under heavy streaming/audio/GPU work.
- [ ] M10-09 Inject missing/corrupt assets, allocation/I/O failures and save interruptions safely; prove diagnostics and bounded recovery.
- [ ] M10-10 Fix all crash/hang/leak/save/audio/visual findings and rerun the affected reference matrices; retain unresolved issues as unchecked tasks.
- [ ] M10-11 Validate memory headroom and thermal/clock observations over the soak; no undocumented overclock requirement.
- [ ] M10-12 Produce a reproducible performance/stability report and full beta acceptance matrix tied to exact candidate SHA.

25+ baseline is operationalized as >=25 measured presented gameplay frames/s over
the standard route with p95 frame time <=40 ms after stated warmup. Loading screens
are reported separately, never silently removed. Any scope/budget change needs the
user's decision; reviewers cannot waive a missed target.

## M11 — Beta packaging, clean deployment and final acceptance

Branch: `milestone/m11-beta-release` <!-- M11 -->

- [ ] M11-01 Rebuild from clean pinned source/toolchain on a second clean environment; verify expected reproducibility or explain deterministic build differences.
- [ ] M11-02 Generate final source/version/build-info, NRO metadata, checksums and license notices without including game/generated assets.
- [ ] M11-03 Package exact switch/gtavcs_nx/gtavcs.nro layout plus separate user-disc preparation instructions/tooling.
- [ ] M11-04 Audit Git history, submodule/patch completeness and release artifacts for game bytes, secrets and accidental test saves.
- [ ] M11-05 Deploy onto a clean test SD layout and reproduce first boot, controller-only game route, audio, overlay, save/reload, dock/resume and exit.
- [ ] M11-06 Repeat final visual/audio/performance/soak acceptance on exact release candidate with 720p handheld/1080p docked output and recorded internal scale.
- [ ] M11-07 Validate mod-folder behavior, failed extraction/wrong-disc diagnostics, storage limits and upgrade preservation of existing saves/settings.
- [ ] M11-08 Write honest beta release notes, known supported configurations, measured limits, troubleshooting and recovery instructions.

After all eight tasks: independent review, resolve/re-review findings, merge and
tag accepted source, then attach only reviewed allowed artifacts to a private
release. Verify release visibility/checksums/download layout and record those
post-review publication actions separately. Review/merge/publication are workflow
gates, not pre-review tasks; this avoids circular review eligibility. M11 is the
last planned milestone; after it, do not invent a new branch/milestone without a
new user objective.
