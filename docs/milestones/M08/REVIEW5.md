# M08 review 5 (a fresh independent reviewer agent, `m08-review5-ledger`)

Candidate `37fdbcd1a3e328fbd940fc5985b0102c8d9bcc6a` (NRO `c81c7428`). Verdict: **REQUEST_CHANGES**, one low finding,
text only. The reviewer edited no file (scratch output only under build/review5-m08/).

1. Low - the records still credit `trainer-r3` (2a02c7b6) and the review-3 mechanism for the choosing-A case, but
   `352b29b..610817c` replaced that mechanism (`g_padWas[b] = padDown(b)` in `openTrainerMenu`) with `g_padIgnored`,
   which `readInput` applies to both the press edge and the held state. TASKS.md's Candidate NRO paragraph says
   `trainer-r3` "stands" (true for nav-r3, settings-r3, relaunch-r3, reset-r3, not for trainer-r3); M08-08, M08-09
   (which still describes the removed mechanism) and M08-15 cite it, and evidence.json's M08-08 / M08-09 entries quote
   the old 'nothing chosen' line. Fix: drop trainer-r3 from the equivalence list and keep it as history; cite
   `trainer-r4c` for c81c7428 with the argument that Main's selected row was Player, so an A not ignored would have
   opened Player with God mode selected and the held Right (3.6 s, same `readInput` path) would have toggled it and
   raised `trainer_used` (0 in all 50 reports); state that both Player-page cases rest on code reading for the
   candidate NRO.

Checks: milestone_gate PASS; validate_project PASS (188 tasks, 17 milestones); unittest 34 OK (1 skipped);
upstream_overlay PASS 12 patches on ecf150e; core host tests 8/8 PASS at 37fdbcd (clean); `gh pr checks 9`: build and
test checks pass, milestone-review fails for the missing review.json (expected); manifest NRO c81c7428 from 610817c,
root_dirty false, later commits docs only; 120 hardware logs match the transcript's read-back sha256; reviews 1-4
resolved. Notes, not findings: trainer-r4c has no `-m08.txt` / `-results.txt` / RESULT line (the raw logs match their
hashes); `nxrun.cfg` left on the card after the trainer-r4d timeout; a stale `g_padIgnored` after closing the trainer
before release can swallow one later press (harmless; reset by the next `openTrainerMenu`); the trainer's hint line names
keyboard keys (upstream text).
