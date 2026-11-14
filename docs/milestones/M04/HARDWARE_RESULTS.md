# M04 real-Switch results

All runs on 2026-11-14, on the user's Switch (HOS 22.5.0, `hosversionGet` = `00160500`, Atmosphère, libnx
4.12.0-1), launched through Sphaira nxlink in application mode (`applet=0`, title override), docked (1920x1080
drawable), driven by `tools/switch_m04_session.py` as in [HARDWARE.md](HARDWARE.md). No emulator result is used.
Records stay outside Git in `build/evidence/M04/hw-session/` (the game log names guest addresses);
`transcript.txt` there holds every step with its hash, the SD read-back of each payload and the crash-report
listings before and after each run.

Rendering is the null RHI until M05: the screen stays black while the game runs. The game's own sound plays
(native sound driver). No gameplay, picture or frame-rate claim is made.

## Payloads

| Payload | Built from | SHA-256 (local = SD read-back in every run) | Bytes |
|---|---|---|---:|
| `gtavcs.nro` (private game) | `tools/build_game.py`, clean `9a1c79e` (manifest `root_dirty=false`) | `96091d30b06b3bf4e3345ed24e480231a3b724f8c4ecbccf3d7603903bad9361` | 53,532,869 |
| `gtavcs_runtime_tests.nro` (public) | `tools/build_core_tests.py`, clean `9a1c79e` (bundle manifest `root_dirty=false`) | `e908526fbd7647879e4aacdc512ebe4655c82223c602aab4b9a22574cd571d23` | |

The game NRO's manifest: generated EE/VU archive `6c44a0bd5dc6233e0b149ec13377ca66dde3eb5f75d51ae53ef8d8066fe1d317`
linked whole-archive, `core_stub_linked=false`, `game_overrides_linked=true`, `vu0_natives_linked=true`,
`unlinked_constructors=[]`, thread-create wrap and the dispatch wrap (`src/switch/branch_trace.cpp`) present. The NRO
was rebuilt byte-identical after the session.

## Final session (candidate `96091d30...`)

| Run | Started | Log SHA-256 (`gtavcs.log` / results) | Observed |
|---|---|---|---|
| `m04-focus` | 14:43:14 | `b826911a584b4addc1a54247b819ed5a709eb6c7c0231b4cbd9b98eb746a6f5c` / `bdda99850b96cf87e3861e914fbfd70982bac731082f49df25e1863d0a78eb1b` | HOME: `confirmed suspension_ns=10125384687`, `focus t=41.834 state=out/in`; sleep: `suspension_ns=16225513281`, pairs at `71.372` / `71.385`; `game_frames` 1126 -> 1263 and 2006 -> 2102 across them; clean end |
| `m04-close` | 14:46:57 | `57f38399765ec8709339ff8dddd1c6f9eb9abee9702b5bb10ad651c2c45380f3` / `49094a5ab5eee0e74db34f7345a58b7af6b3f3f4cb9caf808e7071f837656817` | Closed from HOME: `[platform] quit requested (applet exit request)`, `orderly stop after 7834 frames`, `game thread joined, render worker and platform stopped`, `[stream] reader stopped`, `runtime destroyed`, `folder disc closed`, `[shutdown] core, context, window and SDL closed`; no new crash report |
| `m04-badcfg` | 14:49:49 | `47b28ae0fdeed00e81d4587bb1ec1746baf29e68bbe153cd2d8f20b64e11116b` | `[core] nxrun.cfg:3: unknown directive 'bogus'`, `[boot] core boot failed (exit code 1)`, `failure shown in the system error dialog (00000000)`; the user read the dialog's details naming `nxrun.cfg:3: unknown directive 'bogus'` and pressed OK; FTP back after 15 s; no new crash report |
| preflight | 14:50:36 | | No disc image under `/switch/gtavcs_nx`; folder-disc manifest and `SLUS_215.90` present |
| runtime suites | 14:50:37 | `runtime-tests-M02.log` `f13918d6e40a1be4eecb016b98c163bf5df37a570ade955e92763fa0ff8a3884` | Upstream suites `failures=0`; `[host-fs] failures=0`, 13 checks incl. card rename onto an existing name, card format and unformat of a card holding saves, `replaceFile` onto an existing target twice, a backup before a change and a second backup beside the first (`backups=2`); no new crash report. The suite's TMPDIR leftovers were removed afterwards (2 files, 61 folders) |
| `m04-boot` | 14:51:10 | `503547d5ebda381db40d7ed1a64658b80e8a6120f95ab39850cc59228aee5b84` / `f045818e4143287c6b0211b37902d3c45c118bace2399aa426129d7e9ab96602` | Interrupted from the console at 155-167 s (the user turning auto-sleep off): HOME suspensions, then `quit requested (applet exit request)` and the orderly stop (`reason=runtime`). Not used as a boot; `m04-boot-3` repeats it |
| `m04-boot-2` | 14:55:33 | `2631f7a712cbcca418e72868692343941b27fd351c305162ca23876ff723ced2` / `443314753f0b0b54e8101dc9c9872d47cfdaa9a05e236ffe705627d1c414bed0` | Below |
| `m04-boot-3`, same test card | 15:12:32 | `a2e2a965407ea5de17df3aab31046ce2c74729d46f15719cf1a577c74e3c8a0d` / `e6acadaf43776be886f9755741ae72d77f19684c2a245266b3645858138750d7` | Same lines as `m04-boot-2`; the test card's tree before and after it is identical |
| `m04-files` | 14:59:54 | `09b154d8463f2fc1ddd91c22bc7d3314c8eea489e6a21959be7e9393c60919bf` / `d6d1664f829f22bd6aad8dd736fa8a15fa10e977434bd0b91366c79d84fc9e44` | `verify problems=0` (whole disc tree hashed against the manifest); `setting display.fps 30` -> `ok 30`, `60` -> `ok 60`; `vcs.ini` (`8a084bdf...`) `display.fps=60`, `audio.volume=90` after three saves, each after the first replacing the file; `setting m04.no.such.setting` -> `status=error reply="error unknown setting m04.no.such.setting"`, `m04-no-such-command` -> `status=error reply="error unknown command ..."`, the run carrying on to `stop reason=seconds` |
| `m04-save`, twice | 15:02:26, 15:05:07 | `d78d2d693d9e57b321bebee121b3066e15d5292fcfb47fe2a4341c4fed3387a2` / `f260f2239ebe9df765272f6afb4e3693a6070d71d564ab2f2df87e385eb9e982`; `ad470890f36a46766dd00a9833011a583c4e572be13cc93d6aca64083f40701e` / `749b184213370bcd7089009c096b47c20e1a103633c325c4a8d3fd047026a5ab` | `[mc-test] save done` in both (the game's own memory card code). The `m04-save` test card still held attempt 6's save, so both saves changed a card with a save on it: each wrote a backup beside the existing ones (`auto-20261114-150941.zip`, `auto-20261114-151221.zip`, 6 files each, after attempt 6's `142001` and `142240`) and rewrote the slot. Under `userdata/tests/m04-save`: the save folder, `mc0/.ps2x-order` and the save folder's `.ps2x-order`, `backups/`. On attempt 6's NRO the first save created the folder and the order files on an empty card |
| `m04-branches` | 15:07:49 | `d9375a6dfc29eff053c3d12215e9cda72956bb9db667093e0533188559e03ff7` / `48011f75f40ac5c0731eaf1c2112e83b6b7d68b96ab2f20c0fd48964ea6cbb0e` | `PS2X_FAST_DISPATCH=0`, `VCS_NX_BRANCH_TRACE=1`, the boot route plus a save: `[branch-trace] indirect sites=234 dispatches=12241707 missing=0`; see `indirect_classification.json` |

Every game run that booted ended with the same tail: `stop reason=seconds` (or the exit request), `iso_opens=0`,
`first_iso=""`, `threads_created=6`, `[resources] shutdown live_threads=2` (boot 2, running 4, via the loader's
process handle), `[shutdown] core, context, window and SDL closed`, the NRO's SD read-back equal to the local hash and
identical crash-report listings before and after.

### The boot (`m04-boot-2`, `m04-boot-3`)

- `[boot] checked chdir succeeded: sdmc:/switch/gtavcs_nx/gamefolder`; `SDL Switch; audio switch`;
  `GL 4.3 (Core Profile) Mesa 20.1.0-rc3; renderer NV120`.
- `[core] /switch/gtavcs_nx/gamefolder/SLUS_215.90 sha1 d1832508caf0990c2291c0f255c96a889cce9f88` (upstream's
  `kElfSha1`); `folder disc 1066336 sectors, 0 mods applied; no disc image opened`;
  `io elf=... cd=/switch/gtavcs_nx/gamefolder mc=.../userdata/tests/m04-boot/mc0 cdImage=(none)`.
- `[platform] SDL2 2.28.5 adopted main.c's window 1920x1080`: one window, one GL context; null presenter.
- Game layer installed (upstream's `applyVcs`): `[sound] native sound driver`, `[iop] emulated IOP off`,
  `[menu] pause menu ...`, `[input] player bindings`, both movies `skipped (Display > Intro movies)` through the
  movie hook, 8 `[vu0native]` programs, compiled VU1 programs.
- Progress: `game_frames` rising every 5 s from 0; the player ped from 5 s; `gameinfo` outdoors (area 0) with
  `CAMERA mode 4 follow ped`, the clock advancing; `menustate` `ok open=0`; `soundstate` `ok music=80 effects=112`.
- No `unlisted-resume`, `missing-target`, exception, unsupported-syscall, dormant-thread or `Unhandled` line in any
  final log.

Measured, not a target: median `game_fps` over the last 20 report lines 27.7 (`m04-boot-2`) and 28.8 (`m04-boot-3`),
`game_ms` about 30. The previous candidate measured 46-48 (`game_ms` about 20) earlier the same day. An A/B run of
this candidate built without the dispatch wrap (`890f06416f809c5c9464fa4e05f3f7766a85ea98c0d8668f22f73cb9e86eb8c6`,
`hw-diag-nowrap`, 15:18) measured the same 25-32 game fps right after, so the drop is not from the candidate's
changes; the console ran slower later in the day. M10 owns performance.

### Desktop reference comparison

`tools/m04_compare.py build/evidence/M04/reference/reference-results.txt <run>-results.txt`:

| | Reference | `m04-boot-2` / `m04-boot-3` |
|---|---:|---:|
| first outdoors (s) | 15 | 25 / 25 |
| first follow camera (s) | 15 | 45 / 45 |
| errors | 0 | 0 / 0 |
| stop | seconds | seconds |

`both reached outdoors with a follow camera: True`. From 25 s on both sit at the same player position
(-1614.2, -132.3, 14.8) in area 0. The reference is upstream's own Linux build of the pristine pin, null RHI, SDL
offscreen video driver, same schedule ([VERIFICATION.md](VERIFICATION.md)).

## Earlier attempts (each superseded by a fix and a full rerun)

Kept in `build/evidence/M04/`; none is evidence for the candidate.

| Folder | NRO | What it showed | Fix |
|---|---|---|---|
| `hw-attempt1` | `3dd30fc1` | `game_frames=0` for 240 s: the guest waited in the game's own PS2 movie player (`FUN_0040D930`) for a logo movie that never decodes; `menustate` unknown; `live_threads=-1` (`svcGetThreadList` 0xE401) | `bb8b622` (`vcs_overrides.o` dropped by the linker: link `nx_game_private` whole-archive), `9b00c2e` (real process handle) |
| `hw-attempt2-unreachable` | `b9516f40` | Console off the network; nothing ran | - |
| `hw-attempt3-pre-exitlock` | `b9516f40` | Close from HOME: the log stopped mid-game, no shutdown lines | `194afbd` (`appletLockExit`), `6c7e9b5` (log the exit request) |
| `hw-attempt4-pre-errordialog` | `85c89a9d` | Bad configuration: User Break `2345-0021` (`LibnxError_BadGfxDequeueBuffer`) in `framebufferBegin` from `fatal_screen` after SDL had used the display; `nwindowReleaseBuffers` (`13595ef`) did not help (`hw-diag`) | `8988dc9` (system error dialog after SDL) |
| `hw-attempt5-pre-vu0native` | `e0946953` | All observations met, but `vcs_vu0_native.o` dropped like `vcs_overrides.o`: VU0 programs interpreted (same results, about 9 game fps) | `d68d19a` (self-registering library whole-archive, generic constructor check) |
| `hw-attempt6-pre-review-fixes` | `15b07bae` | All observations met (the first independent review's candidate); the review found that `nxrun.cfg` without a profile used the player's userdata and that M02's indirect-fallback sites were not classified against a run | `9a1c79e` (profile required; branch trace and `classify_indirect.py`) |
| `hw-focus-attempt1-no-input`, `hw-focus-attempt-no-input-2` | | Focus runs during which nobody pressed HOME, one nxlink send that hung, one FTP refusal; repeated with the user at the console | - |
