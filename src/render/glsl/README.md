# GLSL 330 ports of upstream's shaders (OpenGL RHI, M05)

Upstream writes every shader once in HLSL (`upstream/src/render/shaders`) and compiles it to DXBC (D3D11) and
SPIR-V (Vulkan). The Switch's OpenGL 3.3 core backend (`src/rhi/gl/rhi_gl.cpp`) takes GLSL text instead. These
files are hand ports, kept line for line with the HLSL so they can be read side by side.

| GLSL | HLSL | Entries |
|---|---|---|
| `present.glsl` | `present.hlsl` | `vs_main`, `ps_main` |
| `ui.glsl` | `ui.hlsl` | `vs_main`, `ps_main` |
| `gs_util.glsl` | `gs_util.hlsl` | `vs_main`, `ps_blit`, `ps_fill_color`, `ps_fill_depth` |
| `gs_draw.glsl` | `gs_draw.hlsl` | `vs_main`, `vs_world`, `vs_lit`, `ps_main` (SAMPLE_MODE 0-6 x DUAL_SOURCE x SHADER_DEPTH) |

`tools/glsl_shaders.py` reads the variant list from upstream's own `src/render/CMakeLists.txt` (every
`vcs_shader(...)` call, the SAMPLE_MODE loop expanded), so each of upstream's 39 variants gets a GLSL twin named
`<array>_glsl` with the same defines; a variant upstream adds without a GLSL entry point fails the build. Each
variant's text is `#define RHI_VERTEX|RHI_FRAGMENT`, `#define ENTRY_<entry>`, the variant's defines, the vertex
prelude (below) and the file. The device prepends `#version 330 core` and `#define RHI_ZERO_TO_ONE 0|1`.
`shader-manifest.json` (written beside the header) lists every variant with its HLSL source, entry, defines and the
SHA-256 of its text.

## Conventions (what differs from HLSL and how it is resolved)

- **Rows / Y origin.** Every texture holds rows top-down, as in D3D and Vulkan. Where the context has clip control
  (the Switch's Mesa does), drawing into a texture uses `glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE)`: NDC +1 lands
  on row 0, `gl_FragCoord.y` counts rows from the top like `SV_Position`, and an edge exactly on a pixel centre (every
  GS edge: `gs_hw.cpp` moves draws by half a pixel) follows D3D's top-left rule. Without clip control, `rhiPosition(p)`
  multiplies clip-space Y by the uniform `rhi_flipY` (-1 for textures): the same rows, but GL's lower-left tie rule
  (nouveau then dropped the top row of such edges: first M05 review). The window uses `GL_LOWER_LEFT`, unflipped; the
  device converts its viewport / scissor to GL's bottom-left origin.
- **Clip depth.** D3D's clip depth is 0..1. With GL 4.5 / `ARB_clip_control` the device sets `GL_ZERO_TO_ONE` (with
  the origin above) and `RHI_ZERO_TO_ONE` is 1; otherwise `rhiPosition` remaps
  z to `2z - w` (checked with `PS2X_GL_NO_CLIP_CONTROL=1`). `gl_FragDepth` is window depth 0..1 like `SV_Depth`.
- **Bindings.** GLSL 330 has no `layout(binding)`: uniform blocks are named `<Name>_b<N>` (HLSL `register(bN)`)
  and samplers `<name>_t<N>` (`register(tN)`/`sN`, one sampler per unit); the device binds them by name at link.
  Uniform blocks are `std140`, which matches these cbuffers' layout (all `float4`/`int4` members, or a `float2`
  pair at offsets 16/24). Vertex inputs use `layout(location = i)` in `PipelineDesc::attributes` order.
- **Dual source.** `DUAL_SOURCE=1` writes `layout(location = 0, index = 1) out vec4 o_blend` (GL 3.3 core,
  `ARB_blend_func_extended`); the device refuses a pipeline with Src1 factors whose shader lacks it.
- **Interpolation.** `noperspective` and `flat` as in HLSL; the device sets `glProvokingVertex(FIRST)` to match
  D3D's flat-shading vertex.
- **`Load` / `GetDimensions` / `lerp`.** `texelFetch`, `textureSize`, `mix` (the same `FMix` glslang emits for
  `lerp`, so Mesa lowers both the same way).
- **Gather.** GLSL 330 core has no `textureGather`. `sampleGather` reads the same four texels with `texelFetch`:
  the HLSL gathers at an exact texel corner of a power-of-two texture through a nearest sampler whose wrap follows
  WMS/WMT (`gs_hw.cpp` `m_gatherSampler`), so its texels are the corner's two per axis, wrapped (repeat: mask,
  clamp: clamp). Mode 0's `PS2X_HW_GATHER=verify` path compares them with `fetchTexel` exactly as before.
- **`%` on negative values.** HLSL truncates; GLSL 330 leaves it undefined. `wrapc`'s non-power-of-two REPEAT uses
  `floorMod`, the same result as HLSL's `((c % size) + size) % size`.
- **`const` locals.** GLSL 330 needs constant initialisers for `const`; HLSL's `const` locals are plain locals here.
- **Point size.** GL's default point size is 1 (`PROGRAM_POINT_SIZE` off), as D3D11's; no `gl_PointSize` output.

Conformance: `src/rhi_tests` draws every variant on upstream's Vulkan backend and on this one and compares the
readbacks (`tools/rhi_host_tests.py`, `tools/rhi_compare.py`; Switch: `tools/switch_m05_session.py rhi`).
