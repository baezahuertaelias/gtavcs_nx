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
That run did not establish visible clear-screen acceptance or Plus exit. A brighter
blue clear, first-frame pixel readback/error and presented-frame counter were
added for the next run. Its visual acceptance is recorded below from the user's
confirmation and the retrieved shutdown log.

Candidate `a265068fec1737db4f497063610818c61e3d55cd`, NRO SHA-256
`2b3ad6a286f74a872ce126192e155cd0576fd2523fed12bad520392fe4ba66eb`:
the third nxlink run read back center pixel RGBA 31,82,178,255 with GL error
0000 and reported frames 302,603,904,1204,1505,1805,2106 at successive roughly
five-second samples. Audio high water remained 12288 bytes with one initial
empty observation. These counters measure only the scaffold loop; visible-screen
confirmation and orderly exit are recorded below.

The user subsequently confirmed the screen was blue and that Plus closed the
test. FTP retrieved `build/evidence/M01/hardware/bright-plus-exit.log`, SHA-256
`a28d0459d1db7a428814f8fb6149ceeee9cec6a7afcabe8be26b129c053d3bc1`.
It records SDL button bit 00000040 (Start/Plus) then an orderly shutdown marker
after audio/stream/controller/context/window/SDL cleanup, high water 12288 bytes.
This closes M01-07 and M01-09. `aarch64-none-elf-nm -A` on the pinned
`libSDL2.a` shows `SDL_switchaudio.o` referencing audrenInitialize, audrenExit,
audrenStartAudioRenderer and audrenWaitFrame; runtime reported that Switch
audio backend. The program does not initialize audren itself.

User hardware identity: Switch V2, Atmosphere, attached Joy-Cons, handheld with
official charger. Exact Atmosphere and Sphaira versions have not been supplied.
The user confirmed that a Pro Controller and dock/TV are unavailable in this
session; M01-08/M01-10 and complete acceptance remain open without waivers.

The brighter candidate's
[foundation CI](https://github.com/baezahuertaelias/gtavcs_nx/actions/runs/42514795621)
and [Switch CI](https://github.com/baezahuertaelias/gtavcs_nx/actions/runs/42514795760)
passed. Downloaded CI ELF/map/NACP/NRO checksums exactly matched local `/work`
output. Fresh clone, incremental, intentional failed-build rejection and clean
rebuild proof also passed for `a265068fec1737db4f497063610818c61e3d55cd`;
the two clean builds at the proof's temporary source path produced identical NRO
SHA-256 `cde3f668975f46bf6d3a8fb182fe5745460f84b8bf8fdac9d40c481fab020a1e`.

Retrieved SD log hashes: missing-root.log
`12c1cec19c25935f26547a4e783c02a6c2c7b0a80deecf1a37de7f5a0c597f30`;
first-success.log
`75356643aaf0bc37139b79463d9648435dcc5bbab1b24bd52d23ea03e5581176`.

## Joy-Con sweep and lifecycle investigation

The next run used the same brighter NRO. The user confirmed completing an input
sweep, Joy-Con detach/reattach, HOME/return, sleep/wake/return and Plus exit.
Retrieved `build/evidence/M01/hardware/joycon-lifecycle.log` has SHA-256
`bf4824ef9b420b72a150311fe9c1edcb063391dd56172068c5a15cb610be2bd2`.
It contains both sticks, face/d-pad/shoulder bits, ZL/ZR values 0/32767, bounded
audio and orderly cleanup. It contains no removal, disconnected-neutral or
focus/resume markers. Detachment retains wireless connection and cannot prove a
full disconnect. The controller task remains open, also awaiting Pro testing.

The missing focus markers are a real lifecycle evidence gap. The pinned libnx
header describes default SuspendHomeSleep and notification-enabled
SuspendHomeSleepNotify. Its
[owning source](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/services/applet.c)
initializes the default mode, dispatches OnFocusState/OnResume hooks, and defines
the notification mode's different suspend flags. SDL_switchvideo.o also calls
appletMainLoop, so notifications can be consumed during the SDL event pump.
Sampling final focus alone and retaining only resume can miss a focus transition.
The fix requests SuspendHomeSleepNotify for Application mode and retains focus
notifications in the hook for main-thread audio pause/clear/refill and clock
reset. Hooks themselves perform no SDL/GL work. Hardware revalidation is pending;
M01-11 remains unchecked.

Focus candidate `f86079f2e1b3b9295fc31009a39147a9bfe680ac`, NRO SHA-256
`004350e3368d2367d95f2443c355d30c956fb1fae8379f17800038a7928a5dfc`:
the user confirmed HOME/return, sleep/wake/return and Plus all worked. Retrieved
`build/evidence/M01/hardware/focus-notify.log`, SHA-256
`1ddb9fadb6b5e6778971c70702c0efb1d91f15af7998d1c712657edebb4d6bc7`,
records a retained focus notification, audio pause/clear, successful rendering
and bounded refill, then Plus and orderly cleanup. The one notification is
before the first frame; it is not proof of distinct mid-loop loss/gain events.
A deliberately delayed mid-loop sequence is still needed to validate each
transition's reset. User-reported recovery is recorded without overclaiming the
missing per-transition logs. Fresh/incremental/failure/clean proof passed for
this candidate; same-directory clean NRO SHA-256
`442a018bd603aed88a0408cf35133cb677cdba52c77e956da250173817a06ba3`.

The fatal console originally used printf after consoleInit. Inspection of
[libnx consoleInit/con_write](https://github.com/switchbrew/libnx/blob/v4.12.0/nx/source/runtime/devices/console.c)
shows the console device registration, while nxlink may retain ownership of the
already-open stdout descriptor. The fatal-screen implementation now draws text
through the public ConsoleRenderer interface so stdio redirection cannot hide
the on-device diagnostic. It still runs only before SDL init or after all SDL
display ownership has been destroyed. Missing-root visible confirmation remains
pending for this fix.

Still required: visible error confirmation, paired Joy-Con and Pro
mapping/hotplug/disconnect-neutral/reconnect, repeated dock/undock with 1080p,
complete HOME/focus/sleep/resume logs and failure cleanup. Record exact
CFW/Sphaira versions. See HARDWARE.md for the matrix.
M01-12 remains open despite passing cross-build CI because complete hardware logs
and closure of every failure are also required. No game/audio-playback claim.
