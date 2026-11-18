# M06 review 6 (candidate `e03b04e`): REQUEST_CHANGES, and what was done

Reopened M06-12 and M06-18 (`d7cd8ee`), fixed, rechecked, ticked again. The report: REVIEW6.md.

| # | Finding | Resolution |
|---|---|---|
| 1 | The prune age "8.6-10.4 minutes" is two averages, not the span | Measured as the real time for 3,600 cruise frames from each `cruise status` answer (interpolated between answers; rechecked): run 16 6.7-10.5 minutes, run 15 6.6-9.3, shortest early in the drive; the 60 s rates 4.6-12.1 frames a second. Stated as "measured at 6.6-10.5 minutes" in HARDWARE_RESULTS.md (8.6 and 10.4 kept there, labelled as average-rate estimates), TASKS.md M06-12 and MILESTONES.md M10-07. Patch 0009's comment and PERFORMANCE.md text now give the age without Switch figures ("a minute at 60 fps, longer at lower frame rates"; the Switch figures live in the M06 records), so later record corrections no longer change the patch. |
| 2 | TASKS.md M06-18 cites a heading that no longer exists | Cites "Clean-card acceptance on the shipped code". |

The patch edit gave the NRO a new build ID once more: the packaged NRO is `5c2b558f`. Byte compare with the tested
`9b89481c`: 40 bytes differ, none outside the build ID at 0x40-0x53 and its copy at 0x331e5b0-0x331e5c3; code and data
are identical.
