# M01 verification — 2026-11-08

M01 is in progress on `milestone/m01-platform-smoke`, based on reviewed main
`22f418a5d495c5c48b8f0d8890bde1eb21607667`. No reviewer has been created, and
hardware tasks remain open. This target links `src/core_stub.c`, not game code.

## Host and toolchain (M01-01 through M01-03)

Observed `docker version`, `docker info`, `wsl --list --verbose` and a disposable
container: Docker Desktop 4.96.3, Linux engine/client 29.9.2, Linux x86_64 kernel
6.18.54.1-microsoft-standard-WSL2, 12 logical CPUs, 32002 MiB memory and 8192 MiB
swap. Only docker-desktop is installed in WSL; an Ubuntu distribution was not
needed because the Linux engine works. No other project/container was changed.

Immutable base:
`devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`.
`package-lock.txt` is the complete observed `dkp-pacman -Q` output, checked during
image creation and every build. It includes devkitA64 r29.2-1, GCC 15.2.0-7,
libnx 4.12.0-1, switch-cmake 1.5.2-1, SDL2 2.28.5-4, Mesa 20.1.0-5, nouveau
1.0.1-2 and switch-pkg-config 0.28-4. Host CMake 3.31.6, Ninja 1.11.1 and Python
3.11.2 are already supplied by the immutable image. No package updates or host
library substitutions are performed. An image/dependency mismatch fails closed.

`python tools/build_switch.py [--clean]` builds the image noninteractively and
runs the build with networking disabled. Docker caches dependencies; Ninja caches
objects in build/switch. `.dockerignore` permits only the Dockerfile and package
lock into the Docker build context, excluding the local disc and generated data.
The runtime bind mount supplies sources; the scaffold build reads no game inputs.

## Build and artifact proof (M01-04 and M01-05)

Implementation candidate `90b02397a6523a4f43ed143168afc1bee471729b`:

- Clean cross build and install passed. The original scaffold failed because
  Switch Mesa has `glapi.pc`, not `gl.pc`; the owning CMake dependency was fixed.
- `tools/inspect_switch.py` passed: ELF64 little-endian AArch64 DYN/PIE, no
  interpreter or NEEDED host libraries, NRO0 header and embedded matching 0x4000
  NACP with smoke metadata. Compiler, includes and all map archive paths are
  restricted to devkitPro/source paths. SDL2/EGL/stub present; no Vulkan/host
  X11/Wayland/ALSA linked. Full relocation/map data is retained privately.
- `docker run --rm --network=none --mount type=bind,source=<repo>,target=/work
  -w /work gtavcs-nx-toolchain:m01 python3 tools/prove_switch_build.py` passed.
  It cloned committed source into an empty temporary directory, initialized the
  exact pinned submodule from the local mirror, built clean, observed Ninja's
  incremental no-op, deliberately inserted `#error` into its disposable stub,
  observed build failure and removal of staged NRO, restored source and rebuilt
  clean. Both clean NRO hashes in that directory were identical:
  `ec8fce5c636a5dbb1d09122cc67ca57cf3530465d569cf969dceecee1f1dfda4`.
- Local `/work` build and a separate GitHub hosted clean cross build produced
  identical ELF, map, NACP and NRO hashes. NRO:
  `921469f35a3220b5e335c71fd8a8dfe54a2c3812170c6f0b322dad2f4bde8493`.
  Reproducibility is established at the prescribed `/work` path; arbitrary build
  paths were not byte-identical and are not claimed to be.
- [Foundation CI](https://github.com/baezahuertaelias/gtavcs_nx/actions/runs/42514549415)
  and [Switch CI](https://github.com/baezahuertaelias/gtavcs_nx/actions/runs/42514549375)
  passed at that exact candidate. The downloaded CI checksums independently
  matched local output. Source-only CI uploads explicit NRO/checksums/ELF report
  and build logs for 14 days, with no game data.

Raw private outputs: `build/m01-build-console.log`,
`build/evidence/M01/{build.log,readelf.txt,checksums.json,pipeline-proof.log,pipeline-proof.json}`,
and `build/evidence/M01/ci-90b0239/`. Subsequent diagnostic changes require their
own build/CI results; these CI links do not approve later commits.

## Hardware observations and outstanding proof (M01-06 through M01-12)

The user supplied a real modded Switch running Sphaira, nxlink endpoint
192.168.1.107, and explicitly authorized FTP at port 5000. No unrelated SD
content was modified. First nxlink run of the above `/work` NRO returned:
HOS `00160500` (22.5.0), libnx 4.12.0-1, applet=0 (application), then a checked
chdir failure for the missing gamefolder. The process printed its fatal console
message and returned. Actual on-screen visibility of that message was not yet
confirmed. FTP retrieval preserved `build/evidence/M01/hardware/missing-root.log`.

At the user's request FTP created only `/sdmc:/switch/gtavcs_nx/gamefolder`.
Second nxlink run logged checked chdir success, SDL video/audio `Switch`/`switch`,
GL `4.3 (Core Profile) Mesa 20.1.0-rc3`, renderer `NV120`, a `Switch Controller`,
audio S16 stereo 48000 Hz/1024 frames and drawable 1280x720 (mode=0). Observed
audio queue high water 12288 bytes; one initial empty observation. The retrieved
SD log continued for approximately 50 seconds without audio error, but has no
orderly-shutdown marker. Private log:
`build/evidence/M01/hardware/first-success.log`.

The user reported a black screen and no crash, then manually closed the app.
This does not establish visible clear-screen acceptance or Plus exit. A brighter
blue clear, first-frame pixel readback/error and presented-frame counter were
added for the next run. Visual presentation remains an open failure until
observed working; do not infer a pass from GL version or drawable size.

Still required: visible clear, persistent/visible error confirmation, paired
Joy-Con and Pro mapping/hotplug/disconnect-neutral/reconnect, orderly Plus exit,
audren/device close, repeated dock/undock with 1080p, HOME/focus/sleep/resume and
failure cleanup. Record model/CFW/Sphaira versions. See HARDWARE.md for the matrix.
M01-12 remains open despite passing cross-build CI because complete hardware logs
and closure of every failure are also required. No game/audio-playback claim.
