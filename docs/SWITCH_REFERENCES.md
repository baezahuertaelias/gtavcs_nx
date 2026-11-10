# Public Switch port references inspected on 2026-11-08

Discovery index: [robzilla10001/SwitchPorts](https://github.com/robzilla10001/SwitchPorts),
HEAD observed `2332cf80370a9528bda4d2bac27f5088795b0ccf`.
Use it to find source; inclusion/status badges are not independent runtime proof.

## Closest project lineage: RampageRE

[jjmhalew/RampageRE](https://github.com/jjmhalew/RampageRE), `main`, also has a
PS2Recomp reference implementation and a separate native reimplementation.

- [`build_switch.sh`](https://github.com/jjmhalew/RampageRE/blob/main/build_switch.sh):
  inspected devkitA64 compile, SDL2/GLES/EGL/Mesa/libnx dependency chain and NRO
  packaging. Crucially, it builds the native `src/` program, not the PS2Recomp
  reference runtime. It does not demonstrate ARM64 compatibility of VCS's core.
- [`src/plat_sdl.c`](https://github.com/jjmhalew/RampageRE/blob/main/src/plat_sdl.c):
  inspected Switch cwd/log setup, applet-memory note, SDL GL context and drawable
  handling. Uses GLES, so adapt lifecycle patterns rather than its fixed-function
  shim or GL context profile. Our baseline remains GL 3.3 core.
- [`src/pad_sdl.c`](https://github.com/jjmhalew/RampageRE/blob/main/src/pad_sdl.c):
  inspected SDL_GameController opening, disconnect checks, Switch identification
  and asymmetric signed-axis normalization. A good focused controller reference.
- [`README.md`](https://github.com/jjmhalew/RampageRE/blob/main/README.md): explicitly
  distinguishes the recompiled reference from the native port and says Switch
  real-hardware testing has not been performed. Preserve that qualification.

Start with Rampage's small platform/input layer and REDRIVER2's packaging;
neither establishes a drop-in PS2Recomp ARM64 implementation.

## Best immediate reference: REDRIVER2-switch

[jamyaro/REDRIVER2-switch](https://github.com/jamyaro/REDRIVER2-switch), `master`.
Inspected source:

- [`switch/Dockerfile`](https://github.com/jamyaro/REDRIVER2-switch/blob/master/switch/Dockerfile):
  dated devkitA64 image, Linux host tools, devkitPro paths.
- [`switch/build.sh`](https://github.com/jamyaro/REDRIVER2-switch/blob/master/switch/build.sh):
  explicit aarch64-none-elf tool selection, prerequisite/error checks, NACP and NRO
  packaging. Reuse the architecture, while our CMake uses official packaging helpers.
- [`src_rebuild/premake5.lua`](https://github.com/jamyaro/REDRIVER2-switch/blob/master/src_rebuild/premake5.lua):
  separate Switch configuration, ARMv8/Cortex-A57/PIE/TLS flags and portlib paths.
- [`src_rebuild/premake5_psycross.lua`](https://github.com/jamyaro/REDRIVER2-switch/blob/master/src_rebuild/premake5_psycross.lua):
  platform-specific dependency selection; notably it removes its bundled glad.c
  for Switch. Our SDL-resolved generated GLAD target is a deliberate different
  choice, not a file copied from this port.

It is a PS1 reconstruction using PsyCross, not PS2Recomp. Its packaging/platform
patterns apply; its renderer/audio and performance cannot establish VCS behavior.
The listed PsyCross directory is an external dependency: inspect its pinned source
before drawing conclusions about implementation details inside it.

## ARM64 and future low-overhead renderer: Jak Project Switch

[fildicio/jak-project-switch](https://github.com/fildicio/jak-project-switch),
default branch observed `deko3d-migration`.

- [`cmake/toolchains/Switch.cmake`](https://github.com/fildicio/jak-project-switch/blob/deko3d-migration/cmake/toolchains/Switch.cmake):
  useful notes on ARM64 CRC flags, signed-char assumptions, PIE relocations and
  cross dependency isolation. Audit the corresponding VCS assumptions rather
  than copying all flags. Keep the official toolchain for our build.
- [`cmake/Deko3dShaders.cmake`](https://github.com/fildicio/jak-project-switch/blob/deko3d-migration/cmake/Deko3dShaders.cmake),
  `DEKO3D_MIGRATION_PLAN.md`, `docs/deko3d/` and `common/arm64/` were located in
  its tree for later focused inspection. Their implementation has not been
  audited here; they are follow-up pointers, not adopted code.
- [`README.md`](https://github.com/fildicio/jak-project-switch/blob/deko3d-migration/README.md)
  describes an ARM64 OpenGOAL runtime and deko3d migration. Its speed claims were
  not reproduced here. OpenGOAL is a different recompiler/runtime, not a drop-in
  ARM64 PS2Recomp backend.

Useful when investigating numeric/ABI portability or replacing measured Mesa
overhead later. Do not expand the initial SDL2/GL bring-up into deko3d migration.

## Smaller CMake dependency reference: Fallout CE Switch

[`ryandeering/fallout-ce-switch/CMakeLists.txt`](https://github.com/ryandeering/fallout-ce-switch/blob/main/CMakeLists.txt)
explicitly links Switch SDL2, EGL, Mesa support and libnx, then creates NACP/NRO.
It confirms the general dependency family; it uses GLESv2, not our desktop GL
3.3 core profile. It also hardcodes toolchain paths/global flags. Prefer our
official Switch.cmake and cross pkg-config rather than transplanting that build.
Its 2D renderer is not a VCS world-renderer reference.

## Additional candidates from the index

- [NaGaa95/gtalcs_nx](https://github.com/NaGaa95/gtalcs_nx): listed under Android
  ports. Similar franchise assets/controls do not make its binary adaptation a
  PS2Recomp runtime. Source was not audited in this pass.
- [antoxa2584x/burnout3-sw](https://github.com/antoxa2584x/burnout3-sw): listed
  under Xbox ports. Consider host-platform patterns only after source inspection;
  Xbox guest/GPU/APU code is not a PS2 EE/GS/SPU2 implementation.

## Owning platform sources used for architecture

- [devkitPro SDL2 Switch video](https://github.com/devkitPro/SDL/blob/switch-sdl-2.28/src/video/switch/SDL_switchvideo.c):
  EGL-backed single window and automatic handheld/docked dimensions when resizable.
- [devkitPro SDL2 Switch audio](https://github.com/devkitPro/SDL/blob/switch-sdl-2.28/src/audio/switch/SDL_switchaudio.c):
  audren-backed audio output. SDL_AudioStream itself is not a hardware output API.
- [devkitPro Switch CMake helpers](https://github.com/devkitPro/pacman-packages/blob/master/cmake/switch/NintendoSwitch.cmake):
  official platform flags and `nx_generate_nacp` / `nx_create_nro` signatures.
- [devkitPro switch-sdl2 package](https://github.com/devkitPro/pacman-packages/blob/master/switch/SDL2/PKGBUILD):
  packaged SDL2 depends on switch-mesa and libnx.
- [jhuz0/nvk-switch](https://github.com/jhuz0/nvk-switch): experimental public
  Vulkan option exists, but needs explicit integration and Maxwell capability
  validation; linking it does not retrofit stock SDL2's video backend.

No external port source was copied into the runtime in this pass. Review licenses
and preserve attribution before any later reuse. None of these references proves
that VCS reaches gameplay or 25 FPS on Switch.

## Focused M02 follow-up (2026-11-08)

The user asked whether this list helps the ARM64 milestone. Rechecked source:
Jak's toolchain at observed branch commit
`e331185682aee0c51a58fcf9a877d942b71f531d` documents signed-char sentinels,
CRC flags and PIE relocation hygiene; RampageRE's build script at observed
`115ae7a0c6e967f4ce41a5685b08757774083c6e` also explicitly uses signed char
and ARMv8/PIE flags. These are concrete audit prompts for M02-04/M02-09, not
proof that VCS has the same bugs. Our pinned devkitA64 preprocessing independently
confirmed unsigned default char, 64-bit pointers/long and little endian.

The compile-only CPU foundation preserves the reference's signed-char behavior
and uses the official Switch toolchain/PIE flags, rather than replacing it with
another project's toolchain. Rampage's script explicitly builds its native
viewer, not its PS2Recomp reference, so it cannot substitute for VCS EE/VU
numeric/ABI tests. No outside runtime implementation was copied for this step.
