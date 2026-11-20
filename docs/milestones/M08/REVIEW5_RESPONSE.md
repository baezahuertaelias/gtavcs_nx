# M08 review 5: findings and answers

M08-08, 09 and 15 were reopened (`225826a`) and their records corrected. No source, patch or run changed: the candidate
NRO stays `c81c7428` (from `610817c`, clean tree).

| # | Finding | Answer |
|---|---|---|
| 1 (low) | `trainer-r3` and review 3's mechanism still credited for the choosing-A case on c81c7428 | TASKS.md's Candidate NRO paragraph drops `trainer-r3` from the runs that stand and says why (it ran review 3's `g_padWas` seeding, which review 4 replaced with `g_padIgnored`); it stays as history only. M08-08 states the c81c7428 mechanism (every button held at the hand-over, the choosing A included, ignored by `readInput` for the press edge and the held state until let go) and cites `trainer-r4c`; the reopened trainer was on Main with Player, row 0, selected (checked in the log: no input during the first trainer visit, the 150646 ms D-pad down was in the developer menu), so an A not ignored would have opened Player with God mode selected and the held Right, through the same path, would have toggled it and raised `trainer_used` (0 in all 50 reports). It says plainly that both Player-page cases rest on that code reading, not on a run on c81c7428. M08-09 no longer describes the removed mechanism; M08-15 cites `trainer-r4c` and the code reading. evidence.json's three `trainer-r3` entries (M08-08, 09, 15) now read as history, and the `trainer-r4c` entries carry the argument. |

The review's notes, not findings, are left as they are: trainer-r4c has no session summary files (the raw logs match
their hashes); `nxrun.cfg` must be removed from the card with `cleanup` before the next ordinary launch.
