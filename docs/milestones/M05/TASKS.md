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
| Conformance scenes (23), host runner (Vulkan + GL), Switch runner (+ threading measurement, window phase), comparer | `src/rhi_tests/`, `tests/rhi/CMakeLists.txt`, `tools/rhi_host_tests.py`, `tools/rhi_compare.py` |
| Hardware session | `tools/switch_m05_session.py`, `tests/switch/m05-*.cfg`, [HARDWARE.md](HARDWARE.md) |

## Host results (2026-11-14, root `cd95e57` + this record's working tree)

`python tools/rhi_host_tests.py --churn 200` on the Linux host (Intel ADL-N, Mesa 25.0.7). The desktop reference is
upstream's own Vulkan backend (`rhi_vulkan.cpp`, upstream's pinned volk / VMA / Vulkan-Headers) on the same GPU (ANV)
and on lavapipe. Records in [host/](host/).

| Comparison | Readbacks | Byte-identical | Within tolerance | Self-checks |
|---|---|---|---|---|
| Vulkan ANV vs OpenGL iris (default: clip_control, viewport_array) | 125 | 125 | 125 | 48 / 78 pass |
| Vulkan ANV vs OpenGL, `PS2X_GL_NO_CLIP_CONTROL=1` (shader depth remap) | 125 | 125 | 125 | pass |
| Vulkan ANV vs OpenGL, `PS2X_GL_NO_VIEWPORT_ARRAY=1` | 125 | 124 | 125 | pass |
| Vulkan ANV vs OpenGL, `PS2X_GL_WORKER_CONTEXT=1` (free-threaded) | 125 | 125 | 125 | pass, incl. `free_threaded/sampled_by_owner` |
| Vulkan lavapipe vs OpenGL iris | 125 | 89 | 125 | pass |

The 36 lavapipe differences are the same 36 as lavapipe vs ANV (both upstream backends): a software rasteriser's
blend / filter rounding (±1, bilinear ±2), anisotropic and trilinear footprints, multisample sample positions; none
between the two hardware paths. The one no-viewport-array difference is the scene's deliberately fractional
viewport (10.5 px), rounded to whole pixels without `glViewportIndexedf`. The first host run found 21 differences;
every one was a test-design fault, fixed in the scenes, not in the backend: texture coordinates exactly on texel
boundaries (a last-ulp interpolation difference picks the neighbour texel) and colours at an exact .5 after x255
(rounding ties every API leaves to the implementation).

## Tasks (all observed on the Switch, 2026-11-15/16: [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md))

| Task | Implementation | Switch proof |
|---|---|---|
| M05-01 backend selection, desktop unchanged, null kept | patch 0008; `VCS_RHI=opengl\|null`; default OpenGL only where it is the only built backend | `m05-gl` boots on OpenGL; `m05-null` on `VCS_RHI=null` |
| M05-02 textures, formats, pitch, mips, samplers | `textures`, `mips`, `samplers` | checks pass; identical to upstream Vulkan except filtering (±1, implementation-defined footprints) |
| M05-03 buffers, discard, bounds / alignment | `buffers`, `gl_checks` | checks pass; refusals counted without GL errors |
| M05-04 pipelines, layouts, topology, blend, masks, depth | `topology`, `blend`, `write_mask`, `depth`, `gs_native` | identical but blend ±1 rounding |
| M05-05 passes, viewport / scissor, clears, resolves, copies, readback, completeness | `passes`, `copies`, `msaa` | checks pass; 0 incomplete framebuffers |
| M05-06 bindings and every GLSL variant | 39 variants | all compile and link on Mesa 20.1 nouveau (0 failures, 0 debug messages); drawn |
| M05-07 dual source | GL 3.3 core `SRC1` | `max_dual_source_draw_buffers 1`; dual variants ±1 |
| M05-08 depth / Y / swizzle / sRGB conventions | README conventions | `conventions` checks pass; the user saw the orientation card correct |
| M05-09 hazards, fences, staging, deferred destruction; threading decision | owner-thread device, deferred destruction; B opt-in with a sharing check | A adopted: B's shared context shares nothing on the Switch (and crashed when forced) |
| M05-10 SDL back buffer, present, resize | `GlWindow`, drawable per frame | game and test program follow every dock change; programmatic sizes |
| M05-11 UI through ImGui / RHI | `render::UiRenderer` (as on Vulkan) | `present_ui` within ±1; the game's ImGui frame every frame, 0 GL errors |
| M05-12 every Device method / variant on hardware | 23 scenes | 125 readbacks, 0 failed checks, handheld and docked |
| M05-13 pixel / depth comparison with the desktop reference | `tools/rhi_compare.py` | 125/125 within tolerance, 99 identical; differences classified |
| M05-14 GL diagnostics, churn, memory budgets | KHR_debug, counters, churn | 0 GL errors / debug messages; churn objects and bytes return; heap +2.5 MiB of 8 MiB |
