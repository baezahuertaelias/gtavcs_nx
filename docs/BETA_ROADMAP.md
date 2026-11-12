# Roadmap to beta and beyond

How to take this project from today's state to a Switch beta, then to the
post-beta features. The task-by-task ledger stays in [MILESTONES.md](MILESTONES.md);
the workflow rules (branches, evidence, review, merge) stay in
[AUTOMATED_EXECUTION.md](AUTOMATED_EXECUTION.md). This page gives the order, the
reasoning and what to check at each step.

## Scope decision (user, 2026-11-11)

The user set four goals, in this order, replacing the post-beta order below:

1. **Graphics through a new game** = the beta (M03, M04, M05, M06). On an empty
   card a new game is S1 to S4; there is no title menu.
2. **Sound**: real game sound and movies (M07).
3. **Developer menu on L3+R3, then widescreen** (M08, then M09, two branches).
4. **Performance**: 25+ FPS baseline (M10).

All other milestones come after performance: custom textures (M11), custom audio
(M12), full controllers (M13), whole world and saves (M14), final release (M15).
Nothing was deleted or waived.

GL thread ownership (M05-09) is decided by measurement. The user asked for the
best option: prototype both record-on-worker/replay-on-main and a worker-owned
GL context, and adopt the one that is correct and faster on hardware. If the
worker-owned context wins, update AGENTS.md runtime ownership in that milestone.

## Scope decision (user, 2026-11-09)

Superseded for post-beta order by the 2026-11-11 decision above; the beta
definition still stands.

The previous plan defined beta as full gameplay with audio, saves, a dev menu
and 25+ FPS. The user replaced that definition:

1. **Beta** = the game opens on Switch, goes ingame and loads the first scene.
2. **After beta, in this order:** developer menu on L3+R3, widescreen, custom
   textures and custom audio.

Everything else from the old beta (real sound, movies, full controller matrix,
saves, whole-map coverage, 25+ FPS, soak) is still planned, but after those
features. Nothing was deleted or waived; see "Where old tasks went".

## What "first scene" means

Upstream's reference script (`upstream/tools/scenes.py`) records the boot path
from an empty memory card, measured on a desktop i7-7700K:

| Stage | Desktop time | What it is |
|---|---|---|
| S1 | ~0.5–3 s | Legal and loading screens |
| S2 | ~3–38 s | Opening flyover: the first 3D world output |
| S3 | ~44 s onward | Intro cutscene (Cross skips it) |
| S4 | ~100 s | Gameplay at Fort Baxter next to Martinez's bike |

**The first scene is S4.** The game has no title menu on an empty card
(`upstream/docs/game-internals.md`, "Frames outside gameplay"), so there is no
main menu or New Game step to reach. The `scenes.py` header mentions a title and
Start, but its own timeline presses nothing before the cutscene, so the
game-internals description is followed. Expect the Switch to take far longer
than the desktop times. Confirm the sequence on the Switch instead of assuming it.

## Beta acceptance

All of this must be observed on a real Switch, in application mode, at stock
clocks, with evidence (`M06-18`):

- Started from the staged NRO and the extracted `gamefolder` with no ISO on the SD.
- The real recompiled game runs through S1, S2 and S3 to S4 from an empty test card.
- Cross may skip S3. Intro movies may be skipped. Both are stated in the release notes.
- At S4 the 3D world and HUD are visible and the player moves with a real controller.
- It runs 5 continuous minutes in S4 without a crash or hang, then exits cleanly.
- Tested in handheld and docked, at a fixed recorded internal scale, 4:3 picture.
- Median/p95 frame time, memory and CPU/GPU timing are recorded. **No FPS target
  applies to beta.** A slow result is reported, not relabelled.

Beta does not claim audible sound, movies, saves, a dev menu, widescreen, whole-map
coverage or mission playability.

## Current state (2026-11-11)

| Milestone | State |
|---|---|
| M00 discovery, M01 platform smoke, M02 ARM64 core | Reviewed and merged; M02's CPU suites passed 337/337 on a real Switch |
| M03 folder disc | All tasks done on `milestone/m03-folder-vfs`, including the Switch session of 2026-11-12; live BEACH/MALL loads moved to M14-02 and Switch boot from the folder to M04-13 by user decision |
| Root build | Still the M01 platform stub: blue screen and silence. No game code runs in the NRO |

## Execution order

Milestone numbers follow this order since the 2026-11-11 renumbering. Older
records keep the old IDs; MILESTONES.md has the old-to-new table.

```text
BETA TRACK   M02 -> M03 -> M04 -> M05 -> M06 = BETA          goal 1
POST-BETA    M07 game sound and movies                       goal 2
             -> M08 dev menu -> M09 widescreen               goal 3
             -> M10 stability and 25+ FPS                    goal 4
AFTER PERF   M11 custom textures -> M12 custom audio
             -> M13 full controllers -> M14 whole world and saves
             -> M15 final release
```

M07 still precedes M12 because custom audio cannot be heard or checked until the
game's own sound reaches the speakers.

**Known dependency gap.** M10 now runs before M13 and M14. Some M10 tasks touch
their ground: M10-01 needs a repeatable busy route (M14-04 is the scripted one),
M10-07 includes missions and interiors, M10-08 controller loss, M10-09 save
interruptions. M10 must build what it needs for those tasks itself, as new M10
task IDs, or ask the user before reducing them. M15-06 repeats the soak and
performance acceptance on the final candidate.

## How to follow it

Each step is one milestone branch, cut from reviewed `main`. Finish every task,
commit, pass `python tools/milestone_gate.py Mxx`, get the independent review, and
merge before starting the next one.

### Step 1 — Finish M02: the ARM64 core runs

- **Do:** close the open items in [M02/TASKS.md](milestones/M02/TASKS.md):
  generated-code audit and compiler decision, packed/FPU/VU numerics, guest calling
  and scheduler boundaries, allocator bounds, the 561 unhandled-instruction events
  and the indirect-fallback owners.
- **Hardware:** one consolidated Switch session running the public test suite and
  the private core harness.
- **Done when:** real generated EE/VU code executes deterministic workloads on the
  Switch, matches the native reference and stops cleanly.

### Step 2 — M03: game data without an ISO

- **Do:** extraction tool that verifies disc identity, then a manifest-backed
  `IPS2DiscImage` that serves the original LBAs from `gamefolder/`.
- **Watch:** the local image differs in size from upstream's `DISC.md`. Trust
  `docs/discovery/disc_manifest.json`, not upstream LBAs.
- **Done when:** recorded boot and streaming reads match the source ISO byte for
  byte, and no runtime code looks for an ISO.

### Step 3 — M04: real boot with no picture

- **Do:** split upstream `main` into boot/run/stop behind `src/core.h`, replace
  `core_stub.c`, write `platform_switch_sdl2.cpp`, replace `std::_Exit` with an
  orderly stop.
- **Done when:** with the null renderer the real game reaches S1/S2 states on the
  Switch, proven by guest counters compared with the desktop reference. A blue
  screen does not count.

### Step 4 — M05: OpenGL backend

- **Do:** implement every `rhi::Device` method and GLSL equivalents of the four HLSL
  families.
- **Decide early:** upstream records and executes GPU work on a render worker
  (`ps2_render_worker.cpp`), while a GL context belongs to one thread. Prototype
  both record-on-worker/replay-on-main and a worker-owned context, measure on
  hardware, and adopt the correct, faster one (user decision 2026-11-11,
  `M05-09`, `M05-10`).
- **First picture early:** the runtime's default GS is the software one
  (`PS2X_GS=cpu`), and the presenter shows CPU-made pictures. Texture upload,
  the present shader and swap are enough to see S1/S2 before the rest of the
  backend. Then complete `gs_hw.cpp` and the native world path through the RHI.
  `gs_gpu_parallel.cpp` (Vulkan paraLLEl-GS) stays out of the Switch build.
- **Watch:** `gs_draw.hlsl` uses dual-source blending (GL 3.3 core, check the
  nouveau limit); D3D 0..1 depth against GL -1..1 needs `ARB_clip_control` (not in
  the current GLAD) or a shader remap. Use `rhi_d3d11.cpp` as the closer model.
- **Done when:** every method and shader variant passes offscreen comparison with
  the desktop backend on real hardware.

### Step 5 — M06: first scene = BETA

Work through the stages, and fix each before moving on:

1. **S1:** loading screens display.
2. **S2:** flyover renders. This is the first real game picture, so run the deferred
   docking and Pro Controller checks here (`M06-13`, `M06-14`).
3. **S3:** cutscene plays or skips. GTA cutscenes may be paced by audio-stream
   progress, so check this with silent output (`M06-17`).
4. **S4:** Fort Baxter loads, HUD shows, player moves (`M06-16`).
5. **Acceptance run** (`M06-18`), then the beta package (`M06-19`).

Build without FFmpeg at first. `upstream/src/game/vcs_movie.cpp` already skips
movies when its headers are absent. Upstream's widescreen notes treat the intro
cutscene as 3D frames, not a PSS movie, so S3 should not need the decoder. Confirm
that on hardware.

Compare each stage with the same stage on the desktop reference build. When
something diverges, compare traces before changing game logic.

### Step 6 — M07: real sound and movies (goal 2)

Connect `ps2_audio_out.cpp`/`libsd.cpp` output to a bounded producer ring and an
SDL2 audio device consumer. SPU2 output is already 48 kHz. SDL keeps audren. Count
queue depth, latency and underruns; mute and flush across HOME and suspend. Movies
(`.PSS`, MPEG-2) need a Switch-built decoder pinned in the toolchain lock; check
its license obligations before release. Close only with audible hardware playback.

### Step 7 — M08: developer menu on L3+R3 (goal 3)

Upstream already has what the menu needs: a settings registry and menu
(`src/ui/settings_menu.cpp`), a trainer (`src/ui/trainer_menu.cpp`) and a
performance overlay. The work is the Switch input backend and the chord.

The hard part is leakage. L3 and R3 are real PS2 buttons, and the game's free aim
uses R1 then L3. Upstream's Back+Start chord already lets Start reach the game for
one frame (`upstream/docs/SETTINGS_MENU_WIDESCREEN.md`). So hold back stick clicks
from the game for a short window until it is clear whether a chord is happening,
and measure that this does not break free aim.

### Step 8 — M09: widescreen (goal 3)

Upstream's Hor+ widescreen is complete on desktop, so this is port-and-verify:
camera wrapper, entity culling, HUD squeeze and anchoring, 4:3 for 2D-only frames.
Leave the game's own PS2 widescreen option off, as upstream does. Check cost: the
16:9 render target is wider than 4:3.

### Step 9 — M10: stability and 25+ FPS (goal 4)

Measure first: game thread, render worker, GS stage, GPU, memory and underruns on
a repeatable busy route at stock clocks. Then change one owning layer at a time
with before/after evidence, for example thread placement on the three application
cores, wider native-renderer coverage, GCC PGO/LTO, bit-exact NEON fast paths for
hot operations, fewer GL state changes and a warmed Mesa shader cache. deko3d
behind the RHI only if measurements show GL driver cost dominates. No guest-time
changes and no overclock. See the dependency gap under "Execution order".

### Step 10 — After performance

M11 custom textures, M12 custom audio, M13 full controllers, M14 whole world and
saves, then M15 final release.

M11: no replacement system exists upstream, so this is new design. Textures come
from the GS backend's cache as PSMT4/T8 plus CLUT. Choose an identity that stays
stable across streaming, palette changes and render targets, then dump, load
through the VFS from `gamefolder/mods/textures/`, fall back to the original and
control it from the dev menu.

M12 replaces music streams (`.VB`, one stereo stream at a time) first, then effect
banks (`.RAW` + `.SDT`, 68 banks in SPU2 RAM, which limits size). Keep streaming
work off the audio callback and preserve loop and seek behavior.

## Hardware sessions needed from you

Agents cannot run the console, so these need you with the Switch:

| When | What |
|---|---|
| M03 | SD read and cancellation behavior |
| M04 | Real boot, HOME, repeated launch |
| M05 | RHI conformance scenes |
| M06 | S1–S4 bring-up, docking, Pro Controller, acceptance run |
| M07 onward | Each feature on real hardware; M07 and M12 need your ears |

## Main risks

| Risk | Why it matters | Where it is handled |
|---|---|---|
| CPU speed | The desktop game thread takes 10.9 ms at Fort Baxter (upstream `PERFORMANCE.md`), on x86 with AVX2. Switch cost is unmeasured and may be many times higher | Measured in M06-11, targeted in M10 |
| Unresolved recompiler diagnostics | 561 unhandled-instruction events and 1,196 indirect-flow warnings are unclassified | M02-11, then M04-12 at runtime |
| Compiler | Upstream requires Clang 19+; devkitA64 is GCC. Full numeric proof is open | M02-04 |
| GL thread ownership | Upstream's render worker versus one GL owner thread | M05-09, M05-10 |
| Memory | Guest RAM, generated tables, stacks and GL targets are not yet budgeted together | M02-13, M06-11 |
| Cutscene pacing without sound | S3 could stall if it waits on audio progress | M06-17 |
| Nouveau GL behavior | Driver limits and shader compile stalls are unmeasured | M05-12, M05-14 |

Beta has no FPS target, so slow performance does not block it. It may still be
too slow to enjoy. M06-11 reports the real number.

## Where old tasks went

| Old task (pre-2026-11-09 ID) | New home (current ID) |
|---|---|
| M06-04 (beyond the first scene) | M14-01 |
| M06-05 widescreen portion | M09 |
| M06-07 on-foot and driving route | M14-04 |
| M06-08 map traversal | M14-02 |
| M06-10 missions, pause, shops | M14-03 |
| M09-05, 06, 07, 10 saves and player docs | M14-05 to M14-08 |
| M09-11, 12, 13 custom textures | M11-03 and M11-05, M11-06, M11-07 |

`M06-13` and `M06-14` keep their IDs. Their "main menu" trigger, set in M01 before
the boot path was checked, now means reaching S2, the first real game picture.
