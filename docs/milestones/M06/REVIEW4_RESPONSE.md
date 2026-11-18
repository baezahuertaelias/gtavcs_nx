# M06 review 4 (candidate `856af57`): REQUEST_CHANGES, and what was done

Reopened M06-12, M06-18 and M06-19 (`9352901`), fixed, rechecked, ticked again. The report: REVIEW4.md.

| # | Finding | Resolution |
|---|---|---|
| 1 | Wrong NRO history | HARDWARE_RESULTS.md lists each run's NRO from the session transcripts: `1854e2be` runs 1-6, `4a15e831` run 7 and run 8's first attempt, `4e4a8524` run 8, `9a9f7e12` runs 9-15, `9b89481c` runs 16-17; "every run before 16 ran without patch 0009" (HARDWARE_RESULTS.md, REVIEW3_RESPONSE.md). |
| 2 | "A player's launch runs the same code as runs 1-6" | Replaced: patch 0009 changes what every launch runs; only runs 16 and 17 tested the shipped code. |
| 3 | Run 16's route-point figures; the omitted last bin | The method stated (passes interpolated between the `cruise status` answers, the nearest heap sample, only passes between two answers). Run 16 at 2,050 m: 624, 814, 801 MiB; at 4,000 m: 701, 836, 847 MiB. Run 15 at 2,050 m: 641, 794, 904, 1,015 MiB (the last was 1,001 by eye before). Two-minute means quoted as 820.7-843.5 MiB (HARDWARE_RESULTS.md, TASKS.md, evidence.json, REVIEW3_RESPONSE.md). |
| 4 | Runs 14-16 without a tracked transcript | Their session transcripts tracked as `hardware/session-cruise-handheld.txt`, `session-drive-handheld.txt`, `session-drive-handheld-2.txt` (each `new_crashes=[]`; runs 15 / 16 name NRO `9a9f7e12` / `9b89481c`), cited in HARDWARE_RESULTS.md, TASKS.md and evidence.json. |
| 5 | Desktop MiB labelled MB | Relabelled MiB (the private bytes divided by 1,048,576) in HARDWARE_RESULTS.md, TASKS.md, evidence.json, REVIEW3_RESPONSE.md and the PERFORMANCE.md text in patch 0009. |
| 6 | The prune age on the Switch | The patch comment and its PERFORMANCE.md text state it: 3,600 chains are a minute at 60 fps and about 8.6 minutes on the Switch while driving (about 7 game fps; about 2,480 frames a loop). HARDWARE_RESULTS.md states that run 16's 700-1,260 s window is about one retention period, with the route points flat across it, and records the desktop's skip fallbacks from guest RAM (225 null-pruned, 180 Vulkan-pruned, 0 unpatched). Both handed to M10-07 in MILESTONES.md. |
| 7 | BETA_NOTES.md's frame times | Each row dated and named as an earlier build or this beta's code; run 17's row added (84.6 / 419.9 ms, 2026-11-18); the section dated 2026-11-17 and 2026-11-18. |

The comment and the PERFORMANCE.md text changed patch 0009's bytes, hence the patch series' hash, which is part of the
overlay checkout's path. The NRO rebuilt from this change, `03000b3b`, differs from run 16 / 17's `9b89481c` in 40 bytes
only: the 20-byte GNU build ID and its copy (a byte compare of the two files). Code and data are identical, so runs 16
and 17 hold for the packaged NRO; no new hardware run.
