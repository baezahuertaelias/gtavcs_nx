# M05 — OpenGL RHI and shader conformance: task record

Branch `milestone/m05-opengl-rhi`, cut from main `66a6b65` (M04 merged). Upstream pin unchanged
(`ecf150e`); new upstream patch `0008-opengl-backend-seam.patch`. A task is checked in MILESTONES.md only with its
hardware evidence; this file says what each task's proof is and where it stands.

## What was built

| Piece | Where |
|---|---|
| Backend id `OpenGL` (after `Null`, desktop default unchanged), `GlWindow` (the platform's context: entry points, swap, drawable size, swap interval, optional loader context), `createOpenGLDevice`, the GLSL entry in `gsShaders`, the presenter filling `DeviceDesc::gl` | `patches/upstream/0008-opengl-backend-seam.patch` (`[gta-recomp]` markers; built only with `VCS_HAVE_OPENGL`, which no desktop build defines) |
| OpenGL 3.3 core device: every `rhi::Device` method, bounds / alignment / hazard / owner-thread refusals, deferred destruction, KHR_debug, statistics | `src/rhi/gl/rhi_gl.{h,cpp}` |
| GLSL 330 ports of `present`, `ui`, `gs_util`, `gs_draw`; the 39-variant header and shader manifest | `src/render/glsl/*.glsl`, `README.md`; `tools/glsl_shaders.py` |
| SDL2 platform: `GlWindow` callbacks; EGL loader context (opt-in) | `src/platform/platform_switch_sdl2.cpp`, `src/platform/gl_loader_egl.{h,cpp}` |
| `main.c`: the RHI swaps when it presents (`gtavcs_core_presents`); a cleared frame only with `VCS_RHI=null` | `src/main.c`, `src/core.h`, `src/core/core.cpp` |
| Conformance scenes (24), host runner (Vulkan + GL), Switch runner (+ threading measurement, window phase), comparer | `src/rhi_tests/`, `tests/rhi/CMakeLists.txt`, `tools/rhi_host_tests.py`, `tools/rhi_compare.py` |
| Hardware session | `tools/switch_m05_session.py`, `tests/switch/m05-*.cfg`, [HARDWARE.md](HARDWARE.md) |

## Host results (final candidate code `75fd248`)

`python tools/rhi_host_tests.py --churn 200` on the Linux host (Intel ADL-N, Mesa 25.0.7). The desktop reference is
upstream's own Vulkan backend (`rhi_vulkan.cpp`, upstream's pinned volk / VMA / Vulkan-Headers) on the same GPU (ANV)
and on lavapipe. Records in [host/](host/).

| Comparison | Readbacks | Byte-identical | Within tolerance | Self-checks |
|---|---|---|---|---|
| Vulkan ANV vs OpenGL iris (default: clip_control, viewport_array) | 127 | 127 | 127 | ANV 53/53, GL 84/84 pass |
| Vulkan ANV vs OpenGL, `PS2X_GL_NO_CLIP_CONTROL=1` (shader flip and depth remap) | 127 | 127 | 127 | pass |
| Vulkan ANV vs OpenGL, `PS2X_GL_NO_VIEWPORT_ARRAY=1` | 127 | 126 | 127 | pass |
| Vulkan ANV vs OpenGL, `PS2X_GL_WORKER_CONTEXT=1` (free-threaded) | 127 | 127 | 127 | pass, incl. `free_threaded/sampled_by_owner` |
| Vulkan lavapipe vs OpenGL iris | 127 | 91 | 127 | lavapipe 53/53 pass |

The lavapipe differences are the same as lavapipe vs ANV (both upstream backends): a software rasteriser's rounding.
Before the first commit, a host run found 21 differences that were test-design faults (coordinates on texel
boundaries, colours on rounding ties), fixed in the scenes. Intel follows D3D's tie rule even with the shader Y flip,
so only the Switch distinguished it (review 1, finding 1).

## Tasks (observed on the Switch: [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md), rounds 2-5; reviews: [REVIEW1_RESPONSE.md](REVIEW1_RESPONSE.md), [REVIEW2_RESPONSE.md](REVIEW2_RESPONSE.md), [REVIEW3_RESPONSE.md](REVIEW3_RESPONSE.md), [REVIEW4_RESPONSE.md](REVIEW4_RESPONSE.md))

| Task | Implementation | Switch proof (RHI scenes: round 3; game runs: round 2; unless noted) |
|---|---|---|
| M05-01 backend selection, desktop unchanged, null kept | patch 0008; `VCS_RHI=opengl\|null`; default OpenGL only where it is the only built backend | `m05-gl` on OpenGL, `m05-null` on `VCS_RHI=null` |
| M05-02 textures, formats, pitch, mips, samplers | `textures`, `mips`, `samplers` | checks pass; identical but filtering (±1; trilinear / anisotropic within the band tolerance; `*_uses_mips`) |
| M05-03 buffers, discard, bounds / alignment | `buffers`, `gl_checks` | checks pass; refusals counted without GL errors |
| M05-04 pipelines, layouts, topology, blend, masks, depth | `topology`, `blend`, `write_mask`, `depth`, `gs_native` | identical but blend ±1 rounding |
| M05-05 passes, viewport / scissor, clears, resolves, copies, readback, completeness | `passes`, `copies`, `msaa`; copy FBOs detached | checks pass, 0 incomplete framebuffers; viewport / scissor differences explained; resolve ±1 |
| M05-06 bindings and every GLSL variant | 39 variants | all compile and link on Mesa 20.1 nouveau (0 failures, 0 debug messages) and draw |
| M05-07 dual source | GL 3.3 core `SRC1` | `max_dual_source_draw_buffers 1`; dual variants ±1 |
| M05-08 depth / Y / swizzle / sRGB conventions | upper-left clip control for textures, README | `conventions` and `pixel_centre` checks pass; pixel-centre readbacks identical; the user saw the orientation card correct (round 1) |
| M05-09 hazards, fences, staging, deferred destruction; threading decision | owner-thread device, deferred destruction, copy FBOs detached; B opt-in with a sharing check | A adopted (round 1: B shares nothing on the Switch and crashed when forced); `m05-gl-worker` falls back safely |
| M05-10 SDL back buffer, present, resize | `GlWindow`, drawable per frame, swap interval through 0; window fixed at the starting mode's size, not `SDL_WINDOW_RESIZABLE` (`dcda9e1`) | rounds 4-5: game and test program keep the window through every dock change from a handheld start (1280x720) and a docked start (1920x1080), test-program heap flat across the changes, picture full-screen and sharp on TV and handheld; output only at the starting mode's size (M06-20); probe: Immediate 119.6/s, Vsync 60.0/s in every step |
| M05-11 UI through ImGui / RHI | `render::UiRenderer` (as on Vulkan) | `present_ui` within ±2; the UI pipeline and its clip scissor read back from the window identical to a texture |
| M05-12 every Device method / variant on hardware | 24 scenes + window readback | 127 readbacks, 0 failed checks, handheld and docked |
| M05-13 pixel / depth comparison with the desktop reference | `tools/rhi_compare.py` | 127/127 within tolerance, 101 identical; the 26 differences classified |
| M05-14 GL diagnostics, churn, memory budgets | KHR_debug, counters, churn; heap at every `[video]` change | 0 GL errors / debug messages; churn objects and bytes return; heap +2.5 MiB of 8 MiB; rounds 4-5: no growth per dock change in the test program (within 12 KB / 2 KB; the SDL resize leak removed); game heap sampled at changes only (inference) |
