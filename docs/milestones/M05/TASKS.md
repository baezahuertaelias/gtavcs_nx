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

## Tasks

| Task | Implementation | Proof still needed |
|---|---|---|
| M05-01 backend selection, desktop unchanged, null kept | patch 0008; `VCS_RHI=opengl|null`; default OpenGL only where it is the only built backend | Switch: game boots on OpenGL (`m05-gl`) and on `VCS_RHI=null` (`m05-null`) |
| M05-02 textures, formats, pitch, mips, samplers | `textures`, `mips`, `samplers` scenes | Switch conformance run vs host reference |
| M05-03 buffers, discard, bounds / alignment | `buffers`, `gl_checks` scenes | Switch conformance run |
| M05-04 pipelines, layouts, topology, blend, masks, depth | `topology`, `blend`, `write_mask`, `depth`, `gs_native` | Switch conformance run |
| M05-05 passes, viewport / scissor, clears, resolves, copies, readback, completeness | `passes`, `copies`, `msaa`; incomplete-framebuffer counter | Switch conformance run |
| M05-06 bindings and every GLSL variant | 39 variants, `present_ui`, `gs_util`, `gs_modes`, `gs_tfx`, `gs_wrap`, `gs_rt_reads`, `gs_native` | Switch conformance run (the variants compile on nouveau) |
| M05-07 dual source | GL 3.3 core `SRC1`; `GL_MAX_DUAL_SOURCE_DRAW_BUFFERS` checked at creation; no fallback needed where it is ≥ 1 | Switch caps (`caps.txt`) and `dual_source` scene |
| M05-08 depth / Y / swizzle / sRGB conventions | README conventions; `conventions` scene; clip_control and remap both pass | Switch conformance run; orientation card on the screen |
| M05-09 hazards, fences, staging, deferred destruction; threading decision | owner-thread device with deferred destruction; option B (loader context + fences) opt-in | Switch: `bench.tsv` (A vs B burst) and the `m05-gl` / `m05-gl-worker` game runs; adopt the faster correct one |
| M05-10 SDL back buffer, present, resize | `GlWindow`; swap interval per present mode; drawable size per frame | Switch: window phase (programmatic sizes, docking), game runs |
| M05-11 UI through ImGui / RHI | `uiInit` false: upstream's `render::UiRenderer` through the RHI, as on Vulkan | Switch: `present_ui` scene; game run (ImGui frame drawn every frame) |
| M05-12 every Device method / variant on hardware | the scene list covers every method (see [host/](host/) `log-opengl.txt` `covers` lines) | Switch conformance run |
| M05-13 pixel / depth comparison with the desktop reference | `tools/rhi_compare.py`, tolerances per scene | Switch readbacks vs `vulkan_hw` |
| M05-14 GL diagnostics, resize / resource churn, memory budgets | KHR_debug (synchronous), glGetError per frame, object / byte counters, churn scene with an 8 MiB process budget | Switch: churn under `svcGetInfo(UsedMemorySize)`, debug message count, window phase resizes |

Hardware is required for every task (MILESTONES: "operate correctly on Switch"). See [HARDWARE.md](HARDWARE.md).
