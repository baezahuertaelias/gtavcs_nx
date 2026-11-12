# Manual porting checklist

Based on `upstream/` at `ecf150e53f87e4656dfe81eacd922839d952b282`.
Paths below are relative to that checkout. The root scaffold does not yet
compile these files. Items are unchecked until implementation and proof exist.

## 1. Build and ARM64 foundation

- [ ] `CMakeLists.txt`: split host tools from target runtime; add a Switch branch;
  replace the unconditional Clang >=19 gate only after GCC compatibility proof;
  do not select AVX2, Windows DLLs, desktop SDL3 or desktop FFmpeg on Switch.
  Keep desktop configuration working. Fix `CMAKE_SOURCE_DIR` assumptions if
  building upstream as a subdirectory of this port.
- [ ] `third_party/PS2Recomp/ps2xRuntime/CMakeLists.txt` and
  `third_party/PS2Recomp/ps2xRuntime/cmake/ReleaseMode.cmake`: remove x86 CPU/LTO/
  sanitizer/linker assumptions for Switch; link the port-owned host backend;
  disable optional desktop tools/backends, not the required guest scheduler.
- [ ] `third_party/PS2Recomp/ps2xRuntime/include/ps2_guest_cc.h`: preserve consistent
  declarations/function pointer ABI across generated code, hooks and scheduler.
  It already uses default calling convention on ARM64; `preserve_none` is x86-only.
- [ ] `third_party/PS2Recomp/ps2xRuntime/include/ps2_runtime.h`,
  `include/ps2_runtime_macros.h`, `include/runtime/ps2_memory.h`,
  `include/runtime/ps2_vu1_compiled.h` (same runtime directory), plus
  `src/lib/ps2_memory.cpp`: port `__m128i`/SSE operations with semantic tests;
  preserve full guest register bits, alignment and loads/stores.
- [ ] `third_party/PS2Recomp/ps2xRuntime/src/lib/vu/ps2_vu1_{core,upper,lower}.cpp`
  and `include/runtime/ps2_host_std.h`, `src/lib/ps2_host_std.cpp`: inspect numeric,
  timing, thread waits, TLS and host intrinsics. Port only proven platform needs;
  preserve VU behavior. Audit generated EE/VU C++ too; do not edit generated output
  as the maintained fix.

## 2. Startup and asset root

- [ ] `src/main.cpp`: replace executable-directory/ISO discovery, desktop user-data
  directories, process/debug setup and blocking loop with the port's lifecycle.
  Wire a C++ adapter implementing `src/core.h` in the root port; replace
  `src/core_stub.c` in the target once the real adapter exists. Never link both.
  M04: `src/core/core.cpp` replaces the stub (deleted); launcher steps exposed by
  overlay 0007. Box stays open until hardware proof.
- [ ] `src/disc/iso_image.{h,cpp}` and `src/disc/vcs_disc_id.h`: retain build identity
  validation; add extracted-file support, not a different regional executable.
  Local ISO executable check passed as the supported USA build.
- [ ] `third_party/PS2Recomp/ps2xRuntime/include/runtime/ps2_disc_image.h` and
  `src/lib/ps2_disc_image.cpp`: implement `IPS2DiscImage` for the mandated folder
  root. A host-generated manifest must preserve original LBA extents, sizes,
  directory entries and deterministic sector reads; identify any raw reads beyond
  file data and define correct behavior. Test async IOP/CDVD reads and streaming.
- [ ] `src/settings/settings.cpp`: keep settings/mods and Switch userdata under
  the agreed root; remove inherited XDG/executable-path assumptions for this target.
  Keep user saves distinct from disposable validation output.

## 3. Window, controllers and ImGui — replace at the owning seam

- [ ] `src/platform/platform.h`: retain the graphics-API-free interface and its
  thread-safe input snapshot contract. Add only missing Switch lifecycle hooks.
- [ ] `src/platform/platform_sdl3.cpp`: use a new sibling
  `platform_switch_sdl2.cpp`, selected only for Switch. Translate SDL3 APIs to
  SDL2 equivalents: `SDL_Init` returns 0 on success, SDL3 `Gamepad` becomes SDL2
  `GameController`, event names/fields differ, ticks/delay and window APIs differ.
  Use `SDL_GL_CreateContext`/`SDL_GL_SwapWindow` and drawable pixels. Remove desktop
  monitor placement, mouse capture requirements and Vulkan surface callbacks from
  this implementation. Handle docking, focus and suspend without stale input.
- [ ] `src/platform/CMakeLists.txt`: link packaged switch-sdl2 rather than fetching
  desktop SDL3; compile `imgui_impl_sdl2.cpp` in place of `imgui_impl_sdl3.cpp`.
  Keep upstream's pinned ImGui core consistent with its UI renderer.
- [ ] `src/game/vcs_input.{h,cpp}`: retain action bindings and PS2 active-low pad
  encoding; replace keyboard defaults with positional controller defaults for
  Switch. Check sticks, dead zones, digital ZL/ZR thresholds and rumble.
- [ ] `src/game/vcs_frontend.cpp`, `src/game/vcs_aim.cpp`,
  `src/game/vcs_carcam.cpp`, `src/game/vcs_freecam.cpp`: audit mouse/keyboard paths,
  ensure pad camera and pause-map navigation function without desktop devices.
  Do not rewrite their guest/game logic merely to introduce SDL.
- [ ] `third_party/PS2Recomp/ps2xRuntime/src/lib/ps2_pad.cpp`: validate final
  DualShock report, disconnect state and vibration forwarding through the existing
  host interface. Keep SDL types out of guest code.
- [ ] `src/ui/settings_menu.cpp`, `src/ui/trainer_menu.cpp`: enable ImGui gamepad
  navigation, readable handheld layout and an overlay toggle. Consume menu input
  so gameplay does not also act on it; preserve normal Start/Select controls.

## 4. New renderer backend — not a Vulkan-to-GL library rename

- [ ] `src/rhi/rhi.h`, `src/rhi/rhi.cpp`, `src/rhi/CMakeLists.txt`: introduce a
  selected OpenGL backend and GLAD linkage, leaving Vulkan/D3D11 choices intact
  on desktop. No volk/dlopen/Vulkan libraries in the Switch GL build.
- [ ] `src/rhi/vulkan/rhi_vulkan.cpp`: use its interface implementation as a
  behavioral reference for a new `src/rhi/opengl/rhi_opengl.cpp`; do not patch its
  Vulkan calls into GL piecemeal. Map buffers, textures, uniforms, blend/depth,
  render targets, fences, readback and frame lifetime explicitly.
- [ ] `src/render/CMakeLists.txt`, `src/render/gs_shaders.{h,cpp}` and
  `src/render/shaders/{present,gs_draw,gs_util,ui}.hlsl`: add corresponding GLSL
  generation/assets and bindings. Current DXBC/SPIR-V output is insufficient for
  GL 3.3. Verify clip-space/depth conventions, texture origin, swizzles,
  framebuffer feedback and dual-source blending requirements.
- [ ] `src/render/presenter.cpp`, `src/render/ui_renderer.cpp`: bind the SDL GL
  backbuffer, separate internal render scale/output resolution, rebuild targets
  on docking and preserve ImGui scissor/blend state. Reuse the existing RHI UI
  renderer instead of adding a second ImGui renderer accidentally.
- [ ] `src/game/vcs_display.cpp`, `src/game/vcs_hud.cpp`,
  `src/game/vcs_hle_world.cpp`: verify viewport/culling/HUD/widescreen and submit
  through the RHI. Compare visible gameplay before optimizing draw submission.

## 5. Sound, movies and diagnostics

- [ ] `src/platform/platform_sdl3.cpp` audio section (new SDL2 implementation):
  replace SDL3 stream-device APIs with SDL2 audio-device plus `SDL_AudioStream`.
  Use a bounded producer/consumer design; SDL2's Switch backend owns audren.
- [ ] `third_party/PS2Recomp/ps2xRuntime/src/lib/ps2_audio_out.cpp`,
  `src/sound/libsd.cpp`, `src/game/vcs_sound.cpp`, `src/game/vcs_volume.cpp`:
  retain mixer/SPU2 timing and volume semantics; connect output to the SDL2
  platform consumer. Verify audible effects, music, streaming and underruns.
- [ ] `src/game/vcs_movie.cpp`: supply Switch-target decoder libraries or a
  documented staged bring-up policy; desktop FFmpeg binaries cannot be linked.
  A skipped intro is not proof that cutscenes work.
- [ ] `src/debug/crash.cpp`, `src/debug/watchdog.cpp`, `src/debug/profiler.cpp`,
  `src/debug/rig.cpp`: gate desktop signals/debugger/process/socket assumptions;
  preserve useful logs and timing with Switch-supported facilities. Test shutdown
  and suspend with active worker/audio threads and pending GPU work.

## Validation gates

- [ ] Clean cross configure/build and NRO metadata/link-map inspection.
- [ ] Hardware clear screen, error logging, Joy-Con/Pro hotplug, audio device,
  dock/undock and clean exit; capture driver/package versions.
- [ ] ARM64 CPU/guest numeric comparisons and null-renderer boot.
- [ ] Folder-backed sector reads match source ISO for streaming workloads.
- [ ] World/HUD/menu picture comparisons and interactive ingame route.
- [ ] Audible music/effects, pause/resume, save/load on a test card copy.
- [ ] 25+ FPS gameplay route at recorded internal/output resolutions and stock
  clocks, frame-time distribution and 30-minute soak. No title-screen substitutes.
