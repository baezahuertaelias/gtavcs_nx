# M05 hardware results

Console: the user's Switch at 192.168.1.91 (192.168.1.107 in round 4), Sphaira by title override (full application memory), FTP + nxlink;
`tools/switch_m05_session.py`. Driver as the console reports it (`hardware-r1/*/caps.txt`): **Mesa 20.1.0-rc3 nouveau,
renderer NV120, OpenGL 4.3 core profile, GLSL 4.30** (asked for 3.3 core): `ARB_clip_control`, viewport arrays,
anisotropy (max 16), KHR_debug, 1 dual-source draw buffer, 8 samples, max texture 16384, max uniform block 65536,
window framebuffer encoding GL_LINEAR. Reference for every comparison: upstream's Vulkan backend on the host GPU
(`build/m05/host/vulkan_hw`: Intel ADL-N, Mesa ANV; `tools/rhi_host_tests.py`); round 3 also against lavapipe (`build/m05/host/vulkan`). Game screenshots stay private under
build/; their SHA-256 are in `*-screenshot-sha256.txt`.

Four rounds. **Round 1** (2026-11-15/16, [hardware/](hardware/)) was the first review's candidate `cb70b74`; the
independent review requested changes ([REVIEW1_RESPONSE.md](REVIEW1_RESPONSE.md)). **Round 2** (2026-11-16,
[hardware-r1/](hardware-r1/)) is the evidence for the game and backend code `75fd248` (game NRO `39071ced…`; unchanged
since). The second independent review requested changes to tests and records ([REVIEW2_RESPONSE.md](REVIEW2_RESPONSE.md));
**round 3** ([hardware-r2/](hardware-r2/)) ran the changed RHI tests (`e0ba4541…`, `9cb098a`). The third review
found the heap growing at every window size change; the probe that measured it is [hardware-r3/rhi-leak](hardware-r3/rhi-leak),
and **round 4** (2026-11-16/17, [hardware-r4/](hardware-r4/)) is the evidence for the fix `dcda9e1`
([REVIEW3_RESPONSE.md](REVIEW3_RESPONSE.md)).

## Round 4 runs (review 3: the window resize leak; fix `dcda9e1`)

| Run | NRO | Mode | Result |
|---|---|---|---|
| `rhi-fixedwin-handheld` | tests `43d6a930` (`2e7df71`: window created resizable, `SDL_SetWindowResizable(false)` before the docking stretch) | handheld throughout | DONE, 127 images, 0 failed checks, 0 GL errors; resize probe as in round 3 (+44.1 MB per resize to 1920x1080, +5.4 MB to 1280x720, +15.2 MB same size; releasing the context: no change); heap flat to the byte (552,004,592) over 40 s without resizes; the program's own `SDL_SetWindowSize` still leaks (+44.1 / +18.2 / +15.2 MB) |
| `rhi-fixedwin` | tests `43d6a930` | from handheld, 4 dock changes by hand | **the window was still resized** at every dock change (1280x720 ↔ 1920x1080; +44.1 MB per dock, +5.4 MB per undock): `SDL_SetWindowResizable(false)` did not clear the flag |
| `rhi-noresizable` | tests `58c458b0` (`dcda9e1`: created without `SDL_WINDOW_RESIZABLE`) | from handheld, 9 dock changes by hand (11-60 s) | DONE, 127, 0 failed checks, 0 GL errors; **window and drawable 1280x720 at every change, heap 554,110,688-554,122,864 across them** (no growth per change); `resizable 0` |
| `rhi-noresizable-docked` | tests `58c458b0` | not docked (the console was not seated; `operation_mode_start 0`) | DONE, 127, 0 failed checks; counts as a handheld run |
| `rhi-noresizable-docked2` | tests `58c458b0` | docked throughout (`operation_mode_start 1`) | DONE, 127, 0 failed checks, 0 GL errors; window 1920x1080, `window/back_buffer_equals_texture` 1920x1080 0 px; heap 731,154,496-731,154,512 over the first 40 s of the window phase |
| `m05-dock` | game `af94eee1` (`dcda9e1`) | from handheld, 4 dock changes by hand | PASS: `[video] drawable=1280x720 window=1280x720` at all four changes (mode 0 → 1 → 0 → 1 → 0), 0 GL errors, orderly stop after 5,320 frames, no crash; `game_fps=21.4 frame_ms=46.8` at 176 s |

**Cause** (switch-sdl2 2.28.5-4, the locked package; source `devkitPro/SDL` branch `switch-sdl-2.28`,
`src/video/switch/SDL_switchvideo.c`): `SWITCH_PumpEvents` calls `SDL_SetWindowSize(1280x720 / 1920x1080)` at an
operation-mode change only when the window has `SDL_WINDOW_RESIZABLE`; `SWITCH_SetWindowSize` destroys the EGL
surface, calls `nwindowSetDimensions` and creates a new surface every time (also at the same size). The heap lost per
resize (round 3 probe) is the old surface's buffers inside SDL / Mesa; the RHI's own objects return to zero. SDL core
changes the flag in `SDL_SetWindowResizable` only through a backend hook, which the Switch backend lacks, hence
`rhi-fixedwin`. **Fix:** the game and the test program create the window at the starting mode's size without the
flag (1280x720 handheld, 1920x1080 docked) and never call `SDL_SetWindowSize`; the compositor scales the window to the
panel or the TV. The user saw the card and the game full-screen and sharp on the TV and the handheld screen after the
changes ([hardware-r4/person-observations.txt](hardware-r4/person-observations.txt)).

**Heap in the game.** `[video]` now logs the heap at every change: 194 MB at the start, 447 / 534 / 572 / 593 MB at
the four changes; the growth follows the texture cache (26 textures at start, 2,966 / 172 MiB before its first
eviction to 2,289, 2,972 / 203 MiB before the second to 2,317), not the changes, and `heap_in_use` at shutdown is 485 MB. A dock change itself adds nothing (the
test program, above, to within 12 KB).

**Readbacks.** The host Vulkan reference was not rebuilt on the round-4 host (Windows; `tools/rhi_host_tests.py` needs
EGL / a Linux Vulkan driver), so the session's compare step failed for lack of `build/m05/host/vulkan_hw/index.tsv`.
Instead every one of the 127 readbacks of all five round-4 RHI runs is byte-identical (SHA-256 from the transcripts)
to round 3's `rhi-r2` and `rhi-r2-docked`, which passed against ANV and lavapipe:
[hardware-r4/readbacks-vs-round3.txt](hardware-r4/readbacks-vs-round3.txt). No round-4 run added a crash report.

## Round 3 runs (review 2's fixes: RHI tests `e0ba4541`, built from `9cb098a`; game unchanged)

| Run | Mode | Result |
|---|---|---|
| `rhi-r2` | handheld throughout (`operation_mode_start 0`) | PASS: 127/127 within tolerance of ANV (101 identical) and of lavapipe (90 identical); 0 failed checks: `uses_mips_negative_control` (far band 109 without mips), `window/half_pixel_edges_top_left_rule`, `window/back_buffer_equals_texture` 1280x720 0 px |
| `rhi-r2-docked` | docked throughout | PASS: the same against both references; window readback with pixel-centre edges 1920x1080 0 px |

All 127 readbacks are byte-identical to round 2's (`rhi-docked2`). Band spreads between every pair of implementations:
[hardware-r2/band-spread-pairs.txt](hardware-r2/band-spread-pairs.txt). Review 2's findings and their resolutions:
[REVIEW2_RESPONSE.md](REVIEW2_RESPONSE.md).

**NRO reproducibility.** Rebuilding an NRO from the same commit can change only its 20-byte build ID (debug line
strings of a different length; found by review 2), so a recorded NRO hash identifies the binary that ran, not a
rebuild; code identity is shown by comparing with the build ID masked.

## Round 2 runs (game and backend code `75fd248`, the current game NRO)

| Run | NRO | Mode | Result |
|---|---|---|---|
| `rhi-docked2` | tests `4457de9e` | docked throughout | PASS: 127 readbacks within tolerance, 101 byte-identical; 0 failed checks (incl. `pixel_centre/*`, `window/back_buffer_equals_texture` 1920x1080 0 px, `*_uses_mips`); 0 GL errors, 0 debug messages; the present probe and the interval steps all as requested (below) |
| `rhi-dock-repro` | tests `4457de9e` | scenes docked; window phase from handheld with 7 dock changes by hand | PASS: 127 within tolerance (101 identical), 0 failed checks; Vsync 60/s through every dock change, also after the program's own `SDL_SetWindowSize` (finding 6) |
| `m05-gl` | game `39071ced` | docked | PASS: 240 s, 0 GL errors / debug / refusals / hazards, clean exit, no crash; picture correct |
| `m05-gl-worker` | game `39071ced` | docked | PASS: loader refused by the sharing check, textures on the owner; 0 GL errors, clean exit |
| `m05-null` | game `39071ced` | docked | PASS: `[present] null`, clean exit |
| `m05-dock` | game `39071ced` | docked; 4 dock changes by hand | PASS: drawable 1920x1080 ↔ 1280x720 at every change, presentation continued, 0 GL errors, clean exit; screenshots correct at both sizes |

The same fixes before the interval change ran on tests `956cfc79` (`30af93b`): `rhi-r1` (docked) and
`rhi-r1-handheld` (**handheld throughout**, `operation_mode_start 0`): 127 within tolerance, 101 identical, 0 failed
checks, window readback 1280x720 0 px. The only code change from `30af93b` to `75fd248` is the swap-interval path,
which the conformance scenes never take (they present Immediate), and every one of the 127 readbacks is
byte-identical between `rhi-r1-handheld` (handheld, `30af93b`), `rhi-r1` (docked, `30af93b`) and `rhi-docked2`
(docked, `75fd248`). `rhi-dock` and `rhi-probe` (`238c0c8`)
located the interval defect. An interrupted send at 14:51 (`crash-before-rhi-docked.txt`, no results fetched) was
rerun as `rhi-docked2`. No run in round 2 added a crash report (`crash-*.txt`).

## Pixel / depth comparison with the desktop reference (M05-13)

The Switch differs from the host GPU reference in 26 of 127 images, all within the declared tolerances
(`hardware-r1/rhi-docked2/rhi-docked2-compare.json`; every round-2 run the same 26):

| Class | Images | Size |
|---|---|---|
| blend unit rounding of non-representable products | blend add / subtract / reverse-subtract colour; gs_modes mode0 and mode2-6 `_dual` / `_dual_noz` (mode1's dual variants and `dual_source/mode_0..6` are byte-identical) | ±1, 1-512 px |
| `floor()` after filtering / `mix` landing within an ulp of an integer (the GS math floors) | gs_wrap pot / npot, gs_native colour | ±1, 4-84 px |
| bilinear sub-texel weight precision | samplers linear, present_ui letterbox / ui | ±1, 11-110 px |
| a half-covered pixel's resolve (255 and 0 averaged: 127.5 → 7f here, 80 on the reference) | msaa/resolved | ±1, 25 px (`unorm:1`) |
| trilinear / anisotropic LOD and footprint (per pixel the implementation's) | samplers trilinear, anisotropic_16 | per 8-row band mean within 1.0 and spread within 4.5 of ANV's (8.5 of lavapipe's; `bands:8,mean:3,stdev:16`, twice the largest spread between any two implementations); `*_uses_mips` and the no-mip negative control pass |
| a viewport with a fractional origin (x 40.25, a quarter pixel; y 4.5, a half pixel): each implementation treats its edges differently (lavapipe rows 4-14 cols 40-50; ANV rows 4-14 cols 40-49; nouveau rows 5-14 cols 40-50; consistent with nouveau rounding its viewport clip rectangle to whole pixels, not confirmed). Upstream's callers never set a fractional origin: `beginPass` sets (0, 0, target size); `render::UiRenderer` (ui_renderer.cpp:181) sets (0, 0, DisplaySize x FramebufferScale), which on the Switch is the drawable exactly (window = drawable, scale 1.0; `[video] window=1920x1080 drawable=1920x1080`), and only an offscreen capture's scale can make its *width* fractional, its origin staying 0 | passes/viewport_scissor | 20 px (24 allowed) |

The other 101 images are **byte-identical** to upstream's Vulkan backend. That includes `pixel_centre/*`, the game's
own configuration (every GS draw moved by half a pixel, so integer edges lie on pixel centres): with the
`glClipControl(GL_UPPER_LEFT)` fix these follow D3D's top-left rule exactly (finding 1).

## Threading decision (M05-09)

Both prototypes ran on the console in round 1 ([hardware/](hardware/)):

- **A, record on workers / every GL call on the main thread** (upstream's design): correct in every run. A burst of
  256 textures of 128x128 with full mip chains costs the owner 0.68-1.36 s (`*/bench.tsv`; it varied between
  sessions, docked and handheld alike): Mesa nouveau's texture upload is slow on this console, a measurement for M10.
- **B, a worker-owned GL context sharing the window context's objects:** with `eglCreateContext` and with SDL's
  `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`, the window context does not see the loader's textures or fences
  (`glBindTexture: non-gen name`, `glWaitSync: not a valid sync object`; `[gl-loader] sharing check: made 1, texture
  seen by the window context 0, fence 0`); sampling them crashed the console in the first run. **Not correct here.**

Decision (user decision 2026-11-11: the correct, faster one): **A**. `freeThreadedCreation()` stays false on the
Switch; B stays opt-in behind the sharing check, so neither the game nor the tests reach unshared objects
(`m05-gl-worker`, both rounds). AGENTS.md's rule (the main thread owns every GL call) stands.

## Window, docking and presentation (M05-10 / M05-14)

- **Swap interval.** `rhi-docked2`'s probe (`window.tsv` `probe` rows), device changes in 3 s steps: Immediate 119.6
  swaps/s (SDL interval 0), Vsync 60.0/s (interval 1), back and forth five times, also after a same-size window resize;
  interval 2 set directly gives 30.0/s. The interval experiments: 1 → 60.0/s, 0 → 120.0/s, 2 → 30.0/s. **The defect
  behind the first review's finding 2:** a change from interval 2 straight to 1 was reported by SDL but not applied
  (30/s remained, `rhi-dock`); the device now passes through 0 (`75fd248`) and the same step lands at 60/s within a
  second (`rhi-docked2`, 55.6 s). Round 1's "Vsync" windows measured exactly the interval-0 pattern (120/s,
  alternating ~16 / ~0.4 ms); those builds did not log the interval, so which change failed there is not established;
  round 1's claim of 60 Hz in both modes was wrong and is withdrawn.
- **Window size since round 4** (`dcda9e1`): fixed at the starting mode's size, the window and drawable no
  longer change at a dock change (round 4 above); the round-2 / round-3 rows below describe the resizable window
  before the fix.
- **Docking in the game** (`m05-dock`, round 2): `[video] drawable=1920x1080 … mode=1` ↔ `1280x720 … mode=0` at all four
  changes; frames between changes 22-47 ms; one 0.43-0.86 s frame at each re-dock; screenshots after each change show
  the full picture at the new size, 4:3 pillarboxed. The game never calls `SDL_SetWindowSize` (only the test program
  does).
- **Docking in the test program** (`rhi-dock-repro`): mode 0 ↔ 1 and SDL's window 1280x720 ↔ 1920x1080 at 23.9 /
  31.3 / 41.4 / 51.5 s, and after the program's own `SDL_SetWindowSize` (55-64 s) at 80.8 / 91.0 / 102.3 s; Vsync
  60/s before and after every change (one short second at each). **The round-1 black screen after a re-dock
  (`rhi-attended2`, ~1 ms swaps) did not reproduce** on the fixed build in this exact order; its cause is not
  established (that build also had the interval defect).
- **The window path read back** (finding 5): `window/back_buffer_equals_texture`: the window back buffer, drawn with
  sub-viewports, scissors and the UI pipeline with its clip scissor and read with `glReadPixels(GL_BACK)` before the
  swap, equals the same drawing in a texture: 0 differing pixels at 1920x1080 docked and 1280x720 handheld.
- **Diagnostics.** KHR_debug synchronous output on in every run: 0 messages and 0 GL errors in every round-2 run.
- **Churn** (200 rounds, `churn/*`): GL objects and bytes back to their start, 0 deletions pending, programs 35; heap
  in use (newlib `mallinfo`; libnx reserves the whole heap at start, so `svcGetInfo(UsedMemorySize)` stays constant)
  +2.5 MiB after the churn, within the 8 MiB budget, +10 to +13 MiB at its peak. Copy and readback framebuffers now
  detach their textures after use (finding 7). Whether `mallinfo` covers all of nouveau's GPU allocations is not
  established; the backend's own byte counters cover the textures and buffers it makes. In the game the texture cache
  peaks near upstream's 3,000-entry cap (2,987 / 210 MiB) and evicts.

## Game through OpenGL (not M06 acceptance)

`m05-gl` and `m05-dock`: the flyover and Fort Baxter with the world, the player, HUD (clock, health, money), radar and
the mission marker, correct after the clip-control change. At the end of the 240 s run (docked, 1x):
`game_fps=21.3 frame_ms=47.0 host_fps=34 gl_ms=28.8`; the 180 s dock run `game_fps=18.6 frame_ms=53.7`; the user
saw ~20 fps while playing in round 1. Measured, not a target (M10). No controllability, 5-minute or beta claim: that is M06.

## People at the console

The user, 2026-11-16: the orientation card correct (red top-left, green top-right, blue bottom-left, white
bottom-right): "Everything looks right" and "I've saw those colors"; the docking changes in `m05-dock` and
`rhi-dock-repro` done by hand (the TV was not watched in round 2: the logs above are the evidence there).
