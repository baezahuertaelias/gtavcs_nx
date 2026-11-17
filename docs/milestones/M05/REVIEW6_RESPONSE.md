# M05 independent review 6: findings and resolutions

Review 6 (a fresh independent agent, read-only) examined candidate `8255bb2` and returned **REQUEST_CHANGES** with one
Low finding in the records and one informational note on the code; it confirmed every finding of reviews 1-5 resolved
except 5.4 (below) and found nothing wrong in the code or the hardware evidence. M05-14 was reopened (`git log` of
docs/MILESTONES.md). No code or hardware run changed.

| # | Severity | Finding (summary) | Resolution | Proof |
|---|---|---|---|---|
| 1 | Low | TASKS.md's M05-14 row still said "within 12 KB / 2 KB" (review 5 finding 4 fixed elsewhere only) | Now "within 12,000 / 2,064 bytes"; REVIEW5_RESPONSE.md row 4 lists TASKS.md | TASKS.md M05-14; `hardware-r5/rhi-docked-start/window.tsv` `mode` rows |
| 2 | Info | `rhi_gl.cpp` records the new swap interval even when `setSwapInterval` fails, so a failed change is not retried (only logged) | Not changed in M05: no hardware run hit a failed interval change, and any code change would require the Switch proof again. Recorded here as a known, non-blocking behaviour to revisit with the presentation work in M06-20 / M10 | `src/rhi/gl/rhi_gl.cpp` around lines 1124-1132 |

No finding is open. Requested: a fresh independent review of the new candidate.
