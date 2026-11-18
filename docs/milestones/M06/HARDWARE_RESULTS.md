# M06 hardware results (2026-11-17)

The user's Switch, **handheld only** (charging), Mesa 20.1 nouveau OpenGL 4.3 core, stock clocks (the album captures
show Sphaira's status overlay: CPU 1020 MHz, GPU 307.2 MHz, memory 1331.1 MHz). Procedure: [HARDWARE.md](HARDWARE.md).
Scope: [SCOPE_DECISIONS.md](SCOPE_DECISIONS.md) (no Pro Controller, no dock, picture comparison without a new desktop
run). Records: [hardware/](hardware/) (logs as `*-gtavcs.txt`, the runs' results, summaries, perf reports,
transcript, crash-report lists before / after each run, person observations).

Game NRO: `1854e2be…` (code of `98b023b`) for runs 1-6; `4a15e8…` (plus the heap fields of the progress line) for
run 7 and run 8's first attempt; `4e4a8524…` (plus the PS2X_HLE fix below) for run 8; `9a9f7e12…` (built from the
clean `7a1f57d`) for runs 9-15; `9b89481c…` (built from the clean `b309413`: patch 0009, upstream's RSL recordings
pruned, M06-12) for runs 16 and 17. Up to `9a9f7e12` the changes act only with a run configuration (the progress
line) or with `PS2X_HLE` set (the fix); patch 0009 changes what every launch runs, so only runs 16 and 17 tested the
code the beta ships. The packaged NRO, `bf18fd1f…` (built from the review-5 commit), differs from `9b89481c` only in
the 20-byte build ID at 0x40-0x53 (the NRO header) and its copy in the GNU build-ID note at 0x331e5b0-0x331e5c3
(a byte compare of the two files: 38 bytes differ, all inside those ranges); code and data are identical. The build
ID changes with the patch series' hash, which is in the overlay checkout's path, so every edit to patch 0009's
comment or text gives a new one (review 4's `03000b3b` the same way). No build manifest
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
| 5 | play-handheld-2 | `m06-play.cfg`, the person playing | HOME close at 414 s | PASS as a 5-minute play run (5.3 min); its card was not clean, so the acceptance is run 13 |
| 6 | sticks-handheld | `m06-sticks.cfg` (90 s) | by `seconds` | PASS: L3 / R3 reported |
| 7 | soak-handheld | `m06-soak.cfg` (720 s, no input after the Cross schedule) | by `seconds` | clean run; heap trend: see M06-12 |
| 8 | reference-handheld | `m06-reference.cfg` (180 s, `PS2X_HLE=0`) | by `seconds` | PASS on the second attempt: the world through the recompiled VU1 programs (see M06-01 below) |
| 9 | wireless-handheld | `m06-wireless.cfg` (150 s), Joy-Cons detached | by `seconds` | no input logged (nothing pressed); resent |
| 10 | wireless-handheld-2 | `m06-wireless.cfg` (150 s), Joy-Cons detached | by `seconds` | PASS: wireless sticks, A, Start, movement in S4 |
| 11 | soak30-handheld | `m06-soak30.cfg` (1,860 s, no input after the Cross schedule) | by `seconds` | see M06-12 |
| 12 | accept-clean | `m06-play.cfg` on the cleaned card, the person playing | HOME close at 395 s | clean run, but only ~4.8 min of S4 (the cutscene ran until the person found Cross on B) and no disconnect logged: not the acceptance |
| 13 | accept-clean-2 | `m06-play.cfg` on the cleaned card, the person playing | HOME close at 456 s | PASS: the acceptance run (below) |
| 14 | cruise-handheld | `m06-cruise.cfg` (1,560 s, no input; review 3) | by `seconds` | clean run, but not the test it was meant to be: the Triangle presses did not put the player on the bike, so the route moved nothing |
| 15 | drive-handheld | `m06-drive.cfg` (1,260 s, no input; NRO `9a9f7e12`) | by `seconds` | clean run; heap in use rose to the end: the leak (M06-12) |
| 16 | drive-handheld-2 | `m06-drive.cfg` (1,260 s, no input; NRO `9b89481c` from `b309413`, patch 0009) | by `seconds` | PASS: heap in use levels off while driving (M06-12) |
| 17 | accept-clean-3 | `m06-play.cfg` on the re-cleaned card, the person playing; NRO `9b89481c` | HOME close at 390 s | PASS: the acceptance on the shipped code (below; the packaged `bf18fd1f` differs only in its build ID) |

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

Method caveats: perf.csv's `t_s` is the running sum of game frame times and drifts from the run's clock (by about 3 s
in the silent run, 4.5 s in the route, 7.4 s in play-handheld-2 and 25 s in run 4 at their ends); the stage windows
come from `gameinfo` times on the run's clock and are applied to `t_s`, so their edges are a few seconds off, and the
windows are kept clear of the transitions for that reason. GPU samples are placed by the run's clock. The GPU query
begins after the swap and ends before the next swap (`rhi_gl.cpp`, `present`), so GPU work or waiting inside the swap
itself is not in it: "present-to-present" means from just after one swap to just before the next.

| Run, stage (window) | Game frame median / p95 | Host frame median / p95 | GPU present-to-present median / p95 |
|---|---|---|---|
| silent, S2 flyover (3-45 s) | 48.1 / 172.4 ms | 41.9 / 90.2 ms | not timed |
| silent, S3 cutscene (60-195 s) | 49.2 / 129.5 ms | 50.0 / 85.5 ms | not timed |
| silent, S4 standing (225-360 s) | 43.9 / 68.1 ms | 48.6 / 63.0 ms | not timed |
| route, S4 standing (30-300 s) | 43.1 / 67.9 ms | 44.7 / 70.2 ms | 29.2 / 29.9 ms |
| play-handheld-2, S2 flyover (3-30 s) | 45.2 / 180.1 ms | 37.1 / 93.4 ms | 20.9 / 54.2 ms |
| play-handheld-2, S4 playing, on foot and on the bike (90-414 s) | 66.6 / 264.1 ms | 54.8 / 110.8 ms | 11.3 / 72.7 ms |
| accept-clean-2 (run 13, the acceptance on NRO `9a9f7e12`), S4 playing, mostly on the bike across the map (40-450 s) | 87.0 / 452.8 ms | 49.5 / 157.1 ms | 1.8 / 92.3 ms |
| accept-clean-3 (run 17, the acceptance on NRO `9b89481c`, the shipped code), S4 playing, on foot and driving (40-385 s) | 84.6 / 419.9 ms | 59.7 / 146.3 ms | 2.0 / 87.1 ms |

The route's S2 lasts only until the Cross press at 10 s; the S2 rows come from the silent and play runs. In the
route's S4 (perf_report from 100 s) the EE thread spends 36.7 ms per frame in game code (70 %), 4.5 ms in render
submission, 1.7 ms in the IOP and 9.7 ms waiting. The five worst route frames (590-790 ms) each fall about 2 s after
a scheduled screenshot (`present::capture`): capture cost, not the game. Memory: the process has 3.34 GB mapped
(`mapped_total`); the heap in use is 204-213 MiB once running and 535-626 MiB during S4 (soak). No FPS target (M10).

## Heap during play and exit / restart (M06-12)

Run 7 (`soak-handheld-heap.tsv`, 720 s, every 10 s; MiB = 1,048,576 bytes): heap in use rises while S1-S4 load
(296 MiB at 0 s, 545 MiB at 120 s) and from 120 s on still trends up, by about +7.9 MiB a minute (2-minute means
552.5, 555.8, 577.9, 607.1, 608.0 MiB); review 1 found that "no world leak" did not follow from it. Run 11
(`soak30-handheld-heap.tsv`, `m06-soak30.cfg`, NRO `9a9f7e12` from the clean `7a1f57d`) was planned for 1,860 s and
ran 998 s of S4 before the console was suspended for 746.6 s (`[lifecycle] confirmed suspension_ns=746589037813`:
sleep or HOME), after which the person closed it from HOME; the core stopped in order (`stop ... reason=runtime`, the
applet's exit request; no crash report). Its heap in use, 2-minute means:

| t (s) | 0-120 | 120-240 | 240-360 | 360-480 | 480-600 | 600-720 | 720-840 | 840-960 | 960-992 |
|---|---|---|---|---|---|---|---|---|---|
| mean MiB | 449.4 | 551.9 | 550.5 | 565.5 | 604.2 | 605.9 | 601.1 | 603.7 | 606.6 (4 samples) |

Least-squares trend: +8.3 MiB/min over 120-600 s, **-0.016 MiB/min over 600-992 s** (the last sample is at
991.9 s). The heap in use levels off at about 600 MiB from about 480 s once the area's models, textures and renderer
buffers are resident (textures at upstream's 3,000-entry cache cap, 2,400-2,975 / 163-201 MiB; buffers 9,300-11,300 /
30-37 MiB): a plateau of about 8.5 minutes, within about 16 minutes of standing in S4 (the run's first ~25 s are
S1-S3). No world leak is seen while standing. `heap_obtained` (what the allocator took from the system; newlib does
not give it back) still grows after the plateau, more slowly: 807 MiB by 600 s, 843 MiB by 992 s (about 4.5
MiB/min): fragmentation of freed memory, not live data.

While moving (run 5, played on foot and on the bike; that NRO had no heap fields on the progress line) the renderer's
objects stay bounded: from the eleventh `[rhi-stats]` line on, textures 1,862-3,010 and buffers 4,227-11,934, no
upward drift to the end. The heap itself was not sampled during moving play. That a moving-route heap soak is left to
M10-07 (which already requires a 30-minute soak with streaming) is the implementer's choice, not a user decision.

**Moving play (review 3; runs 14-16).** Review 3 asked for the heap during moving play with streaming. Run 14
(`m06-cruise.cfg`) did not move: its Triangle presses did not seat the player on the bike. Run 15 (`m06-drive.cfg`,
NRO `9a9f7e12`) makes a car beside the S4 spot with upstream's rig script, seats the player in it and lets the rig's
`cruise` drive it along upstream's `tools/routes/city.txt` (5,736 m over the busiest cells on both sides of the bay)
at 40 m/s in a loop, nobody touching the console, `VCS_STREAM_LOG=1`: the car went round 3.5 times and new areas
streamed in all the time. The heap in use rose to the end (2-minute means, MiB):

| t (s) | 0-120 | 120-240 | 240-360 | 360-480 | 480-600 | 600-720 | 720-840 | 840-960 | 960-1080 | 1080-1200 | 1200-1260 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| run 15 | 500.0 | 680.1 | 745.4 | 801.6 | 840.3 | 866.6 | 903.8 | 946.9 | 963.4 | 998.1 | 1029.5 |
| run 16 (patched) | 498.0 | 675.6 | 740.0 | 801.8 | 830.1 | 831.0 | 830.8 | 843.5 | 831.3 | 837.5 | 820.7 |

Run 15: +15.6 MiB/min over 700-1,260 s, and higher at the same route point on every loop (2,050 m: 641, 794, 904,
1,015 MiB), so a leak, not a cache filling. Method for the route points: the time the car passes the point on each
loop, interpolated between the `cruise status` answers (every 60 s; the distance unwrapped at the route's 5,736.1
m), and the heap sample nearest that time; only passes between two answers are counted. The renderer's own objects stayed bounded (textures 2,058-3,014 /
85-114 MiB, buffers 6,088-12,003 / 16-27 MiB from the eleventh `[rhi-stats]` line). The desktop reference (upstream's
build at the pinned commit, `tools/m04_reference.py`, now recording its private bytes) ran the same schedule with
the null renderer at 60 fps: +33.0 MiB/min over 700-1,260 s, 1,080 MiB at the end (the private bytes divided by
1,048,576), and upstream's RSL recording cache
(`g_recordings`, `src/game/vcs_rsl_world.cpp`) at 32,582 entries and still growing. A recording is keyed by its
block's guest address and length and was never removed; streaming puts a model somewhere else each time its area
comes back. Patch 0009 (`patches/upstream/0009-rsl-recordings-prune.patch`) drops recordings no chain used in the
last 3,600 chains, every 1,024 chains; a dropped recording is only a cache miss (the block is recorded again, or a
skipped one taken from guest RAM). Desktop with the patch: null renderer +1.4 MiB/min over 700-1,260 s, 391 MiB at
the end (max 414 MiB), 5,674 recordings (76,041 dropped); Vulkan +2.7 MiB/min, 784 MiB, 7,864 recordings (an earlier
unpatched Vulkan run is not comparable: another program held the GPU and the reference ran at 2 fps). Results in
`hardware/desktop/`. The cost: the desktop's median game time 5.3 -> 6.7 ms from blocks recorded again.

Run 16 (the same schedule, NRO `9b89481c` built from `b309413` with patch 0009, clean tree): the heap in use levels
off from about 480 s (two-minute means 820.7-843.5 MiB), **-0.5 MiB/min over 700-1,260 s**
(`hardware/drive-handheld-2-heap.tsv`); at the same route point on each loop, by the method above, 2,050 m: 624,
814, 801 MiB (t 121, 421, 812 s) and 4,000 m: 701, 836, 847 MiB (t 220, 556, 958 s), where run 15 kept rising. The plateau is higher than run 11's standing 600 MiB
because more of the city is resident. `heap_obtained` still grows, more slowly as the run goes on (+51.2 MiB/min
over 300-700 s, +17.1 over 700-1,260 s; 1,389 MiB of the 3,189 MiB mapped at the end): fragmentation of freed
memory, which newlib does not return; whether it levels off over a longer soak is for M10-07. The prune age is
3,600 chains (one a game frame, by the patch's own reading; not traced on the Switch, whose logs have no
`[rsl-world]` lines): a minute at 60 fps, but on the Switch while driving 8.6-10.4 minutes: 8.6 at run 16's
whole-run rate (8,034 cruise frames from 44 s to 1,200 s, 6.95 a second; 3.24 loops, about 2,480 frames a loop),
10.4 inside the 700-1,260 s window (5.76 a second between the answers at 720 s and 1,200 s; run 15: 7.42 a second
over the run). That window (560 s) is therefore somewhat shorter than one retention period (about 0.9); across it
the route points above move little (2,050 m: 814 then 801 MiB; 4,000 m: 836 then 847 MiB). On the desktop, blocks the game thread still holds confirmed whose recording was dropped now
take `replaySkipped`'s fallback from guest RAM (225 in the null-pruned run, 180 Vulkan-pruned, 0 unpatched; the
Switch has no write watch, so no skipping there). Both are handed to M10-07. 0 GL errors in every
`[rhi-stats]` line, stopped in order at 1,260 s, no crash report. Every run before 16 ran without patch 0009 (the
NROs above).

Exit during play: run 5 was closed from HOME while the player was moving through Fort Baxter (inputs to the last
second, 2,993 textures in the cache). That streaming was active at that moment is inferred from the movement, not
logged: `VCS_STREAM_LOG` is off in m06-play.cfg. The main loop ended on the applet's exit request (`appletMainLoop()` false: no `stop` line, the
results go straight to `shutdown`), then the game thread, the render worker, the stream reader, the runtime and
the folder disc stopped in that order (`play-handheld-2-gtavcs.txt`, last 15 lines). Runs 1-17 and run 8's first attempt (eighteen game launches) each ended in order (the core's workers joined: `live_threads=2` at shutdown, as at boot, in every `*-gtavcs.txt`), the session transcripts record `new_crashes=[]` for all eighteen (runs 14-16: `hardware/session-*.txt`; run 17:
`hardware/accept2/transcript.txt`), and the next launch started cleanly every time: no hang. Run 11 also resumed from a 746.6 s suspension and then closed from HOME in order. The heap still in use at
the `shutdown` line (297-947 MiB across the seventeen runs) is measured before main.c destroys the GL context (Mesa still holds the textures
then) and is returned with the process.

## Controller (M06-14, M06-16; paired Joy-Cons only)

- Identity at open: `controller physical: ... handheld styles 0x2 (Joy-Cons attached (handheld), device type 0xc)`;
  SDL's mapping `Switch Controller` with `a:b1, b:b0, x:b3, y:b2` under `SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS=0`
  (physical positions: SDL's A = south = Cross).
- Run 4: one change sequence, handheld (0x2), then at about 275 s `No1 styles 0x0 ... handheld styles 0x0: nothing
  connected for player 1, input neutral` for at most about 6.6 s (between the `player` answer at 275.2 s and the
  progress line at 280.7 s), then `No1 styles 0x4 (paired Joy-Cons (dual), device type 0x30)`, play continued, HOME
  close in order. That is consistent with sliding the Joy-Cons off the rails; no deliberate disconnect of wireless
  Joy-Cons (dual, then none, then dual) is in this run. SDL still had its joystick open throughout: the M01
  `connected=1` observation. M01's observation is resolved: switch-sdl2 keeps a fixed joystick list, so the connection now comes from the
  HID npad style sets.
- Run 5: buttons a 47, start 14, back 2, b, x, y, d-pad, leftshoulder; left stick 162 / 22 axis crossings, triggers;
  Start pressed 14 times (the log records SDL's events, not the game's menu; the run's two `[menu]` lines are
  boot-time messages present in every run); 5+ minutes of movement (the `player` answers move from
  (-1621.8, -142.8) at 85 s to (-1142.7, -1087.2) at 405 s, through the bike at 150 s).
- Run 6: `leftstick` 5 and `rightstick` 4 presses (L3 / R3).
- Run 4's wireless part is thin (review 1): the paired style appears just before a focus change at 281.9 s and the
  run ends at 293 s, with one A press, a few stick moves and no Start. Wireless proof is runs 9 and 10
  (`m06-wireless.cfg`, NRO `9a9f7e12` built from the clean `7a1f57d`; the person detached the Joy-Cons before the
  launch, although the configuration's comment describes detaching them during the run): style `No1 styles 0x4
  (paired Joy-Cons (dual), device type 0x30)` for the whole run. Run 9 logged no input at all (the person did not
  press anything; resent at their request). Run 10: `[input]` lines carry SDL's event time, about 2.1 s ahead of the
  run's `t`. The 42 A presses all fall before run t about 18 s, while the scripted Cross presses skip the cutscene; in S4
  (from about run t 20 s) the left stick (30 crossings) and right stick (33) move, Start is pressed twice (SDL time
  32.7 s and 33.6 s, run t about 30.6 s and 31.5 s), and the player walks from (-1616.0, -136.5) at run t 20 s to
  (-1624.8, -149.1) at 30 s; 0 GL errors, clean exit.
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

## Clean-card acceptance on the shipped code (M06-18; run 17, review 3)

Patch 0009 changed the game NRO after run 13, so the acceptance was repeated on NRO `9b89481c`, the code the beta ships (the packaged `bf18fd1f` differs only in its
build ID)
(built from `b309413`, clean tree; the same bytes from `359c757`). With the user's consent `clean-sd` cleaned the card
again (`hardware/accept2/sd-moves-restored.tsv`, `sd-tree-clean.txt`: 367 entries, only `gtavcs.nro` and the disc
files); `restore-sd` now names what the run made after its evidence folder (`/switch/gtavcs_nx_backup/acceptance-
accept2-*`), so run 13's kept output there keeps its names. After the run `sd-tree-restored.txt` (766 entries) equals
the tree before cleaning; it differs from run 16's preflight tree only in run 16's own `gamefolder/gtavcs.log` and
`m06-drive/nxrun-results.txt`, written after that listing. No separate before-clean listing was taken this time:
`sd-moves-restored.tsv` lists every move.

Run 17 (`hardware/accept2/accept-clean-3-gtavcs.txt`): Joy-Cons attached (`handheld styles 0x2 (Joy-Cons attached
(handheld), device type 0xc)`), application mode (`mapped_total=3343908864`), an empty test card; S1-S3 (the
cutscene skipped with B, which is Cross), the player at the S4 spot at 30 s (-1596.7, -133.5, on foot), then on foot
and driving (`state driving` at 90-150 s and 270-390 s) to (-687.2, -1383.4) at 365 s, until the HOME close at
390.4 s: about **6.0 continuous minutes of S4**. Inputs (SDL position names): a 66, b 12, y 12, x 10, left shoulder 2,
both sticks. 0 GL errors in all 81 `[rhi-stats]` lines; the game thread, render worker, stream reader, runtime and
folder disc stopped in order (`live_threads=2`); `new_crashes=[]`. S4 timing (40-385 s): game 84.6 / 419.9 ms, host
59.7 / 146.3 ms, GPU present-to-present 2.0 / 87.1 ms (`hardware/accept2/accept-clean-3-summary.txt`). Like run 13, it
was started through Sphaira's nxlink netloader with nxrun.cfg's instrumentation; stock clocks are inferred (the
console's settings were not changed). Video: run 5's clips by the user's decision (SCOPE_DECISIONS.md).

## Clean-card acceptance (M06-18, M06-14; runs 12 and 13, review 2)

Review 2: run 5's card also held old test NROs, logs, folders and the user's real save, so it was not "the NRO and
gamefolder only". With the user's consent, `tools/switch_m06_session.py clean-sd` moved everything but `gtavcs.nro`
and the disc files (the `.nxdisc` folder and the disc tree) into `/switch/gtavcs_nx_backup/` by FTP renames
(`hardware/accept/sd-moves-restored.tsv`); the tree before (`sd-tree-before-clean.txt`) and the clean tree
(`sd-tree-clean.txt`, re-cleaned before run 13: `sd-tree-clean-2.txt`, the game itself creates `gamefolder/mc0` and
`mc1` at boot) are recorded. The session tool's own additions are `gamefolder/userdata/nxrun.cfg` (deleted after the
run) and the run's test profile under `userdata/tests/m06-play` (an empty test card). After run 13,
`restore-sd` moved everything back; `sd-tree-restored.txt` equals `sd-tree-before-clean.txt` path for path and size for
size, the user's save included. NRO `9a9f7e12` (built from `7a1f57d`; byte-identical to the build from `f047894`).

Run 13 (`hardware/accept/accept-clean-2-gtavcs.txt`): paired Joy-Cons detached from the start (`No1 styles 0x4 (paired
Joy-Cons (dual), device type 0x30)`); application mode (`mapped_total=3343908864`); S1-S3, then the player at the S4
spot at 35 s and gameplay to the HOME close at 455.6 s, about 7 continuous minutes (6.1 of them before the
disconnect step): on foot and mostly on the bike (camera `behind vehicle` at 90-270 s and 390-450 s), from (-1614.2,
-132.3) at 35 s to (-738.6, 21.2) at 455 s: 888.9 units net, 2,671.4 along the 10-second samples. Inputs (SDL position names; SDL's `a` is the bottom button, labelled B on the
Switch: Cross): a 53 presses, b 12, y 11, x 7, leftstick and rightstick 2 each, both sticks and both triggers. 0 GL
errors; the game thread, render worker, stream reader, runtime and folder disc stopped in order; no crash report.
S4 timing (40-450 s): `hardware/accept/accept-clean-2-summary.txt`.

Deliberate disconnect of wireless Joy-Cons (the person pressed each Joy-Con's sync button): after the progress line
at 401.2 s `nothing connected for player 1, input neutral`; the progress line at 411.3 s reports `gamepad=0` and no
`[input]` event is logged while disconnected; the player is on the bike (`state driving` at 390 s and 450 s), which
comes to rest with neutral input: (-701.3, 253.8) at 405 s, (-703.6, 250.8) at 415 s, 0.3 units further at 425 s; then `paired Joy-Cons (dual)` again before the focus change at 420.6 s, and play continues to the HOME
close (the player at (-738.6, 21.2) at 455 s). So: dual, then none, then dual, with input neutral in between.

Run 12 (`accept-clean-gtavcs.txt`) is kept as a clean run that did not meet the 5 minutes (S4 from between 85 and 105
s to 391 s). The session's instructions for run 12 said A for Cross; the person first pressed A: by position
the bottom button, B on the Switch, is Cross (AGENTS.md), and the game behaved so.

Video: no album clips were taken during run 13; by the user's decision (SCOPE_DECISIONS.md) M06-18's video is run 5's
two clips.

How run 13 differs from a player's launch: it was started through Sphaira's nxlink netloader (sockets and nxlink's
stdout are open) and ran with nxrun.cfg's instrumentation (the test profile, perf / GPU logs, the input log, `player`
every 10 s, `gameinfo` every 60 s), not from the homebrew menu as BETA_NOTES.md tells players. Stock clocks for run 13
are inferred: the console's settings were not changed during the session, and the clock overlay is only in runs 4-6's
album captures.

Between runs 12 and 13 the card was cleaned again by hand over FTP, outside `clean-sd` (which refuses while a moves
file exists): run 12's own output (`gtavcs-boot.log`, `gamefolder/userdata`, `gamefolder/gtavcs.log` and the game's
`gamefolder/mc0`, `mc1`) moved to `/switch/gtavcs_nx_backup/accept1-*` (`hardware/accept/transcript.txt`,
18:44:45-18:44:54). The first `restore-sd` stopped with `553 File exists` (the run's own `mc0`, `mc1`); the tool was fixed
to move such leftovers aside and the second attempt passed. "Restored identically" is `/switch/gtavcs_nx`;
`/switch/gtavcs_nx_backup` keeps the two clean runs' output for the user to delete.
