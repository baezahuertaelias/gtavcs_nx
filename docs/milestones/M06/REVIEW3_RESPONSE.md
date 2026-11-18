# M06 review 3 (candidate `35d412c`): what was done

The review's own text was not kept in the repository; this lists the changes made in answer to it, as committed.

| Area | Resolution |
|---|---|
| M06-12: the heap during moving play with streaming | Reopened (`b309413`). Run 14 (`m06-cruise.cfg`) did not move the player. Run 15 (`m06-drive.cfg`: a car on upstream's city route at 40 m/s, 21 minutes, nobody touching the console) found a leak: heap in use +15.6 MiB/min to the end, higher at the same route point on every loop. The desktop reference showed it too (+33.0 MiB/min, null renderer): upstream's RSL recording cache, keyed by guest address, never pruned. Patch 0009 prunes it (desktop +1.4 MiB/min null, +2.7 MiB/min Vulkan). Run 16 on the patched NRO `9b89481c`: heap in use levels off (two-minute means 820.7-843.5 MiB from 480 s), -0.5 MiB/min over 700-1,260 s. `heap_obtained`'s slowing growth (fragmentation) is recorded for M10-07. HARDWARE_RESULTS.md, "Moving play". |
| M06-11: the moving acceptance run's timing | Run 13's S4 (40-450 s, mostly on the bike) added: game 87.0 / 452.8 ms, host 49.5 / 157.1 ms, GPU present-to-present 1.8 / 92.3 ms (HARDWARE_RESULTS.md, BETA_NOTES.md). |
| M06-12 figures | Run 11's trend restated over the samples it has (-0.016 MiB/min over 600-992 s); S4 length about 973 s of the 998 s run; the shutdown heap range across all runs. |
| M06-14: run 13's disconnect | `gamepad=0` and no input events while disconnected stated; the player was on the bike, which comes to rest with neutral input (not "standing"). |
| M06-16: run 10's presses | All 42 A presses fall before run t about 18 s (in the cutscene skip). |
| M06-18: run 13's distances and provenance | 888.9 units net / 2,671.4 along the samples over 35-455 s; launched through nxlink with nxrun.cfg's instrumentation, not from the homebrew menu; stock clocks inferred; the card cleaned again by hand over FTP between runs 12 and 13 and the first `restore-sd` failure (fixed) recorded; run 12's wrong button instruction stated as the session's. |

Patch 0009 changes the game NRO: every run before 16 ran without it (runs 9-15 on `9a9f7e12`, earlier runs on older
NROs: HARDWARE_RESULTS.md), runs 16 and 17 on the patched `9b89481c`, whose code the beta ships. By the user's choice the clean-card acceptance was repeated on it: run 17, about 6.0 continuous minutes of S4,
HOME close, 0 GL errors, no crash (HARDWARE_RESULTS.md, "Clean-card acceptance on the shipped code").
