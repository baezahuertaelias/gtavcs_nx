# M08 review 2 (a fresh independent reviewer agent)

Candidate `23092917e82131c768c3e6d5fc75f2ed482e7960` (NRO `6b62dca7`). Verdict: **REQUEST_CHANGES**, four low findings.
The reviewer edited no file (host tests only under build/review2-m08-host/).

1. Low - M08-17's mouse half not verified on hardware and not deferred by a user decision (TASKS.md:60,
   platform_switch_sdl2.cpp:239-243); `toUiCoordinates` assumes switch-sdl2 reports panel-scaled mouse positions; if it
   reports surface pixels the mapping is the crop's offset without scaling. Verify, or record the user's decision, and
   state the assumption in the code.
2. Low - on the candidate the performance overlay was reached only by touch (final-r1), not the controller (M08-15);
   controller proof exists on f9aff8f1 (acceptance-f9-gtavcs.txt:776-778) but is not cited.
3. Low - `ImGuiConfigFlags_NavEnableGamepad` stays on after the developer menu hands over to the trainer
   (dev_menu.cpp:76-85, :210, :395); the trainer never clears it: leftover UI state.
4. Low - the trainer marker path in TASKS.md:18 is wrong (`userdata/tests/<profile>/` for a test profile).

Review 1's answers were checked and held (free aim, recording by buttons, the chord after focus loss, stages, clean
host logs, M14-09, text). Checks: the candidate built clean from 22bc86b; later commits change no code; every hash
matches its read-back; final-r1 30 `chord after` lines, 0 leaking; gl errors 0; the 1080p equivalence argument holds;
settings runs as claimed; patch 0012 conforms. Tests: validate_project PASS (187 tasks), unittest 34 OK, milestone_gate
PASS, host tests 8 PASS (clean tree), `gh pr checks 9`: cross-build, public-cpu-runtime, foundation, GitGuardian pass,
milestone-review failing as expected before review.json. Limitations: no hardware reproduction; no user statement on
720p readability (accepted from the person reaching and touching the rows); HOME vs sleep rests on the person; the
free-aim flag read is a benign data race (diagnostic only); no own NRO build.
