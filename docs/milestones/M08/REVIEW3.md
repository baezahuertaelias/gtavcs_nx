# M08 review 3 (a fresh independent reviewer agent)

Candidate `ec97c853987dc141f8fd5bd2922eba0c350344e0` (NRO `63360935`). Verdict: **REQUEST_CHANGES**, one medium finding.
The reviewer edited no file (host tests only under build/review3-m08-host/).

1. Medium - the A press that picks "Trainer..." also chooses the trainer's selected row (dev_menu.cpp:207-212; patch
   0012 `openTrainerMenu`; upstream trainer_menu.cpp:122-131, :554): ImGui activates the button on A's down frame, the
   trainer draws in the same UI callback after the host UI, and its `g_padWas` (updated only while open) still has A up,
   so it reads a new press (`in.select` with the swapped layout). The stack and selection survive a close by L3 + R3, so
   reopening can toggle a cheat unasked and count as trainer use. Seen in functions-handheld-2-gtavcs.txt:182-188 and
   acceptance-final-gtavcs.txt:286-293 (a D-pad Left right after the first open counted as a trainer use). Required:
   seed the trainer's pad state on open (or open on release), add a check (page depth or a log line), rerun on
   hardware; reopen M08-08, 09, 15.

Answers to reviews 1 and 2 checked and held. Other checks: candidate build clean (b2b4b1c), runs on 63360935 tied by
the transcript, the equivalence diffs as claimed, settings runs, patch 0012 hash and markers, code read for chord /
touch / output / core wiring. Tests: validate_project PASS (188), unittest 34 OK, milestone_gate PASS, host tests all
PASS; `gh pr checks 9` partly pending, milestone-review failing as expected. Limitations: no hardware reproduction; the
trainer leak inferred from code and four logs (no log named the trainer's page); no own NRO build.
