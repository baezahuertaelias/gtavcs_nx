# M06 hardware results (2026-11-17)

The user's Switch, **handheld only** (charging), Mesa 20.1 nouveau OpenGL 4.3 core, stock clocks (the album captures
show Sphaira's status overlay: CPU 1020 MHz, GPU 307.2 MHz, memory 1331.1 MHz). Procedure: [HARDWARE.md](HARDWARE.md).
Scope: [SCOPE_DECISIONS.md](SCOPE_DECISIONS.md) (no Pro Controller, no dock, picture comparison without a new desktop
run). Records: [hardware/](hardware/) (logs as `*-gtavcs.txt`, the runs' results, summaries, perf reports,
transcript, crash-report lists before / after each run, person observations).

Game NRO: `1854e2be…` (code of `98b023b`) for runs 1-6; `4a15e8…` (plus the heap fields of the progress line) for
run 7; `4e4a8524…` (plus the PS2X_HLE fix below) for run 8. The later changes act only with a run configuration (the
progress line) or with `PS2X_HLE` set (the fix); a player's launch runs the same code as runs 1-6. No build manifest
was kept for `1854e2be` and `4a15e8`: that they match those commits is the build log's and transcript's word, not
checkable now (the packaged NRO is checked against its manifest: tools/package_beta.py). Every run: a new test
profile (empty memory card, new settings), `[shutdown] core, context, window and SDL closed`, no new crash report
(transcript `new_crashes=[]`), the NRO on the card equal to the local one (`sd_matches_local=True`); 0 GL errors in
the last `[rhi-stats]` line of every run that logs it (all but run 6, whose configuration does not set
`PS2X_RHI_STATS`). Application mode: every run reports `mapped_total=3343908864` (3.34 GB), the full application
memory pool, not applet mode's.

## Runs

| # | Label | Configuration | Ended | Result |
|---|---|---|---|---|
| 1 | output-handheld | `m06-dock.cfg` (240 s, Cross schedule; used with no dock changes) | by `seconds` | PASS: `[video] output 1280x720 on the 1920x1080 surface, crop lower-left (00000000)`; the person: "It looks good and it has 4:3 ... nothing cut off" |
| 2 | route-handheld | `m06-route.cfg` (300 s) | by `seconds` | PASS: S1-S4, 11 screenshots, perf / GPU per frame, both movies "built without FFmpeg, skipped" |
| 3 | silent-handheld | `m06-silent.cfg` (360 s, `PS2X_AUDIO=0`, no input) | by `seconds` | PASS: no audio device; S2 flyover to ~45 s, S3 cutscene from before 60 s to after 195 s, S4 by 225 s |
| 4 | play-handheld | `m06-play.cfg`, the person playing | HOME close | PASS as the Joy-Con connection-state run (gameplay ~3.5 min before the controller steps: not the acceptance run) |
| 5 | play-handheld-2 | `m06-play.cfg`, the person playing | HOME close at 414 s | PASS: the acceptance run |
| 6 | sticks-handheld | `m06-sticks.cfg` (90 s) | by `seconds` | PASS: L3 / R3 reported |
| 7 | soak-handheld | `m06-soak.cfg` (720 s, no input after the Cross schedule) | by `seconds` | clean run; heap trend: see M06-12 |
| 8 | reference-handheld | `m06-reference.cfg` (180 s, `PS2X_HLE=0`) | by `seconds` | PASS on the second attempt: the world through the recompiled VU1 programs (see M06-01 below) |
| 9 | wireless-handheld | `m06-wireless.cfg` (150 s), Joy-Cons detached | by `seconds` | no input logged (nothing pressed); resent |
| 10 | wireless-handheld-2 | `m06-wireless.cfg` (150 s), Joy-Cons detached | by `seconds` | PASS: wireless sticks, A, Start, movement in S4 |
| 11 | soak30-handheld | `m06-soak30.cfg` (1,860 s, no input after the Cross schedule) | by `seconds` | see M06-12 |

A first attempt of run 1 stopped before launching (the session tool did not accept Sphaira's `450` reply for a
missing folder; fixed in the tool) and a first attempt of run 5 stopped before launching (Docker Desktop not
running); neither reached the console's game. Runs 4 and 5 share the `m06-play` label, so run 5's crash-report and
userdata lists overwrote run 4's, as run 8's second attempt overwrote its first; for those, the transcript's
`new_crashes=[]` lines are the record.

## Stages (screenshots private, hashes in `hardware/screenshot-sha256.txt`)

Where each stage starts and ends in a run comes from the camera in its `gameinfo` answers (every 30 s; listed at the
top of each `hardware/*-summary.txt`) and the screenshots. Route (Cross every 4 s from 10 s): S2 skipped at 10 s, the
cutscene camera at 15 s, S4 from about 25 s (the player standing at the S4 spot; HUD and the subtitle at 30 s).
Silent (no input): flyover camera at 15 and 45 s, the S3 office scene at 60 s, the cutscene camera to 195 s, S4 from
225 s.

| Stage | Switch evidence |
|---|---|
| S1 legal / loading | route `s1-002`: the legal screen with the title logo, 4:3 |
| S2 flyover | route `s2-006`: palms against the sky (the first 3D world picture); silent `s2-030`: the flyover's army truck on a sunset road. Route `s2-015` is black: the transition after the Cross presses at 10 s and 14 s skipped the flyover |
| S3 cutscene | silent `s3-060`, `s3-120`, `s3-180`, and reference `s4-120`, `s4-170` (named by schedule; still S3 in that slow run): the office scene, skinned characters, letterbox bars; camera `mode 17 flyby / cutscene` |
| S4 gameplay | route `s2-030`, `s3-050`, `s3-070` (named by the planned schedule, taken in S4: the Cross presses skipped S2 and S3) and `s4-110`..`s4-290`; silent `s4-240`..`s4-350`: Fort Baxter, the player next to Martinez's bike, the HUD (clock, health, money), the radar with north and the mission marker, the mission subtitle ("Coge la moto del sargento Martinez."), 4:3 pillarboxed; camera `mode 4 follow ped` |

Album captures (`hardware/album-sha256.txt`: 14 screenshots, 4 video clips; files private). The album's clock runs
about 5-7 minutes ahead of the PC's (run 4 ended at 13:13:53 by the PC, its captures are stamped 13:15-13:18), so by
time: the 10 screenshots stamped 13:15:41-13:18:14 are run 4; the screenshots 16:19:28, 16:19:57, 16:20:15 and the
clips 16:20:34 and 16:21:03 (19.9 MB, 19.4 MB; the Switch records at most 30 s per clip) are run 5, the acceptance
run; the clips 16:28:47, 16:28:55 and the screenshot 16:29:00 are run 6 (sticks). They show S4 on foot and on the
bike, the mission text ("Ve a la terminal V.I.P. del aeropuerto.") and the button prompt, 4:3, nothing cut off.

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
`PS2X_GL_GPU_TIME`), cut at each run's own stage boundaries (above; `tools/switch_m06_session.py summary --window`).
GPU column: `GL_TIME_ELAPSED` from the query begun at one present to its end at the next, as the GPU reaches those
points; it includes any GPU idle time in between, so it is an upper bound of the GPU's busy time per frame, not the
busy time itself. gpu.csv's frame numbers are placed in time through the progress lines' `host_frames` (every 5-10 s).
Summaries: `hardware/*-summary.txt`; whole-run reports: `hardware/*-perf-report.txt`.

| Run, stage (window) | Game frame median / p95 | Host frame median / p95 | GPU present-to-present median / p95 |
|---|---|---|---|
| silent, S2 flyover (3-45 s) | 48.1 / 172.4 ms | 41.9 / 90.2 ms | not timed |
| silent, S3 cutscene (60-195 s) | 49.2 / 129.5 ms | 50.0 / 85.5 ms | not timed |
| silent, S4 standing (225-360 s) | 43.9 / 68.1 ms | 48.6 / 63.0 ms | not timed |
| route, S4 standing (30-300 s) | 43.1 / 67.9 ms | 44.7 / 70.2 ms | 29.2 / 29.9 ms |
| play-handheld-2, S2 flyover (3-30 s) | 45.2 / 180.1 ms | 37.1 / 93.4 ms | 20.9 / 54.2 ms |
| play-handheld-2, S4 playing, on foot and on the bike (90-414 s) | 66.6 / 264.1 ms | 54.8 / 110.8 ms | 11.3 / 72.7 ms |

The route's S2 lasts only until the Cross press at 10 s; the S2 rows come from the silent and play runs. In the
route's S4 (perf_report from 100 s) the EE thread spends 36.7 ms per frame in game code (70 %), 4.5 ms in render
submission, 1.7 ms in the IOP and 9.7 ms waiting. The five worst route frames (590-790 ms) each fall about 2 s after
a scheduled screenshot (`present::capture`): capture cost, not the game. Memory: the process has 3.34 GB mapped
(`mapped_total`); the heap in use is 214-223 MiB once running and 535-626 MiB during S4 (soak). No FPS target (M10).

## Heap during play and exit / restart (M06-12)

Run 7 (`soak-handheld-heap.tsv`, 720 s, every 10 s; MiB = 1,048,576 bytes): heap in use rises while S1-S4 load
(296 MiB at 0 s, 545 MiB at 120 s) and from 120 s on still trends up, by about +7.9 MiB a minute (2-minute means
552.5, 555.8, 577.9, 607.1, 608.0 MiB); review 1 found that "no world leak" did not follow from it. Run 11
(`soak30-handheld-heap.tsv`, `m06-soak30.cfg`, NRO `9a9f7e12` from the clean `7a1f57d`) was planned for 1,860 s and
ran 998 s of S4 before the console was suspended for 746.6 s (`[lifecycle] confirmed suspension_ns=746589037813`:
sleep or HOME), after which the person closed it from HOME; the core stopped in order (`stop ... reason=runtime`, the
applet's exit request; no crash report). Its heap in use, 2-minute means:

| t (s) | 0-120 | 120-240 | 240-360 | 360-480 | 480-600 | 600-720 | 720-840 | 840-960 | 960-998 |
|---|---|---|---|---|---|---|---|---|---|
| mean MiB | 449.4 | 551.9 | 550.5 | 565.5 | 604.2 | 605.9 | 601.1 | 603.7 | 608.0 |

Least-squares trend: +8.3 MiB/min over 120-600 s, **+0.02 MiB/min over 600-998 s**. The heap in use levels off at
about 600 MiB once the area's models, textures and renderer buffers are resident (textures at upstream's 3,000-entry
cache cap, 2,400-2,975 / 163-201 MiB; buffers 9,300-11,300 / 30-37 MiB): no world leak is seen in 16.6 minutes of
standing in S4. `heap_obtained` (what the allocator took from the system; newlib does not give it back) still grows
after the plateau, more slowly: 807 MiB by 600 s, 843 MiB by 998 s (about 4.5 MiB/min): fragmentation of freed
memory, not live data. A longer soak, gameplay with movement and the growth of `heap_obtained` are M10-07's 30-minute
soak; this is standing at one place, not a moving route.

Exit with active streaming: run 5 was closed from HOME during play (the player moving through Fort Baxter, inputs to
the last second, 2,993 textures in the cache; this run did not log stream reads, `VCS_STREAM_LOG` is off in
m06-play.cfg). The main loop ended on the applet's exit request (`appletMainLoop()` false: no `stop` line, the
results go straight to `shutdown`), then the game thread, the render worker, the stream reader, the runtime and
the folder disc stopped in that order (`play-handheld-2-gtavcs.txt`, last 15 lines). Runs 1-11 and run 8's first attempt (twelve game launches, back to back) each ended in order (the core's workers joined: `live_threads=2` at shutdown, as at boot, in every `*-gtavcs.txt`), the transcript records `new_crashes=[]` for all twelve, and the next launch started cleanly every time: no hang. Run 11 also resumed from a 746.6 s suspension and then closed from HOME in order. The heap still in use at
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
  Start pressed 14 times (the log records SDL's events, not the game's menu; the run's two `[menu]` lines are
  boot-time messages present in every run); 5+ minutes of movement (the `player` answers move from
  (-1621.8, -142.8) at 85 s to (-1142.7, -1087.2) at 405 s, through the bike at 150 s).
- Run 6: `leftstick` 5 and `rightstick` 4 presses (L3 / R3).
- Run 4's wireless part is thin (review 1): the paired style appears just before a focus change at 281.9 s and the
  run ends at 293 s, with one A press, a few stick moves and no Start. Wireless proof is runs 9 and 10
  (`m06-wireless.cfg`, NRO `9a9f7e12` built from the clean `7a1f57d`, Joy-Cons detached from the start): style
  `No1 styles 0x4 (paired Joy-Cons (dual), device type 0x30)` for the whole run. Run 9 logged no input at all (the
  person did not press anything; resent at their request). Run 10: between 18 and 36 s (S4 from about 20 s) the
  left stick (30 crossings), the right stick (33), A 8 and Start 4 events (two presses, 32.7 s and 33.6 s), and the
  player walks from (-1616.0, -136.5) at 20 s to (-1624.8, -149.1) at 30 s; 0 GL errors, clean exit.
- Exit: HOME close is the deliberate exit (the smoke build's Plus-to-exit is not in the game build; Plus is Start).

## The reference world path (M06-01)

The first attempt of run 8 set `PS2X_HLE=0` and still drew natively (`[hle] world packets ... drawn 480793`):
upstream's `vcs_hle_world.cpp` picks the world path in a static initializer, before the core applies nxrun.cfg's
environment (the desktop sets it in the process environment). Fixed in `src/core/core.cpp`: after applying the run's
environment, `PS2X_HLE=0` removes the MSCAL hooks again, as the initializer would have. Second attempt:
`[core] PS2X_HLE=0: the world through the VU1 programs (reference path)`, no `[hle] world packets` line, 296-1,774
VU1 programs per frame (the perf lines from 20 s), 0 GL errors, clean exit; the S3 office scene drawn correctly
through the OpenGL RHI (screenshots `reference-handheld/*`). The reference path runs at 1.8-10.1 game fps here, so in
180 s it reached S3, not S4. `PS2X_HLE_VERIFY` from nxrun.cfg is only partly applied (the RSL world hooks see it and
stay off; the MSCAL verification hooks are private to vcs_hle_world.cpp) and the core says so.
