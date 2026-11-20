# M08 review 4 (a fresh independent reviewer agent)

Candidate `d33ce2f94c7132e4c03e1188fb63590509d4d556` (NRO `2a02c7b6`). Verdict: **REQUEST_CHANGES**, two low findings.
The reviewer edited no file (host tests only under build/review4-m08-host/).

1. Low - a D-pad direction held at the hand-over still acts in the trainer: `openTrainerMenu` (patch 0012) seeded only
   `g_padWas`; the repeat path `Repeat::fire(pressed, held)` reads `padDown` directly with a stale timer, so a held
   direction fires on the first trainer frame and every 60 ms (Left / Right toggles the row, e.g. God mode on the Player
   page the stack keeps; Up / Down scrolls). Arm the timers or ignore pad input until each button is seen up; check it
   (hold Right on "Trainer..." with the trainer reopened on Player: trainer_used stays 0); reopen M08-08, 09, 15.
2. Low - the line `[trainer] opened from another menu, ... nothing chosen` asserts a result it cannot see (printed
   before any input), and TASKS M08-08 cites it; reword it and cite the `trainer_used=0` / God-mode argument.

Answers to reviews 1-3 checked and held. Checks: candidate built clean (352b29b), runs tied by hash, equivalence diffs
as claimed, patches 0001-0012 apply to ecf150e, the other hand-overs (settings, free camera, close by B / chord / touch)
read without a leak. Tests: validate_project PASS (188), unittest 34 OK, milestone_gate PASS, host tests 8 PASS,
`gh pr checks 9` passing but one pending and milestone-review (expected). Note, not a finding: the trainer's hint line
still names keyboard keys (upstream text).
