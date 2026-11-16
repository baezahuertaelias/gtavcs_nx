# M05 hardware results (2026-11-15 / 16)

Console: the user's Switch at 192.168.1.91, Sphaira by title override (full application memory), FTP + nxlink;
`tools/switch_m05_session.py`. Driver as the console reports it (`hardware/*/caps.txt`): **Mesa 20.1.0-rc3 nouveau,
renderer NV120, OpenGL 4.3 core profile, GLSL 4.30** (asked for 3.3 core): `ARB_clip_control`, viewport arrays,
anisotropy (max 16), KHR_debug, 1 dual-source draw buffer, 8 samples (colour / depth), max texture 16384, max uniform
block 65536, default framebuffer encoding GL_LINEAR (0x2601). Records: [hardware/](hardware/) (test-program outputs,
comparisons, session transcripts, excerpts of the game logs; game screenshots stay private under build/, their
SHA-256 in `hardware/*-screenshot-sha256.txt`). Reference for every comparison: upstream's Vulkan backend on the host
GPU (`build/m05/host/vulkan_hw`, Intel ADL-N ANV, `tools/rhi_host_tests.py`).

## Runs (in order)

| Run | NRO (sha256, source commit) | Mode | Result |
|---|---|---|---|
| `rhi-handheld` (first try) | tests `7c916dfc...` (`99f977a`) | docked | 125 readbacks within tolerance (99 identical); the 25 failed checks are the worker-context scene; **the B benchmark crashed the console** (Atmosphère report). Fixed in `610c830`, see M05-09 |
| `rhi-run2` | tests `3eb9a53e...` (`610c830`) | docked throughout | **PASS**: 125/125 within tolerance (99 identical), 0 failed checks, 0 GL errors, 0 debug messages, no crash; loader context refused by the sharing check |
| `m05-gl` | game `8ba17eda...` (`610c830`) | docked | **PASS**: 240 s, 9,599 host frames, ~4.0 M draws, 0 GL errors / debug / refusals / hazards; clean exit, no crash; flyover and Fort Baxter drawn |
| `m05-gl-worker` | game `8ba17eda...` (`610c830`) | docked | **PASS**: `PS2X_GL_WORKER_CONTEXT=1`: loader refused by the sharing check, textures on the owner; 0 GL errors, clean exit. The user moved the camera during it (stick input reached the game); its timings are not compared |
| `m05-dock` | game `60d315d8...` (`a69402c`) | docked → 4 changes by hand | **PASS**: drawable 1920x1080 ↔ 1280x720 on every mode change, presentation continued, 0 GL errors, clean exit; screenshots at both sizes correct |
| `m05-null` | game `60d315d8...` (`a69402c`) | docked | **PASS**: `VCS_RHI=null`: `[present] null`, 3,600 frames in 60 s, clean exit |
| `rhi-attended2` | tests `3eb9a53e...` (`610c830`) | handheld at the benchmark, docked at the window phase | **PASS** (125/125, 0 failed); the screen went black after the second re-dock: that build resized its own window before the docking (see M05-10) |
| `rhi-final` | tests `5169f24c...` (`284f5ac`) | started docked, undocked during the scenes; 4 dock changes by hand | **PASS**: 125/125 (99 identical), 0 failed; the user: card colours right, picture right after every change |
| `rhi-handheld-full` | tests `5169f24c...` (`284f5ac`) | handheld throughout (`operation_mode_start 0`, mode 0 at the end) | **PASS**: 125/125 (99 identical), 0 failed checks, 0 GL errors, 0 debug messages |

Every run's crash-report listing before and after is in `hardware/crash-*.txt`; only the first try added a report.

## Pixel / depth comparison with the desktop reference (M05-13)

The Switch readbacks differ from the host GPU reference in 26 of 125 images, all within the declared tolerances
(`hardware/rhi-handheld-full/rhi-handheld-full-compare.json`; docked run2 the same), every difference in a class the
host already explained:

| Class | Images | Size |
|---|---|---|
| blend unit rounding of non-representable products (incl. every dual-source variant) | blend add / subtract / reverse-subtract colour, gs_modes `*_dual*` | ±1, 1-512 px |
| `floor()` after filtering or `mix` landing within an ulp of an integer (the GS math floors) | gs_wrap pot / npot, gs_native colour | ±1, 4-84 px |
| bilinear sub-texel weight precision | samplers linear, present_ui letterbox / ui | ±1, 11-110 px |
| a deliberately fractional viewport's edge pixels (10.5 px wide; both have viewport arrays) | passes/viewport_scissor | 20 px (24 allowed) |
| multisample sample positions on edge pixels | msaa/resolved | 25 px (implementation-defined) |
| anisotropic / trilinear footprint (the spec's "approximation") | samplers anisotropic_16, trilinear | implementation-defined |

The other 99 images (textures, pitches, R32F, D32F depth values, every Compare function, shader depth vs raster
depth, copies, clears, scissor, Y origin, clip depth, UNORM encode, all non-dual GS modes, TFX / fog / alpha tests,
render-target reads, the native world / lit shaders' depth) are **byte-identical** to upstream's Vulkan backend.

## Threading decision (M05-09)

Both prototypes ran on the console:

- **A, record on workers / every GL call on the main thread** (upstream's design): correct in every run. A burst of
  256 textures of 128x128 with full mip chains costs the owner 676-782 ms docked and 1,215-1,356 ms handheld
  (`hardware/*/bench.tsv`): Mesa nouveau's texture upload on this console is slow, a measurement for M10.
- **B, a worker-owned GL context sharing the window context's objects:** made with `eglCreateContext` (first try) and
  with SDL's `SDL_GL_SHARE_WITH_CURRENT_CONTEXT` (the fix): in both, the window context does not see the loader's
  textures or fences (`glBindTexture: non-gen name`, `glWaitSync: not a valid sync object`; `[gl-loader] sharing
  check: made 1, texture seen by the window context 0, fence 0`). Sampling such textures crashed the console.
  **B is not correct on this driver / SDL backend.**

Decision (rule in HARDWARE.md, user decision 2026-11-11: the correct, faster one): **A**. `freeThreadedCreation()`
stays false on the Switch; B remains an opt-in that the sharing check disables wherever objects are not shared,
so neither the game nor the tests can reach unshared objects (`m05-gl-worker`). AGENTS.md's ownership rule (the
main thread owns every GL call) stands.

## Window, docking and presentation (M05-10 / M05-14)

- Game (`m05-dock`): the back buffer follows every dock change (`[video] drawable=1920x1080 ... mode=1` ↔
  `1280x720 ... mode=0`), the RHI's back buffer the drawable; frames between changes 22-46 ms; one 0.43-0.46 s frame
  at each re-dock; the screenshots after each change show the full picture, 4:3 pillarboxed, HUD at its aspect.
- Test program (`rhi-final`, after moving its own `SDL_SetWindowSize` calls behind the docking): mode 0 ↔ 1 and SDL's
  window 1280x720 ↔ 1920x1080 at 17.6 / 23.1 / 39.9 / 48.4 s; one 0.55-0.83 s frame at each change; then
  programmatic sizes 1920x1080 / 960x540 / 1280x720 followed. The user, at the console, reported the card correct and the picture right after every change ("Everything looks right", 2026-11-16).
  `rhi-attended2` went black after a re-dock: that build had called `SDL_SetWindowSize` before the
  docking; the game never sets a window size. Recorded for M13's docking work.
- Presentation: 16.0-16.2 ms per frame with swap interval 1, and also with interval 0 (Immediate): the console
  presents at 60 Hz either way. While the console is moved between dock and hand, swaps briefly stop waiting
  (~1 ms) until the new display takes over.
- Diagnostics: KHR_debug synchronous output on in every run: 0 messages after the `g_pixel` initialisation (the first
  try had Mesa's "used uninitialized" compiler warning, high severity); 0 GL errors in every passing run.
- Churn (200 rounds, `churn/*` checks): GL objects and bytes back to their start, 0 deletions pending, programs 35
  (bounded by the variants); heap in use (newlib `mallinfo`; libnx reserves the whole heap at start, so
  `svcGetInfo(UsedMemorySize)` is constant) +2.5 MiB after the churn, within the 8 MiB budget (peak during it +10 to +13 MiB). In the game the
  texture cache peaked at 2,888 textures / 205 MiB and evicted to 2,284 (upstream's 3,000-entry cap).

## Game through OpenGL (not M06 acceptance)

`m05-gl`: the flyover at 8 s, Fort Baxter with the world, the player, HUD (clock, health, money), radar and the
mission marker from 60 s; the same frames as the desktop's (docs/milestones/M05/desktop). At 236 s (docked, 1x):
`game_fps=16.9 frame_ms=59.0 game_ms=24.2 render_ms=10.0 gl_ms=25.3 host_fps=38`; the user saw ~20 fps while
playing. Measured, not a target (M10). No controllability, 5-minute or beta claim: that is M06.
