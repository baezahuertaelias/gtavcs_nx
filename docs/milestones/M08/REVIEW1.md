# M08 review 1 (independent reviewer agent)

Candidate `f01d40d2450c59227a41afb2b5d4aaae525df027` (NRO `f9aff8f1`). Verdict: **REQUEST_CHANGES**. The reviewer edited
no file (host tests written only under build/review-m08-host/).

Findings, as reported:

1. Medium - M08-14 free-aim proof is absent (TASKS.md:48, :50; evidence.json:109, :113; AGENTS.md:311). The L3 presses
   reached only the platform snapshot; the game's pad read around them shows R1 only (`buttons=0800` at t=127.135 and
   t=129.025; `0804` at t=131.089 is R1 + R3); the only game state is `gameinfo` t=130 lock-on, which R1 alone produces.
   Rerun with game-side proof (aimstate or the game's free-aim state) or correct the claims.
2. Medium - M08-15 frame-time recording not driven from the controller on the candidate (by touch in layout1080-f9, by
   the control channel in settings-f9; controller start / stop only on 022522af). Rerun or cite 022522af with a
   code-equivalence argument.
3. Low - the chord filter can toggle after focus returns: after `reset()` with L3 + R3 still held, both become Withheld
   in one update and the chord fires; a single held button is re-withheld and delivered again (chord.cpp:96-116,
   filterSticks). Ignore buttons until seen up after a reset; add a host test.
4. Low - stage claims inferred or thin: no gameinfo between t=70 and 130 in acceptance-f9; S2 had a single pair.
5. Low - committed host-test logs from a dirty tree (`dirty=True`).
6. Low - nothing makes a save test read the trainer marker; a failed marker fopen is silent.
7. Low - imprecise or stale text: `closed by B` on f9 was the settings menu's; AGENTS.md:248 "No ImGui implementation
   or menu"; folder tree without src/devmenu; m08-layout1080.cfg "M10-13" / "Circle"; m08-functions.cfg "Logged as
   m08-chord"; the session tools write ignored `*-gtavcs.log`.

Checks that held: the candidate NRO built clean (root_commit 7e2881c, root_dirty false); after it only records and one
run-file line changed; every f9 run names sha256 f9aff8f1 and its logs match the read-back hashes; acceptance-f9: 60
`chord after` lines, 0 without `game sees L3=0 R3=0`; lone presses delivered 103.2-185.3 ms; HOME with the menu open
(sleep not tested); settings damaged / relaunch / reset as claimed; touch mapping 720p and 1080p; gl errors 0; frame order
ps2_runtime.cpp:3229-3330; one ImGui context; patch 0012 markers and PS2RECOMP_PATCHES row. Tests: validate_project PASS
(186 tasks, 17 milestones); unittest 34 OK (1 skipped); milestone_gate M08 PASS; run_core_host_tests (clean tree) all 8
builds PASS. Limitations: no hardware reproduction, screenshots not viewed, the USB-mouse path unproven, no own NRO build,
CI's --public not checked.
