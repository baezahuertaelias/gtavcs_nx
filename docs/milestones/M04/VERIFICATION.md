# M04 verification

Host, build and desktop results, 2026-11-13 and 2026-11-14, on a Linux host (Debian, Docker engine 29.7.2,
4 cores). Hardware results are in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md). No game data is reproduced here;
private logs stay in `build/evidence/M04/`.

## Private generation on this host

The private EE/VU code was regenerated from the user's disc (`GTA Vice City Stories Esp.iso`, SHA-256
`981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740`, the image M03 extracted to the SD card;
`upstream/tools/vcs_disc.py check`: SLUS-21590 USA v1.00) with upstream's unchanged `tools/recompile.py`, run in an
overlaid checkout of the pin (`tools/upstream_overlay.py`, all 7 patches; patches 0002/0003 change the recompiler
and the VU generator). It matches M02's generation exactly: recompile stamp inputs `9dc29d7b...`, source
`ecf150e5...`, 10,386 EE files, 1,196 warnings and 561 errors, VU1 50 programs / 3,032 pairs / 110,463 lines.

`cmake/core` compiled it in the locked container into the `gtavcs-nx-m02-core` volume: 328/328 steps, archive
1,768,209,486 bytes, SHA-256 `6c44a0bd5dc6233e0b149ec13377ca66dde3eb5f75d51ae53ef8d8066fe1d317`. The archive differs
from M02's 1,768,078,598 bytes (build paths differ); the generated sources are the same.

## Builds of the candidate

| Command | Result |
|---|---|
| `python tools/build_game.py --public` (source CI) | `PASS public core library compiled; no game code linked, no NRO`; `nx_core` and the new `nx_self_registering` both compiled |
| `python tools/build_game.py` at clean `d68d19a` and again at clean `8e8f6b3` | `PASS private game NRO 15b07baefe52db8f4fe1b3c7ed354d860373e058e3e9c99037ada7b5989813c7` both times (`root_dirty=false`) |
| `python tools/build_core_tests.py` at clean `8e8f6b3` | PASS; overlay applied, synthetic VU fixture reproduced; `gtavcs_runtime_tests.nro` `4172518f...c8951` |

The private build's own checks (`tools/build_game.py`, inside the container) on the candidate's link map:
`core_stub_linked=false`, `generated_linked=true`, `wrapped_thread_create=true`, `game_overrides_linked=true`,
`vu0_natives_linked=true`, `unlinked_constructors=[]`. The last lists every member of `libnx_core.a`,
`libnx_platform.a` and `iop/libps2_iop.a` that has a static constructor (`.init_array`) and is missing from the link
map; the only documented exception is `games_database.o` (a name table for PS2Recomp's generic runner `main()`, which
the port does not build; nothing references it). Against an empty link map the check lists all 47 such members, so it
would have caught both objects the first Switch sessions lost (`vcs_overrides.o`, `vcs_vu0_native.o`).

## Platform layer and SDL

- `upstream/src/platform/platform.h` declares 42 functions; every one is defined in
  `src/platform/platform_switch_sdl2.cpp` (a script over the two files).
- The candidate's link map holds `libSDL2.a` only (3,616 references) and no SDL3 symbol; no compile command of the
  game build names SDL3.
- `main.c` creates the one window and GL context; the platform layer adopts them
  (`[platform] SDL2 2.28.5 adopted main.c's window` in every Switch log).

## Host tests at the candidate

| Command | Result |
|---|---|
| `python tools/run_core_host_tests.py build/evidence/M04/host-tests` | `run_config` and `host_fs_logic`, sanitized and optimized: PASS |
| `python -m unittest discover -s tools -p 'test_*.py'` (as CI) | 28 tests OK |
| `python tools/validate_project.py` | PASS: 179 unique tasks, 16 milestones, inventories, paths, tracked-artifact policy |

## Audits

- `python tools/audit_boot_os.py build/m04/private/compile_commands.json docs/milestones/M04/boot_os_audit.json` on
  the candidate's compile commands: unchanged from the committed audit; every `desktop_os` / `thread_time` candidate
  compiled into the NRO has its ruling, `unresolved: 0`, 20 `std::thread` sites with owner and end.
- `python tools/classify_unhandled.py build/m04/gen-source/generated/ee docs/milestones/M04/unhandled_classification.json`
  with the final Switch logs (both boots, files, focus, close) and the desktop reference: all 561 of M02's
  unhandled-instruction events in 43 owners compile to a throw (`without_throw: 0`; 499 SPECIAL, 45 COP2, 15 COP0
  CO-OP, 2 COP0 format); none executed in any of the six logs (`executed_per_log` all 0). An executed site ends the
  game thread with its address in the log; every run reached its own end.

## Desktop reference

Upstream's own Linux build of the pristine pin (`build/m02/reference`, a clone of the submodule at `ecf150e5`):
`python build/m02/reference/tools/build.py --recompile <the disc> --quick --verbose` built `build/vcs` (898 steps).
`tools/m04_reference.py` (fixed in `dfc1eb4` to find `build/vcs` as well as `vcs.exe`) ran
`tests/switch/m04-boot.cfg` on it with `VCS_RHI=null` and SDL's `offscreen` video driver (this host has no display),
with its own empty card: `stop reason=seconds`, 0 errors, 237 result lines. It runs the same game layer, including
the 8 native VU0 programs, at about 60 game fps.

`tools/m04_compare.py` against both final Switch boots: both reach outdoors with a follow camera (reference 15 s /
15 s, Switch 25 s / 45 s), the same area changes and from 25 s the same player position; 0 errors on either side.
The Switch is slower, so it reaches each state later; no timing target applies to M04.

The `m04-save` schedule ran on the same reference first: `[mc-test] save done`, the save folder, both `.ps2x-order`
files and an `auto-*.zip` backup in the run's own data folder.

## Lifecycle code review points

- Boot order (`src/core/core.cpp`): checked `chdir` in `main.c`; `GamePaths` from the gamefolder; the ELF's SHA-1
  against upstream's `kElfSha1`, failing with the path; `FolderDisc::load` (and `verifyFiles` when the run asks);
  `ps2_disc::setImage(disc)`; early `IoPaths` with the card under userdata; `initialize`; `loadELF`; the final
  `IoPaths` (no disc image path).
- Shutdown order: game thread joined and render worker / platform stopped (`runEnd`), unanswered commands reported,
  `vcs::stopStreamReader()`, `ps2_disc::setImage(nullptr)`, runtime destroyed, folder disc closed, then SDL in
  `main.c`. Upstream's `main()` and its `std::_Exit` are not built (`VCS_LAUNCHER_NO_MAIN`); the `_Exit` calls left
  in the NRO are the crash handlers' fatal path (`installCrashHandlers`: terminate / abort messages, then exit) and
  the `VCS_SOUND_REPLAY` debug mode, neither on the run or stop path.
- `main.c` holds `appletLockExit` from just after the application-mode check until the very end, so a close from HOME
  arrives as an exit request (`SDL_QUIT` from switch-sdl2's `appletMainLoop`) and runs the orderly shutdown.
- A startup failure before SDL starts uses the libnx console text screen (M01); after SDL has used the display it uses
  the system error dialog (`errorApplicationCreate/Show`): the console cannot take the display back from Mesa EGL
  (2345-0021, [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md) attempt 4).
- File replacement and removal on the SD card go through overlay 0007's `ps2x::replaceFile` / `ps2x::removeTree`:
  settings (`settings.cpp`), card backups and card renames (`MemoryCard.cpp`), card format / unformat.
