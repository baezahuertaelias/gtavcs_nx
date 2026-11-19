# M08 review 1: findings and answers

Review 1 (independent reviewer agent, candidate `f01d40d`, NRO `f9aff8f1`): **REQUEST_CHANGES**, seven findings. The
reviewer's checks that held: the candidate NRO and its clean build, every f9 run's NRO hash and read-back hashes, 0 chord
inputs reaching the game, the settings runs, touch, `gl errors 0`, frame order, one ImGui context, patch 0012's markers;
tests run: validate_project PASS, unittest 34 OK, milestone_gate PASS, host tests all PASS on a clean tree.

All ten tasks were reopened in the ledger (`06be6c0`, recorded after the fixes were committed) and proven again on the new candidate NRO `6b62dca7` (commit
`22bc86b`); see [TASKS.md](TASKS.md).

| # | Finding | Answer |
|---|---|---|
| 1 (medium) | M08-14's free-aim proof was only the platform snapshot (`game sees L3=1`); the game's pad read showed R1 only, and lock-on (mode 11) is also R1 alone | The developer menu now reads the game's own free-aim flag (player ped +0xCCC bit 19, game-internals.md 12c, as `vcs_aim.cpp` does) every host frame and logs each change (`noteGameFreeAim`, `game_free_aim` in the reports). `final-r1`: `L3 delivered after 104.6 ms: game sees L3=1` then `[devmenu] the game's free aim on (player ped 01d439c0, flags +0xCCC bit 19)`. AGENTS.md's "free aim works" now rests on that line. |
| 2 (medium) | M08-15's recording was not started / stopped from the controller on the candidate | `final-r1`: started and stopped with the A button (`[input] ... button b down` -> `chose: recording start`, again -> `chose: recording stop`, the CSV saved and read back). |
| 3 (low) | After a focus loss, L3 + R3 still held toggled the menu at the first live pump; a single held button was handed on again | `ChordFilter::reset` now puts both buttons in an Ignored state left only once the button is seen up; `filterSticks` resets and reports nothing while the pad is not live, without an update that would clear it. Host tests added (both held, one held, a new chord after release): 40 PASS lines each in `host-tests/chord-*.txt`. |
| 4 (low) | Stage claims inferred: no `gameinfo` between t=70 and 130; S2 had one pair | `m08-final.cfg` queries the camera mode every 5 s for 3 minutes. `final-r1`: 2 pairs in the flyover (mode 15, t=7-32), 1 in the cutscene (mode 17, t=37-47), the rest in play; TASKS.md gives the counts and times. |
| 5 (low) | Host-test logs made from a dirty tree | Regenerated from `22bc86b`, `dirty=False`, all PASS. |
| 6 (low) | Nothing makes a save test read the trainer marker; a failed marker write was silent | New ledger task M14-09 (every save test checks `trainer-used.txt` and reports a save made after trainer use as not a normal-release result); a failed write is logged (`could not write ... trainer-used.txt`). |
| 7 (low) | Imprecise or stale text; `.log` names the next runs would hit | TASKS.md rewritten for the new candidate (`closed by B` attributed correctly, rows chosen with A); AGENTS.md's ImGui line and folder tree updated; `m08-layout1080.cfg` (M16-01, B) and `m08-functions.cfg` comments corrected; `switch_m08_session.py` renames the run's logs to `.txt` itself. |

Also run on `6b62dca7`: `settings-r1`, `relaunch-r1`, `reset-r1` (the settings proof again), and `final-r1` with HOME and
sleep, each with the menu open. Not rerun: the 1080p layout runs (on `f9aff8f1`); `git diff 7e2881c 22bc86b -- src
CMakeLists.txt patches` shows no change to the layout, the settings menu, the touch path or the output.
