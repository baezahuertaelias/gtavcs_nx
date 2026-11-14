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
| `gtavcs.nro` (private game) | `tools/build_game.py`, clean `d68d19a`; rebuilt byte-identical at clean `8e8f6b3` | `15b07baefe52db8f4fe1b3c7ed354d860373e058e3e9c99037ada7b5989813c7` | 53,528,773 |
| `gtavcs_runtime_tests.nro` (public) | `tools/build_core_tests.py`, clean `8e8f6b3` (bundle manifest `root_dirty=false`) | `4172518ff7867d0e5d74fcfa65e0f34c481c1e57041f428660bc48a8249c8951` | |

The game NRO's manifest: generated EE/VU archive `6c44a0bd5dc6233e0b149ec13377ca66dde3eb5f75d51ae53ef8d8066fe1d317` linked whole-archive, `core_stub_linked=false`,
`game_overrides_linked=true`, `vu0_natives_linked=true`, `unlinked_constructors=[]`, thread-create wrap present.
The runtime-test bundle that ran was built before `4895086` was committed from the same file contents; the clean
rebuild at `8e8f6b3` reproduced its hash exactly.

## Final session (candidate `15b07bae...`)

| Run | Started | Log SHA-256 (`gtavcs.log` / results) | Observed |
|---|---|---|---|
| preflight | 13:30:03 | | No disc image under `/switch/gtavcs_nx`; folder-disc manifest and `SLUS_215.90` present |
| runtime suites (with the second-backup check) | 14:06:35 | `runtime-tests-M02.log` `9195e15f372fc1a1f5735ffe8481a42eac58aec3a4f54153cf1b371ebfd69c61` | Upstream suites `failures=0`; `[host-fs] failures=0`, 13 checks incl. card rename onto an existing name, card format and unformat of a card holding saves, `replaceFile` onto an existing target twice, a backup before a change and a second backup beside the first (`backups=2`); no new crash report. The suite's TMPDIR leftovers were removed afterwards (2 files, 61 folders) |
| `m04-boot` | 13:30:25 | `00ee39e1485219546853ecc5fc4b7ba721500190078984982c7a0d2ba29b4a18` / `1b6ca9b0eb48a0e54d7a42f0f5b2c5624168ce73a4b3920046453ab7349147b4` | Below |
| `m04-boot` again (`m04-boot-2`), same test card | 13:34:45 | `965a7a352906acc434227b756f5d0f0197194578442f6d1b7bb00ab92044a01f` / `0179c0988406f113d26241ca1a12464512f81f59415e95df23bb3a3716588b43` | Same lines as the first; the card tree before and after it is identical |
| `m04-files` (`m04-files-2`, final schedule) | 14:07:06 | `740a02b183b4f55bbe7122bf4610608944260b6eefa28f99099606d807f93e98` / `1411b568f62c08e73540e2b59ed6e6da33a2629cccb6ff7e07f777353feb1b43` | `verify problems=0` (whole disc tree hashed against the manifest); `setting display.fps 30` -> `ok 30`, `60` -> `ok 60`; `vcs.ini` `display.fps=60`, `audio.volume=90` after three saves, each after the first replacing the file; `setting m04.no.such.setting` -> `status=error reply="error unknown setting m04.no.such.setting"`, `m04-no-such-command` -> `status=error reply="error unknown command ..."`, the run carrying on to `stop reason=seconds` |
| `m04-save`, twice | 14:12:48, 14:15:28 | `11481bbb16ccb96253f596fa590db816b57e126983f5e9d2ae60eda5490c8e33` / `a339f04c13faaed2f1a6979e940db7afcdf2faa81802306f8e2a433adda166ad`; `68180e2800386d1d166a0818287cb6a8a55e83e0f860edb814c880a946be9120` / `86facba980c7b96286fcc7f449df8d62d6caed622fdb7d8f81f5b929a43d729c` | `[mc-test] save done` in both (the game's own memory card code). Under `userdata/tests/m04-save`: the save folder, `mc0/.ps2x-order` and the save folder's `.ps2x-order`, `backups/auto-20261114-142001.zip` (4 files); after the second run `backups/auto-20261114-142240.zip` (6 files, holding the first save) beside it |
| `m04-focus` | 13:52:41 | `fd73fe9e7f7c06c16aad04f93ef797b2c65652416213f5b6e3c22058cc6dc146` / `79bac8403f219f75edf5b2e2c9625a63becc0d2ac3c7a608962afae854c94c6d` | HOME: `confirmed suspension_ns=14035144636`, `focus t=40.689 state=out/in`; sleep: `suspension_ns=18127793594`, pairs at `56.384` / `56.391`; `game_frames` 1748 -> 1982 and 2456 -> 2612 across them; clean end |
| `m04-close` | 13:56:25 | `2764a33c7e5f0658595aa97e1f016c69b584a563b72463776d1d13c64091ada9` / `1d33012b83e727fe7da2564fec5c0a15b50a67f2734d0bc2944e10f0e3284d28` | Closed from HOME at about 40 s: `[platform] quit requested (applet exit request)`, `orderly stop after 2378 frames`, `game thread joined, render worker and platform stopped`, `[stream] reader stopped`, `runtime destroyed`, `folder disc closed`, `[shutdown] core, context, window and SDL closed`; no new crash report |
| `m04-badcfg` | 13:57:35 | `47b28ae0fdeed00e81d4587bb1ec1746baf29e68bbe153cd2d8f20b64e11116b` | `[core] nxrun.cfg:3: unknown directive 'bogus'`, `[boot] core boot failed (exit code 1)`, `failure shown in the system error dialog (00000000)`; the user read the dialog's details naming `nxrun.cfg:3: unknown directive 'bogus'` and pressed OK; FTP back after 15 s; no new crash report |

The first runtime-suite run (13:30:04, the suite without the second-backup check) and the first `m04-files` run
(13:39:06, without the error commands) also passed; they are superseded by the reruns above, on the same game NRO.
The two `m04-focus` runs at 13:41:28 and 13:51:57 had no console input or could not reach FTP
(`hw-focus-attempt-no-input-2`).

Every game run ended with the same tail: `stop reason=seconds` (or the close's exit request), `iso_opens=0`,
`first_iso=""`, `threads_created=6`, `[resources] shutdown live_threads=2` (boot 2, running 4, via the loader's
process handle), `[shutdown] core, context, window and SDL closed`, the NRO's SD read-back equal to the local hash
and identical crash-report listings before and after.

### The boot (`m04-boot`, both launches)

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
- No `unlisted-resume`, exception, unsupported-syscall, dormant-thread or `Unhandled` line in any final log.
- Measured, not a target: median `game_fps` over the last 20 report lines 46.1 (boot 1), 47.9 (boot 2) with the
  null renderer. The same route ran at about 9 game fps before `vcs_vu0_native.o` was linked (attempt 5).

### Desktop reference comparison

`tools/m04_compare.py build/evidence/M04/reference/reference-results.txt <run>-results.txt`, for both boots:

| | Reference | Switch boot 1 / 2 |
|---|---:|---:|
| first outdoors (s) | 15 | 25 / 25 |
| first follow camera (s) | 15 | 45 / 45 |
| errors | 0 | 0 / 0 |
| stop | seconds | seconds |

`both reached outdoors with a follow camera: True`. From 25 s on both sit at the same player position
(-1614.2, -132.3, 14.8) in area 0. The reference is upstream's own Linux build of the pristine pin, null RHI, SDL
offscreen video driver, same schedule ([VERIFICATION.md](VERIFICATION.md)).

## Earlier attempts in this session (each superseded by a fix and a full rerun)

Kept in `build/evidence/M04/`; none is evidence for the candidate.

| Folder | NRO | What it showed | Fix |
|---|---|---|---|
| `hw-attempt1` | `3dd30fc1` | `game_frames=0` for 240 s: the guest waited in the game's own PS2 movie player (`FUN_0040D930`) for a logo movie that never decodes; `menustate` unknown; `live_threads=-1` (`svcGetThreadList` 0xE401) | `bb8b622` (`vcs_overrides.o` dropped by the linker: link `nx_game_private` whole-archive), `9b00c2e` (real process handle) |
| `hw-attempt2-unreachable` | `b9516f40` | Console off the network; nothing ran | - |
| `hw-attempt3-pre-exitlock` | `b9516f40` | Close from HOME: the log stopped mid-game, no shutdown lines | `194afbd` (`appletLockExit`), `6c7e9b5` (log the exit request) |
| `hw-attempt4-pre-errordialog` | `85c89a9d` | Bad configuration: User Break `2345-0021` (`LibnxError_BadGfxDequeueBuffer`) in `framebufferBegin` from `fatal_screen` after SDL had used the display; `nwindowReleaseBuffers` (`13595ef`) did not help (`hw-diag`) | `8988dc9` (system error dialog after SDL) |
| `hw-attempt5-pre-vu0native` | `e0946953` | All observations met, but `vcs_vu0_native.o` dropped like `vcs_overrides.o`: VU0 programs interpreted (same results, about 9 game fps) | `d68d19a` (self-registering library whole-archive, generic constructor check) |
| `hw-focus-attempt1-no-input`, `hw-focus-attempt-no-input-2` | | Focus runs during which nobody pressed HOME, and one nxlink send that hung; repeated with the user at the console | - |
