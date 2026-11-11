# GTA Vice City Stories NX — developer contract

## Scope and status

Native Nintendo Switch homebrew port of `gothamor/gta-vcs-recomp` (PS2 static
recompilation), using devkitPro/devkitA64 and libnx on Horizon OS. Host work runs
in WSL2 Ubuntu or a Linux Docker container. This workspace initially contained
only a user-provided ISO; the upstream checkout is now in `upstream/`.

Inspected upstream revision: `ecf150e53f87e4656dfe81eacd922839d952b282`.
Check that revision before applying the file checklist to a newer checkout.
Read `upstream/AGENTS.md` before editing anything below `upstream/` and preserve
its documentation, runtime patch markers, numeric accuracy and reference paths.

## Mandatory milestone workflow

Follow [docs/MILESTONES.md](docs/MILESTONES.md) as the authoritative task ledger
and [docs/AUTOMATED_EXECUTION.md](docs/AUTOMATED_EXECUTION.md) as the runbook.
Read [docs/PORT_DISCOVERY.md](docs/PORT_DISCOVERY.md) and its complete scoped
function/asset/portability inventories before implementation. Work on exactly
one milestone branch at a time, created from reviewed main. Never implement or
commit milestone work directly on main.

**Only after ALL tasks in a milestone are complete, evidenced, committed and
validated may a fresh independent reviewer agent be created.** Run
`python tools/milestone_gate.py Mxx` first. Do not create a reviewer for partial
work. The reviewer must independently check the exact candidate; resolve every
finding on the same branch and re-review changes. Record actual independent
approval and pass `--require-review` plus CI before merging into main. Only then
create/push the next milestone branch. Missing hardware keeps required tasks open;
no waivers, invented proof or premature milestone advancement.

The user explicitly authorizes the independent milestone reviewer. This does not
authorize general implementation delegation or additional user-owned chats.
GitHub plan limitations on private branch protection must be recorded honestly;
they do not relax these operating rules.

**Current deliverable is a platform scaffold, not an integrated game port.**
The root build compiles `src/main.c` and `src/core_stub.c`: a GL clear, controller
hotplug and bounded silent audio streaming. It neither generates nor links game
code. The C ABI in `src/core.h` is new port-owned glue, not an upstream API.
Do not describe a successful scaffold build as ingame validation.

## Corrected graphics architecture

The agreed baseline is **SDL2 + OpenGL core 3.3 + EGL/Mesa nouveau + libnx**:

```text
generated EE/VU C++ + PS2Recomp runtime + VCS native HLE
                       |
               platform / RHI interfaces
                /          |           \
  SDL_GameController   OpenGL RHI     PCM producer ring
       Joy-Con/Pro     GLAD -> Mesa   SDL_AudioStream -> SDL2 -> audren
                          EGL
                          libnx NWindow -> display
```

Use `SDL_WINDOW_OPENGL` and `SDL_GL_SwapWindow`. SDL owns the EGL surface/context
and presentation. Do not create another EGL surface, NVN context, Vulkan
swapchain, SDL_Renderer or libnx framebuffer for the same window.

NVN is a separate proprietary API, not a Vulkan driver. Stock switch-sdl2's
Switch video backend supplies EGL/GL hooks, not Vulkan hooks. Linking `vulkan`
does not add SDL Vulkan support. No Vulkan library belongs in this baseline.
An experimental NVK route requires a separately pinned driver AND compatible
SDL backend and hardware feature tests; it is outside the current build.
deko3d is a possible future backend behind the RHI, after measurements justify it.

Upstream currently uses **SDL3**, D3D11 on Windows and Vulkan on Linux; it has
no existing OpenGL RHI to switch on. Preserve its API-neutral platform/RHI seams.
Implement SDL2 and GL adapters rather than introducing graphics calls into game
logic. Upstream's HLSL shaders need GLSL equivalents and visual comparison.

## Folder structure

```text
gtavcs_nx/
  AGENTS.md                      this contract and build instructions
  CMakeLists.txt                 Switch scaffold; official toolchain only
  src/
    main.c                       process, cwd, SDL2/GL/audio/input lifetime
    core.h                       C ABI to a future C++ runtime adapter
    core_stub.c                  explicitly labelled platform-only stub
  third_party/glad/              generated GLAD 2.0.8, GL 3.3 core, no extensions
    include/{glad,KHR}/
    src/gl.c
    README.md                    provenance and regeneration
  upstream/                      pinned public upstream Git submodule
    src/{game,platform,rhi,render,sound,settings,ui,disc,debug}/
    third_party/PS2Recomp/        EE/VU/GS/IOP runtime and host tools
    game/vcs/                    supported executable config/function maps
    tools/                       host recompiler and validation scripts
    generated/                   private output generated from the user's disc
  docs/
    PORTING_CHECKLIST.md         exact upstream files and integration order
    SWITCH_REFERENCES.md         inspected public Switch port references
    BETA_ROADMAP.md              beta definition, execution order, step-by-step route
    MILESTONES.md                complete task/branch/review/merge contract
    PORT_DISCOVERY.md            function/asset/subsystem discovery dossier
    AUTOMATED_EXECUTION.md       resume, validation, review and handoff runbook
    discovery/                  metadata-only complete scoped inventories
    milestones/Mxx/             task evidence, reports and independent review
  tools/                         discovery, foundation and milestone gates
  build/                         ignored build output and host tool venv
  dist/                          ignored staged SD layout, NRO only
  gamefolder/                    optional ignored local asset staging
```

`upstream/` is registered in `.gitmodules` and pinned by the root gitlink.
Clone with `git clone --recurse-submodules` or initialize with
`git submodule update --init`. Maintain port changes in root overlay/patch tooling
against that pin; never assume uncommitted submodule edits are captured by a root
commit. See the discovery dossier for the patch strategy.

## Target filesystem

```text
sdmc:/switch/gtavcs_nx/
  gtavcs.nro
  gamefolder/
    TEXT/ AUDIO/ MODELS/ ...     actual extracted PS2 tree, preserve names
    mods/                       future mod loader root
    userdata/                   future saves/cache; test on separate copies
    gtavcs.log                  scaffold stderr log, overwritten per run
```

Call checked `chdir("sdmc:/switch/gtavcs_nx/gamefolder")` before SDL/core startup,
before creating workers. Missing root is a fatal configuration error; never
fall back to the NRO directory or silently create an empty asset tree.
All game assets and mod lookups resolve relative to this root. `chdir` is not a
sandbox: the future VFS must normalize `cdrom0:`, `\\`, ISO `;1` suffixes and case,
and reject absolute/traversing mod paths. Keep the actual disc layout and filename
case; do not invent flat replacements for archives.

Upstream reads the ISO through sector/LBA APIs. A loose extracted tree alone does
not preserve those semantics. Implement and validate a manifest-backed folder
disc adapter, retaining original LBA/size metadata, before claiming folder-only
boot. Do not substitute a hidden runtime ISO dependency for the specified layout.

## Build prerequisites

Install devkitPro using its official instructions, then in a devkitPro-enabled
Ubuntu/container environment install packages (not Ubuntu SDL/OpenGL packages):

```bash
sudo dkp-pacman -Syu --needed switch-dev switch-cmake switch-pkg-config switch-sdl2 switch-mesa
```

Omit `sudo` inside a root container. Install host CMake >=3.24 and Ninja using
the host package manager. `switch-dev` supplies devkitA64, libnx and Switch tools;
`switch-mesa` supplies GL/EGL and depends on the nouveau support library.
`switch-sdl2` is a package name; the linked library is `SDL2`, not `switch-sdl2`.
GLAD sources are already generated and included; no network fetch at configure.

## Custom build commands (WSL2 Ubuntu)

Prefer a WSL ext4 checkout for generated-code builds; the Windows-mounted
Desktop is usable for this small scaffold but expensive for many generated files.
An Ubuntu WSL distribution must be installed: `docker-desktop` alone is not one.

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH"
cmake -S . -B build/switch -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/switch --parallel "$(nproc)" --target gtavcs_nro
cmake --install build/switch --prefix "$PWD/dist"
```

Outputs: `build/switch/gtavcs.elf`, `gtavcs.map`, `gtavcs.nacp`, `gtavcs.nro`.
Installation stages `dist/switch/gtavcs_nx/gtavcs.nro`. Build before installing.
Asset extraction/copy is separate; install never packages the ISO or assets.
Copy the staged NRO into the exact SD layout above. Launch with full application
memory (title override); applet-mode memory is not a supported game target.

For Docker, run the same commands inside a devkitA64 image with the prerequisites
installed, mounting source as `/work` and using `/work` as the working directory:

```bash
# From WSL, after Docker Desktop's Linux engine/WSL integration is running:
docker run --rm -it -v "$PWD:/work" -w /work devkitpro/devkita64:20260219 bash
```

That dated image is an inspected REDRIVER2 reference, not a validated lock for
this project. Run prerequisite installation inside it. Pin an image digest and
record `dkp-pacman -Q` before calling the pipeline reproducible; package updates
change a dated base image's dependencies. CMake deliberately uses only the
Switch cross pkg-config and static portlibs, not Linux X11/Wayland/ALSA libraries.

The upstream generator remains a **host** tool. First check a disc without
extracting it: `python3 upstream/tools/vcs_disc.py check /path/to/disc.iso`.
The provided ISO was checked successfully as SLUS-21590 USA v1.00 on 2026-11-08.
Generate EE/VU code with upstream's host tooling in a separate host build;
`tools/build.py --recompile` also builds the desktop target, so it is not the
Switch build command. See the checklist before attempting ARM64 core compilation.

## Runtime ownership and developer guidelines

- The main thread owns SDL events, window/context, GL calls and presentation.
  The current SDL backend automatically resizes a resizable window at dock changes;
  always query drawable pixels and rebuild future render targets on size changes.
  1280x720 handheld / 1920x1080 docked are output targets, not measured performance.
  Keep render scale distinct from drawable size and keep HUD aspect correct.
- Snapshot controller state once per event pump; publish a synchronized snapshot
  to the future game thread. Handle hotplug, disconnect-to-neutral, dead zones,
  signed axis extremes, triggers and rumble. Map physical positions:
  south -> PS2 Cross, east -> Circle, west -> Square, north -> Triangle. Test the
  actual SDL mapping for paired Joy-Cons and Pro Controller. Plus -> Start,
  Minus -> Select. The smoke test temporarily uses Plus to exit.
- SDL2 `SDL_AudioStream` converts PCM; an audio device actually sends it to audren.
  SDL owns audren initialization/shutdown. Do not initialize audren a second time.
  The scaffold queues bounded silent S16 stereo 48 kHz blocks on the main thread.
  For the game use a producer ring and independent consumer so render stalls do
  not starve audio; count underruns/latency. Never perform file I/O or game boot
  from an audio callback. Mute/reset queued audio across suspend/resume safely.
- Keep a small C entry point; implement the C++ core adapter behind `core.h`.
  Convert upstream's blocking `main` into explicit boot/run/shutdown ownership.
  Preserve EE scheduler yields and guest-call semantics. Do not call a generated
  function directly and assume it runs to completion.
- Port x86 intrinsics through verified portable/SIMDe/NEON operations at the owning
  layer; no blanket header substitutions or AVX flags on ARM64. Preserve 128-bit
  guest layout, unaligned accesses, saturation, rounding and aliasing behavior.
  Disable FP contraction and preserve the guest/VU rounding rules.
- Upstream currently requires Clang 19+ in CMake, but `ps2_guest_cc.h` already
  defaults to the platform ABI on non-x86. Audit GCC compatibility and generated
  code before deciding whether to patch that gate or use a compatible Clang
  cross setup. Do not claim devkitA64 GCC is already supported by the game.
- Reuse upstream's ImGui context and RHI UI renderer. Replace its SDL3 backend
  with `imgui_impl_sdl2`, enable gamepad navigation and design an overlay chord
  that consumes input without leaking it into gameplay. No ImGui implementation
  or menu is included in the current smoke target.
- Preserve original source licenses and credit; no game ISO, ELF, extracted assets,
  saves, generated game C++ or copyrighted shader dumps in commits/releases.
  Keep generated GLAD notices. Record runtime modifications in upstream's
  `docs/PS2RECOMP_PATCHES.md` and discoveries in `docs/game-internals.md`.
- Keep changes scoped and reviewable. Preserve desktop behavior and upstream's
  reference rendering paths. No speculative speed claims or GPU overclock changes.

## Acceptance direction

The authoritative M00–M15 tasks and their execution order live in MILESTONES.md;
docs/BETA_ROADMAP.md explains the route. Milestone numbers are stable identifiers,
not the order.

Beta (M06, user decision 2026-11-09): the real game opens on a real Switch, goes
ingame and loads the first scene, Fort Baxter gameplay, with the world and HUD
visible and the player controllable for 5 minutes, then exits cleanly. Timing is
measured, with no FPS target. After beta, in order (user decision 2026-11-11):
real sound and movies, dev menu on L3+R3, widescreen, then performance (M10);
custom textures, custom audio, full controllers, saves and release follow M10.

Final acceptance (M10/M11) includes a controllable ingame route with audible
   music/effects, stable saves and docking, 30-minute soak, 720p/1080p output and
   adjustable internal scale. Target 25+ FPS: <=40 ms frame budget on a repeatable
   busy route. Record median/p95 frame time, audio underruns, memory and CPU/GPU
   timing at stock clocks in both modes. Intro/title FPS is not gameplay proof.

Current verification status (2026-11-08): Docker's Linux engine works; only
`docker-desktop` is installed in WSL and no local `C:/devkitPro` is required by
the pinned container pipeline. Cross compilation and source-only CI pass.
An actual Switch reports application mode, GL 4.3 core/NV120, 1280x720 and a
bounded silent audio queue, visible blue clear, HOME/sleep recovery, readable
startup failure and clean process exit. Full paired wireless/Pro controller
and docking acceptance remain open. See M01 evidence; no game code is
linked and no gameplay validation is claimed.
