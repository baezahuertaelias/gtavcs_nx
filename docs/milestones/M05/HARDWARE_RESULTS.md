# M05 hardware results

Console: the user's Switch at 192.168.1.91, Sphaira by title override (full application memory), FTP + nxlink;
`tools/switch_m05_session.py`. Driver as the console reports it (`hardware-r1/*/caps.txt`): **Mesa 20.1.0-rc3 nouveau,
renderer NV120, OpenGL 4.3 core profile, GLSL 4.30** (asked for 3.3 core): `ARB_clip_control`, viewport arrays,
anisotropy (max 16), KHR_debug, 1 dual-source draw buffer, 8 samples, max texture 16384, max uniform block 65536,
window framebuffer encoding GL_LINEAR. Reference for every comparison: upstream's Vulkan backend on the host GPU
(`build/m05/host/vulkan_hw`: Intel ADL-N, Mesa ANV; `tools/rhi_host_tests.py`). Game screenshots stay private under
build/; their SHA-256 are in `*-screenshot-sha256.txt`.

Two rounds. **Round 1** (2026-11-15/16, [hardware/](hardware/)) was the first review's candidate `cb70b74`; the
independent review requested changes ([REVIEW1_RESPONSE.md](REVIEW1_RESPONSE.md)). **Round 2** (2026-11-16,
[hardware-r1/](hardware-r1/)) is the evidence for the current candidate: game NRO `39071ced…` and RHI-tests NRO
`4457de9e…`, both built from `75fd248` (the code has not changed since; later commits are records only).

## Round 2 runs (current candidate code, `75fd248`)

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
| trilinear / anisotropic LOD and footprint (per pixel the implementation's) | samplers trilinear, anisotropic_16 | per 8-row band mean within 1.1 and spread within 4.5 of the reference's (`bands:8,mean:3,stdev:8`); `*_uses_mips` pass |
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
- **Docking in the game** (`m05-dock`): `[video] drawable=1920x1080 … mode=1` ↔ `1280x720 … mode=0` at all four
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
`game_fps=18.6 frame_ms=53.7 host_fps=32 gl_ms=32.2`; the dock run `game_fps=21.3`; the user saw ~20 fps while
playing in round 1. Measured, not a target (M10). No controllability, 5-minute or beta claim: that is M06.

## People at the console

The user, 2026-11-16: the orientation card correct (red top-left, green top-right, blue bottom-left, white
bottom-right): "Everything looks right" and "I've saw those colors"; the docking changes in `m05-dock` and
`rhi-dock-repro` done by hand (the TV was not watched in round 2: the logs above are the evidence there).
