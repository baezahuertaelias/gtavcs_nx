# M07 review 2 (candidate `f820d89`): REQUEST_CHANGES, and what was done

Reopened M07-01, 03, 04, 05, 07, 08, 09, 10, 11 (`acaf903`), corrected, rechecked against the tracked logs, ticked
again. No code of the game or the runtime changed; no new hardware run.

| # | Finding | Resolution |
|---|---|---|
| 1 | evidence.json kept statements review 1 corrected | Every entry of the reopened tasks rewritten from the logs and TASKS.md: movies-handheld-2 cited only for the device line; Start skipped the movie (one entry, no contradiction); "first mission running" removed; drive-handheld-2 recorded with its 14.3 s starved in window 0 and the 86 % filler as superseded; stall-handheld-2 dropped as proof; M07-03 / 10 / 11 rest on stall-handheld-4 and drive-handheld-4; the movie margins as measured |
| 2 | readfail never reached S4 | TASKS M07-09, evidence.json and REVIEW1_RESPONSE #6: INTRO.PSS whole, then the S3 intro cutscene, still running at the 240 s stop (no input), clean exit. The error-recovery claim does not need S4: the failed movie is passed over and the game goes on |
| 3 | The shutdown log line is no proof | The proof is the code (patch 0010: `runEnd` joins the game thread, then `ps2AudioShutdown` joins the keeper, then `platform::shutdown`); the text says no log line shows the join and that `[platform] audio closed` precedes `[platform] shut down` in every run, before and after the fix; the wrong parenthetical removed |
| 4 | 0x10DC8C sends 0x80000, not 0x4A0000 | Checked in the ELF: the BGEZL at 0x10DC78 has `lui a0,8` in its delay slot and reaches the jal; the `lui a0,0x4a` / `addiu 0x99c0` path branches past it. loop-command.md's row and TASKS M07-05's list corrected; the conclusion (no 0x90500) stands |
| 5 | Not every open ended by a stop | 12 opens across 5 files (3 at position 0), 11 ended by a stop, the last (file 4 at 1,380,779 ms) still playing at exit; the cutscene file 54 with its own stop. TASKS, evidence.json, loop-command.md, REVIEW1_RESPONSE #5 |
| 6 | Movie sync 0.00-0.07 s | Over all movie runs the picture is due 0.00-0.07 s ahead of the sound clock, never behind (0.07 only at a movie's first line); movie-sync-handheld and -2 alone 0.01-0.07 |
| 7 | Wording; readfail tool cleanup | "Windows 0-1" for the boot-movie time plus the hand-over after INTRO.PSS; mission-handheld-2's 70 ms hand-over named in M07-04. `tools/switch_m07_readfail.py`: the folders and the put inside the `try`; each cleanup step failure-tolerant; a leftover mods/ fails the run. The recorded readfail run used the earlier version (its cleanup succeeded: "mods left=False") |

Review 2's other notes, recorded, not changed: the re-base can fire before the runtime forgives (the lead includes the
ring's fill), so the EE may repay up to one target fill (<= 250 ms) twice: bounded, 0 dropped in stall-4 / drive-4; a
fill holds the engine lock for up to ~100 ms of mixing, which can delay a game-thread RPC (not measured).
