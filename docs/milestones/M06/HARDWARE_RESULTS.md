# M06 hardware results (2026-11-17)

The user's Switch, **handheld only** (charging), Mesa 20.1 nouveau OpenGL 4.3 core, stock clocks (the album captures
show Sphaira's status overlay: CPU 1020 MHz, GPU 307.2 MHz, memory 1331.1 MHz). Procedure: [HARDWARE.md](HARDWARE.md).
Scope: [SCOPE_DECISIONS.md](SCOPE_DECISIONS.md) (no Pro Controller, no dock, picture comparison without a new desktop
run). Records: [hardware/](hardware/) (logs as `*-gtavcs.txt`, the runs' results, summaries, perf reports,
transcript, crash-report lists before / after each run, person observations).

Game NRO: `1854e2be…` (code of `98b023b`) for runs 1-6; `4a15e8…` (plus the heap fields of the progress line) for
run 7; `4e4a8524…` (plus the PS2X_HLE fix below) for run 8. The later changes act only with a run configuration (the
progress line) or with `PS2X_HLE` set (the fix); a player's launch runs the same code as runs 1-6. Every run: a new test profile (empty memory card, new settings), 0 GL errors in the last
`[rhi-stats]` line, `[shutdown] core, context, window and SDL closed`, no new crash report, the NRO on the card equal
to the local one (`sd_matches_local=True`).

## Runs

| # | Label | Configuration | Ended | Result |
|---|---|---|---|---|
| 1 | output-handheld | `m06-dock.cfg` (240 s, Cross schedule) | by `seconds` | PASS: `[video] output 1280x720 on the 1920x1080 surface, crop lower-left (00000000)`; the person: "It looks good and it has 4:3 ... nothing cut off" |
| 2 | route-handheld | `m06-route.cfg` (300 s) | by `seconds` | PASS: S1-S4, 11 screenshots, perf / GPU per frame, both movies "built without FFmpeg, skipped" |
| 3 | silent-handheld | `m06-silent.cfg` (360 s, `PS2X_AUDIO=0`, no input) | by `seconds` | PASS: no audio device; S3 cutscene from ~75 s to ~195 s, S4 by 225 s |
| 4 | play-handheld | `m06-play.cfg`, the person playing | HOME close | PASS as the Joy-Con connection-state run (gameplay ~3.5 min before the controller steps: not the acceptance run) |
| 5 | play-handheld-2 | `m06-play.cfg`, the person playing | HOME close at 414 s | PASS: the acceptance run |
| 6 | sticks-handheld | `m06-sticks.cfg` (90 s) | by `seconds` | PASS: L3 / R3 reported |
| 7 | soak-handheld | `m06-soak.cfg` (720 s, no input after the Cross schedule) | by `seconds` | PASS: heap in use bounded over 10 min of S4 |
| 8 | reference-handheld | `m06-reference.cfg` (180 s, `PS2X_HLE=0`) | by `seconds` | PASS on the second attempt: the world through the recompiled VU1 programs (see M06-01 below) |

A first attempt of run 1 stopped before launching (the session tool did not accept Sphaira's `450` reply for a
missing folder; fixed in the tool) and a first attempt of run 5 stopped before launching (Docker Desktop not
running); neither reached the console's game.

## Stages (route-handheld; screenshots private, hashes in `hardware/screenshot-sha256.txt`)

| Stage | Switch evidence |
|---|---|
| S1 legal / loading | `s1-002`: the legal screen with the title logo, 4:3 |
| S2 flyover | `s2-006`: palms against the sky (the first 3D world picture); `s2-015` black: the Cross presses at 10 s and 14 s skip the flyover (a fade); `s2-030` the following script camera |
| S3 cutscene | `s3-050`, `s3-070`; in the silent run `s3-060`..`s3-180`: the office scene, skinned characters, letterbox bars; camera `mode 17 flyby / cutscene` |
| S4 gameplay | `s4-110`..`s4-290`: Fort Baxter, the player next to Martinez's bike, the HUD (clock, health, money), the radar with north and the mission marker, 4:3 pillarboxed; camera `mode 4 follow ped` |

The album captures of the play runs (15 screenshots, 4 clips) show S4 on foot and on the bike, the mission text
("Ve a la terminal V.I.P. del aeropuerto.") and the button prompt; output 1280x720, 4:3, nothing cut off.

## Guest state against the desktop reference (M06-09)

The player at S4 (the `player` answers: ped, x, y, z, state) with the same Cross schedule:

| Run | Position once gameplay starts |
|---|---|
| desktop reference (M04, upstream's desktop build, `build/evidence/M04/reference/reference-results.txt`) | `01d439c0 -1614.150 -132.330 14.811 1`, heading 58.0, camera follow ped |
| Switch route-handheld | `01d439c0 -1614.150 -132.330 14.811 1` from t=25 s to the end, heading 58.0, camera `mode 4 follow ped` |
| Switch silent-handheld (no input; the cutscene ran out by itself) | `01d439c0 -1614.15 -132.33 14.81`, heading 58.0, from 225 s |
| Switch soak-handheld | camera `mode 4 follow ped` at all 12 `gameinfo` answers |

Identical to the printed precision. Pictures: compared by description with M05's desktop record of the same schedule
through upstream's Vulkan backend and the port's OpenGL backend (`docs/milestones/M05/desktop/README.md`: flyover with
palms and the sunset street; Fort Baxter with the world, the player, the HUD clock / health / money, the radar and the
blue mission marker). Every element of that record is in the Switch's S2-S4 pictures; no missing geometry, wrong
colours, orientation or HUD placement was seen. Not a pixel comparison (SCOPE_DECISIONS.md).

## Timing (M06-11; handheld, stock clocks, internal 1x = 640x448, output 1280x720)

From `perf.csv` (one row per game frame, `PS2X_PERF_LOG`) and `gpu.csv` (one row per presented frame,
`GL_TIME_ELAPSED` from a present to the next, `PS2X_GL_GPU_TIME`). Reports: `hardware/*-perf-report.txt`.

| Run, window | Game frame median / p95 | Host frame median / p95 | GPU frame median / p95 (whole run) |
|---|---|---|---|
| route, S2 (3-38 s) | 39.2 / 102.1 ms | 44.1 / 62.9 ms | 29.2 / 30.8 ms |
| route, S4 (100-304 s) | 43.3 / 68.8 ms (19.0 fps mean) | 44.1 / 72.2 ms | (as above) |
| silent, S4 (100-360 s) | 44.5 / 81.9 ms | 48.9 / 69.8 ms | not timed |
| play-handheld-2, S2 | 48.2 / 205.1 ms | 37.4 / 95.7 ms | 26.5 / 67.8 ms |
| play-handheld-2, S4 (playing, on foot and on the bike) | 66.8 / 267.5 ms | 55.0 / 110.9 ms | (as above) |

In the route's S4 the EE thread spends 36.7 ms per frame in game code (70 %), 4.5 ms in render submission, 1.7 ms in
the IOP and 9.7 ms waiting. The five worst route frames (590-790 ms) each fall about 2 s after a scheduled screenshot
(`present::capture`): capture cost, not the game. Memory: the process has 3.34 GB mapped (`mapped_total`); the heap
in use is 214-223 MB once running and 535-626 MB during S4 (soak). No FPS target (M10).

## Heap during play and exit / restart (M06-12)

`soak-handheld-heap.tsv`, every 10 s: heap in use rises while S1-S4 load (296 MB at 0 s, 545 MB at 120 s) and then
stays between 535 and 626 MB to 720 s, falling as well as rising; the renderer's objects stay bounded (textures at
upstream's 3,000-entry cache cap, 2,854 / 194 MiB at the end; buffers ~11,500 / 37 MiB). No steady growth: no world
leak in 10 minutes of S4. `heap_obtained` (what the allocator took from the system) grows from 573 MB at 120 s to
828 MB at 720 s, more slowly over time (+160 MB in 120-360 s, +94 MB in 360-720 s): newlib does not return freed
memory, so this is fragmentation, not live data. Recorded for M10-07's 30-minute soak.

Exit with active streaming: run 5 was closed from HOME during play (the player moving through Fort Baxter, inputs to
the last second, 2,993 textures in the cache; this run did not log stream reads, `VCS_STREAM_LOG` is off in
m06-play.cfg). The main loop ended on the applet's exit request (`appletMainLoop()` false: no `stop` line, the
results go straight to `shutdown`), then the game thread, the render worker, the stream reader, the runtime and
the folder disc stopped in that order (`play-handheld-2-gtavcs.txt`, last 15 lines). Runs 1-7 each ended in order (the core's workers joined: `live_threads=2` at shutdown, as at boot) and the
next launch started cleanly every time: seven consecutive launches, no hang, no crash report. The heap still in use at
the `shutdown` line (500-620 MB) is measured before main.c destroys the GL context (Mesa still holds the textures
then) and is returned with the process.

## Controller (M06-14, M06-16; paired Joy-Cons only)

- Identity at open: `controller physical: ... handheld styles 0x2 (Joy-Cons attached (handheld), device type 0xc)`;
  SDL's mapping `Switch Controller` with `a:b1, b:b0, x:b3, y:b2` under `SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS=0`
  (physical positions: SDL's A = south = Cross).
- Run 4: at 275 s `No1 styles 0x0 ... handheld styles 0x0: nothing connected for player 1, input neutral` (the
  Joy-Cons disconnected; SDL still had its joystick open: the M01 `connected=1` observation), then
  `No1 styles 0x4 (paired Joy-Cons (dual), device type 0x30)` (reconnected wireless), play continued, HOME close in
  order. M01's observation is resolved: switch-sdl2 keeps a fixed joystick list, so the connection now comes from the
  HID npad style sets.
- Run 5: buttons a 47, start 14, back 2, b, x, y, d-pad, leftshoulder; left stick 162 / 22 axis crossings, triggers;
  Start opened the pause menu (`[menu] ...` lines); 5+ minutes of movement (the `player` answers move from
  (-1621.8, -142.8) at 85 s to (-1142.7, -1087.2) at 405 s, through the bike at 150 s).
- Run 6: `leftstick` 5 and `rightstick` 4 presses (L3 / R3).
- Exit: HOME close is the deliberate exit (the smoke build's Plus-to-exit is not in the game build; Plus is Start).

## The reference world path (M06-01)

The first attempt of run 8 set `PS2X_HLE=0` and still drew natively (`[hle] world packets ... drawn 480793`):
upstream's `vcs_hle_world.cpp` picks the world path in a static initializer, before the core applies nxrun.cfg's
environment (the desktop sets it in the process environment). Fixed in `src/core/core.cpp`: after applying the run's
environment, `PS2X_HLE=0` removes the MSCAL hooks again, as the initializer would have. Second attempt:
`[core] PS2X_HLE=0: the world through the VU1 programs (reference path)`, no `[hle] world packets` line, 715-868
VU1 programs per frame, 0 GL errors, clean exit; the S3 office scene drawn correctly through the OpenGL RHI
(screenshots `reference-handheld/*`). The reference path runs at 2.4-2.9 fps here, so in 180 s it reached S3, not
S4. `PS2X_HLE_VERIFY` from nxrun.cfg is not applied (its hooks are private to that file) and the core says so.
