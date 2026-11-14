# M04 execution checklist

Branch `milestone/m04-runtime-boot`. Parent task status lives in [docs/MILESTONES.md](../../MILESTONES.md). A step
here is ticked only when its proof exists: host and desktop results in [VERIFICATION.md](VERIFICATION.md), Switch
results in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md) (candidate NRO `96091d30...`, built from clean `9a1c79e`; runtime
tests `e908526f...`). The session procedure is [HARDWARE.md](HARDWARE.md).

## M04-01 — Explicit boot / run / stop behind the C ABI

- [x] Overlay 0007 splits upstream's blocking `run()` into `runBegin` / `runFrame` / `runEnd`; `src/core/core.cpp`
  implements `gtavcs_core_boot` / `_frame` / `_shutdown` of `src/core.h` on them, and `src/main.c` keeps the loop.
- [x] `core_stub.c` is not linked (`core_stub_linked=false` in the candidate's link map).
- [x] One SDL window and GL context, made by `main.c` and adopted by the platform layer (`adopted main.c's window` in
  every Switch log).

## M04-02 — `platform.h` on SDL2

- [x] All 42 functions declared in upstream's `platform.h` are defined in `src/platform/platform_switch_sdl2.cpp`.
- [x] Desktop-only operations are accepted with no effect and say so (`mouse capture: not on the Switch (accepted, no
  effect)`).

## M04-03 — SDL3 interfaces on SDL2

- [x] Events, timing, controller (`SDL_GameController`, the Switch mapping logged), audio (48 kHz stereo device,
  pause / resume across focus) on SDL2 2.28.5.
- [x] No SDL3 symbol in the link map (only `libSDL2.a`); no SDL3 in the game build's compile commands.

## M04-04 — Runtime initialization order

- [x] Upstream `main()`'s order: early `IoPaths` (card under userdata), UI callbacks, `initialize`, `loadELF` (which
  applies the game override registry), final `IoPaths` with no disc image, `installHooks`, self-test, `runBegin`.
- [x] The mapped `FolderDisc` lives from before `initialize` until after the runtime is destroyed.
- [x] Generated-function registration: the generated archive linked whole-archive with its registration constructor;
  every self-registering archive member linked (`unlinked_constructors=[]`, the generic check in `build_game.py`).

## M04-05 — Hooks and UI callbacks

- [x] The game layer (`applyVcs`: movie, stream, sound, display, HUD, frame-rate, script and physics timing, menu,
  aim and camera, RSL hooks) installs through the override registry at `loadELF`; `launcher::installHooks` then adds
  the rig, trainer, input, volume and game-info hooks and the `setting` / `menu` commands; the UI callbacks are set
  before `initialize`. The same `[menu]`, `[sound]`, `[iop]`, `[input]`, `[movie]` lines appear on the Switch and in
  the reference.
- [x] `gameinfo`, `soundstate`, `menustate`, `setting`, `player`, `frames` answer over the run schedule; nothing needs
  a desktop hotkey (the schedule drives them; the pad's buttons stay the game's).

## M04-06 — Host waits, workers and the event owner

- [x] The main thread owns SDL events and the GL context; the game thread and render worker are the core's own
  (`threads_created=6` in every game run that booted, `live_threads` 2 / 4 / 2 at boot / running / shutdown).
- [x] Guest yields resume: the boot reaches gameplay state on both launches, with no `unlisted-resume` line.
- [x] No main / game thread deadlock in any run of the session: three boots, files, two saves, the branch trace, HOME,
  sleep and close.

## M04-07 — Desktop OS facilities on the boot path

- [x] `boot_os_audit.json`: every `desktop_os` / `thread_time` candidate compiled into the NRO ruled, `unresolved: 0`,
  20 `std::thread` sites with owner and end; unchanged at the candidate.
- [x] The desktop watchdog and sampling profiler are not started on the Switch, and the log says so.

## M04-08 — Orderly cleanup

- [x] No `std::_Exit` on the stop path (upstream's `main()` is not built); the core joins the game thread and stops
  the render worker and platform, then the stream reader, then clears `ps2_disc::setImage`, destroys the runtime and
  closes the folder disc, in that order, in every Switch run's log.
- [x] The stream reader (`vcs_stream.cpp`) is stopped and joined (overlay 0007); `sceCdBreak` / `sceCdStop` are no-ops
  in both runtimes and the emulated IOP is off (as on the desktop), so no CD request is outstanding at the stop.

## M04-09 — Real boot progress

- [x] Asset reads from the folder disc (`file_opens` 145 per boot, `iso_opens=0`), the game's own start-up and main
  loop (guest pcs in the main loop functions, `game_frames` rising from 0).
- [x] Native sound driver and the retired emulated IOP, as on the desktop.
- [x] Front-end and game state progress: the player ped from 5 s, outdoors with a follow camera, the in-game clock
  advancing, `menustate ok open=0`.

## M04-10 — Lifecycle

- [x] Stop by time: `stop reason=seconds` and the clean shutdown line in every timed run.
- [x] Startup failures: the bad configuration names `nxrun.cfg:3` and is shown in the system error dialog, with no
  crash; a failure before SDL keeps M01's console screen.
- [x] HOME and sleep: suspensions detected, focus pairs logged, progress resumes.
- [x] Close from HOME: the exit request reaches the orderly shutdown.
- [x] Repeated launches: two full boots and two saves on the same test cards, the same thread counts, the card tree
  unchanged by the repeated boot; no new crash report in any run.

## M04-11 — Diagnostic and control commands

- [x] `nxrun.cfg` schedules: bounded (lines, line length, commands, env lines, seconds), env limited to `VCS_` /
  `PS2X_` and never the control socket, hidden mode or data folder; refusals name the line (host tests, bad
  configuration on the Switch).
- [x] Results are one machine-readable line per event, with `status=ok|error|timeout|unanswered`; an unknown setting
  and an unknown command on the Switch answer `status=error reply="error ..."` and the run carries on.
- [x] User-save isolation: a schedule must name a `profile` (refused otherwise, host tests); every scripted run's card,
  settings and results go to `userdata/tests/<profile>`, never the player's own userdata.

## M04-12 — Reference comparison and unhandled instructions

- [x] The desktop reference (upstream's own build of the pin, null RHI) and both Switch boots reach outdoors with a
  follow camera on the same route, 0 errors each (`m04_compare.py`).
- [x] No runtime exception, unlisted resume or unsupported syscall in any final log.
- [x] M02's 561 unhandled-instruction events: all compile to a throw; none executed in the nine final Switch logs or
  the reference (`unhandled_classification.json`).
- [x] M02's indirect-fallback owners (1,196 warnings = 598 per-owner promotions, each reported twice; 1,370 indirect
  sites in them, all compiled to a runtime dispatch): on the Switch branch trace of the boot route plus a save, 148
  owners ran an indirect dispatch, every target registered, and 450 were not reached on that route
  (`indirect_classification.json`).
- [x] Generator entry-list completeness for the running game: no missing-target or unlisted-resume report in any final
  Switch log or the reference; the trace saw 234 indirect sites and 12,241,707 dispatches with no missing target.

## M04-13 — Folder disc and game paths only

- [x] No ISO discovery or staging: `cdImage=(none)`, `no disc image opened`, `iso_opens=0`; the ELF read from
  `gamefolder/SLUS_215.90`; preflight finds no disc image on the card.
- [x] The ELF's SHA-1 checked against `kElfSha1` at run time, naming the path on a mismatch.
- [x] `FolderDisc::verifyFiles` as a run option: `verify problems=0` on the Switch.
- [x] Settings, cache, both card folders and the card backups under `gamefolder/userdata` through `GamePaths`, test
  runs under `userdata/tests/<profile>`: the game's own save to the first card wrote its save folder, the card's and
  the folder's `.ps2x-order` files and a backup there, and each later save another backup beside the existing ones.
  The second card's folder and the cache were created and never written (the game saves to the first card).
- [x] Checked removal instead of `remove_all` (card format / unformat of a card holding saves) and replacement instead
  of rename-onto-existing (a second and third settings save, card rename onto an existing name, `replaceFile` twice),
  on the Switch.
