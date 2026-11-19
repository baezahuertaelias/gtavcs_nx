# M08 review 2: findings and answers

M08-08, 09, 15 and 17 were reopened (`a247885`) and proven again. New
candidate NRO `63360935` from `b2b4b1c` (clean tree). `git diff 22bc86b b2b4b1c -- src CMakeLists.txt patches`: only
`dev_menu.cpp` (the trainer hand-over turns navigation off, `nav=` in the reports) and `platform_switch_sdl2.cpp`
(navigation no longer turned on at init; the mouse comment). So `final-r1` (6b62dca7) stands for everything else; run on
63360935: `nav-r2b`, `settings-r2b`, `relaunch-r2b`, `reset-r2b` (automatic) and `navhand-r2` (the person).

| # | Finding | Answer |
|---|---|---|
| 1 (low) | Mouse half of M08-17 unverified, not deferred | The user chose "Move it to M16" (SCOPE_DECISIONS.md); new task M16-04 verifies a USB mouse on hardware and fixes the mapping if needed; M08-17's ledger text names it. `toUiCoordinates`' comment now states the assumption (panel-scaled positions) and the alternative (surface pixels: the crop's offset without scaling), marked unverified. |
| 2 (low) | Overlay on the candidate only by touch | `navhand-r2` (63360935): D-pad down to the row, A: `[input] ... button dpdown down` (x4) -> `chose: performance overlay on` -> `closed by B`. Also cited: `acceptance-f9` (f9aff8f1) `[input] ... button b down` -> `chose: performance overlay on`. |
| 3 (low) | Navigation flag left on after the hand-over to the trainer | The trainer hand-over now closes the developer menu without the "other menu" exception (the trainer reads the pad itself); the reports carry `nav=`. `nav-r2` (d37a0adb) showed the flag on from boot too (the platform layer set it at init), so init no longer sets it. `nav-r2b` (63360935): `nav=0` at boot, 1 with the developer menu open, 0 with the trainer open, 0 after close, 1 with the settings menu open, 0 after. |
| 4 (low) | Marker path | TASKS.md and M14-09 now name `userdata/trainer-used.txt`, or `userdata/tests/<profile>/trainer-used.txt` for a test profile. |
