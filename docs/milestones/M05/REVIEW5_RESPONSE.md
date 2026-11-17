# M05 independent review 5: findings and resolutions

Review 5 (a fresh independent agent, read-only) examined candidate `4f77ef1` and returned **REQUEST_CHANGES** with
three Low and two informational findings, all in the records; it confirmed every finding of reviews 1-4 resolved,
regenerated the round-4 and round-5 readback identity files, and judged the output-size limitation with M06-20 an
honest resolution. M05-01, 09, 10 and 14 were reopened (`2c45b67`). No code or hardware run changed.

| # | Severity | Finding (summary) | Resolution | Proof |
|---|---|---|---|---|
| 1 | Low | AGENTS.md's status attached "no heap growth per change" to the game, whose heap grows between changes | Now: window kept in the game and the test program; no growth per change shown in the test program; the game's heap sampled at the changes only | AGENTS.md "Current verification status"; `hardware-r5/m05-dock-log-excerpt.txt` |
| 2 | Low | `tests/switch/m05-dock.cfg` claimed the heap flat | Comment now says the heap is logged at each change | `tests/switch/m05-dock.cfg` |
| 3 | Low | HARDWARE_RESULTS.md called round 2's game NRO `39071ced` unchanged / current; `m05-gl`, `m05-gl-worker`, `m05-null` ran only on it | Corrected; stated that those runs were not repeated on `af94eee1`, whose only game-code change since `75fd248` is `src/main.c` in `dcda9e1` (window flag, `[video]` heap), which backend selection, the worker sharing check and the null presenter do not depend on; TASKS.md M05-01 / M05-09 rows say so | HARDWARE_RESULTS.md "Round 2 runs"; `git diff --stat 75fd248 dcda9e1 -- src patches CMakeLists.txt cmake` |
| 4 | Info | "within 2 KB" is 2,064 bytes | Byte figure quoted | HARDWARE_RESULTS.md, REVIEW4_RESPONSE.md, evidence.json |
| 5 | Info | Rounds 4-5 have no `*-screenshot-sha256.txt` | Stated that their hashes are in the transcripts' `read-back` lines | HARDWARE_RESULTS.md header |

No finding is open. Requested: a fresh independent review of the new candidate.
