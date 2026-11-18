# M06 review 5 (candidate `3d36559`): REQUEST_CHANGES, and what was done

Reopened M06-12 and M06-18 (`c03dd6e`), fixed, rechecked, ticked again. The report: REVIEW5.md.

| # | Finding | Resolution |
|---|---|---|
| 1 | `9b89481c` called the shipped NRO | Each place now says run 17 ran on NRO `9b89481c`, the shipped code, and that the packaged NRO differs only in its build ID: evidence.json (M06-18, M06-11), HARDWARE_RESULTS.md (row 17, the timing row, the acceptance heading and its first sentence), HARDWARE.md (row 17), REVIEW3_RESPONSE.md's reference. |
| 2 | The prune age is a range | 8.6-10.4 minutes on the Switch while driving: 8.6 at run 16's whole-run rate (6.95 cruise frames a second), 10.4 inside the 700-1,260 s window (5.76 a second, between the answers at 720 s and 1,200 s; rechecked from the results); that window is about 0.9 of a retention period, and the route points across it are given (HARDWARE_RESULTS.md, TASKS.md, MILESTONES.md M10-07, patch 0009's comment and PERFORMANCE.md text). One chain per game frame stays untraced on the Switch, stated as such. |
| 3 | The patch comment lacks a unit | "8.6-10.4 min at the Switch's ~6-7 game fps driving". |
| 4 | AGENTS.md's status | A sentence on review 3's leak, patch 0009 and run 17 (84.6 / 419.9 ms while moving) added. |
| 5 | MB and MiB mixed | "1,389 MiB of the 3,189 MiB mapped". |

Patch 0009's bytes changed again (the comment and its text), so the NRO's build ID changed with the series hash: the
packaged NRO is now `bf18fd1f`. Byte compare with the tested `9b89481c`: 38 bytes differ, all inside the build ID at
0x40-0x53 and its copy at 0x331e5b0-0x331e5c3 (two bytes of the new ID happen to equal the old); code and data are
identical, so runs 16 and 17 hold for it.
