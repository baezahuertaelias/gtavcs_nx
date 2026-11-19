# M07 review 3 (candidate `40a03c3`): REQUEST_CHANGES, and what was done

Reopened M07-03, 05, 08, 09, 10 (`ff9ca4c`), fixed, rechecked, ticked again. New candidate NRO `e3583aa2` (built from
`8f4f337`); compared with `6976c8d7` (the runs of reviews 1-2) its sources differ only in a comment of patch 0010 and
the documentation patch 0011, so the earlier runs' code is the candidate's code.

| # | Finding | Resolution |
|---|---|---|
| 1 | M07-03 lacks queue depth and latency | From the tracked logs (the ring at each 5 s window's end, after windows 0-1): drive-4 median 40 ms, 23-186 ms; stall-4 median 44 ms, 22-170 ms, at most 37-72 ms in the 15 s after each stall; target 60-129 ms. Latency = ring + SDL's 1024-frame device buffer (21.3 ms): about 61-66 ms at the median, at most about 207 ms; after a resume up to about 221 ms (finding 2's run). TASKS M07-03, evidence.json |
| 2 | No suspend / resume on the candidate; the sleep run's cost hidden | `resume-handheld` on NRO `e3583aa2` (`tests/switch/m07-resume.cfg`; the person was asked for HOME during the opening movie, sleep once walking, close from HOME): suspensions of 7.1 s (STINGS.PSS) and 24.0 s (INTRO.PSS) with the movie's clock resuming where it stopped (7.04 -> 7.08 s, 9.54 -> 10.54 s; the next picture due 0.00-0.03 s ahead after each), 12.2 s on foot in S4, 1.3 s before the close; audio paused / resumed each time; 3 underruns, all in windows 0-2 (the movie windows and a 39 ms hand-over after the person's skip of INTRO.PSS), none after a resume (corrected in review 4); after the on-foot resume the game's catch-up overfills the ring once (41,706 frames, 0.87 s, dropped) and the ring drains from 200 ms (165, 141 ms at the next windows). The earlier sleep run's cost stated (35,948 frames dropped, 175-288 ms for ~40 s). The log does not tell HOME from sleep; stated |
| 3 | M07-08 contradicts itself | "No movie sound after either skip; the skip in movie-sync-handheld-2 is followed by the hand-over underrun" (49 ms) |
| 4 | AGENTS.md "within 0.05 s" | "The picture due 0.00-0.07 s ahead"; the drive figure stated for the revised patch |
| 5 | M07-10 cites drive-handheld-2 unqualified | M07-10 rests on drive-handheld-4; drive-handheld-2 named as superseded; its evidence.json entry removed from M07-10 |
| 6 | "audio closed" misattributed | It is printed by whichever call closes the device first (`ps2AudioShutdown` on the patched builds, `platform::shutdown` before), so it cannot show the order; the code is the proof (TASKS M07-09, evidence.json) |
| 7 | `runIopTime` does not exist | Patch 0010's comment and PS2RECOMP_PATCHES row name `PS2Runtime::advanceIopEeCycles` (series.json sha256 updated); REVIEW1_RESPONSE corrected (REVIEW2.md / REVIEW3.md keep the reviewers' words) |
| 8 | The discovery is not upstream | Root patch 0011 (`0011-music-loop-unused.patch`): docs/game-internals.md's music server entry says 0x90500 is never sent, with the 12 call sites, the scan and the radio log |
| 9 | readfail tool | Each removal reported on its own ("removed X" / "could not remove X"), the first connection closed, the second quit; "mods left=" last. Not re-run on hardware (the recorded run's cleanup succeeded) |
