# M06 hardware session

Console: the user's Switch at `NX_HOST` 192.168.1.107, handheld only (SCOPE_DECISIONS.md), Sphaira by title override
with FTP on port 5000 and nxlink; Docker Desktop must run on the PC (nxlink runs in the toolchain image). The
extracted game is already on the card (M03). Raw evidence goes to `build/evidence/M06/hw/` (private); the tracked
copies are in [hardware/](hardware/). Game pictures (the runs' screenshots, the Switch album captures) stay private;
only their SHA-256 are tracked.

Build first: `python tools/build_game.py` (build/m04/private/gtavcs.nro).

Every run starts from an empty test card: `tools/switch_m06_session.py game` deletes the run's test profile
(`gamefolder/userdata/tests/<profile>`) before sending the NRO, then reads the log, the run's results, perf.csv /
gpu.csv and the screenshots back. `summary <dir> <label>` writes `<label>-summary.txt` (median / p95 per stage window).

| # | Command | Person at the Switch | Proves |
|---|---|---|---|
| 0 | `python tools/switch_m06_session.py preflight build/evidence/M06/hw` | Sphaira open, FTP on | reachability, no ISO on the card |
| 1 | `... game build/evidence/M06/hw tests/switch/m06-dock.cfg --label output-handheld` | handheld, do not touch; is the picture upright, 4:3, nothing cut off? | M06-20 handheld output on the fixed surface |
| 2 | `... game ... tests/switch/m06-route.cfg --label route-handheld` | do not touch (5 min) | M06-02..07, 09, 11, 15: S1-S4 route, timing, pictures, movies |
| 3 | `... game ... tests/switch/m06-silent.cfg --label silent-handheld` | do not touch (6 min) | M06-17: no audio device, S3 plays out |
| 4 | `... game ... tests/switch/m06-play.cfg --label play-handheld` | play, then detach / disconnect / reconnect the Joy-Cons, close from HOME | M06-14 connection-state matrix |
| 5 | `... game ... tests/switch/m06-play.cfg --label play-handheld-2` | skip S3 with Cross, play 5+ min, + twice, close from HOME | M06-16, M06-18 acceptance |
| 6 | `... game ... tests/switch/m06-sticks.cfg --label sticks-handheld` | click each stick | M06-16 L3 / R3 |
| 7 | `... game ... tests/switch/m06-soak.cfg --label soak-handheld` | do not touch (12 min) | M06-12 heap during play |
| 8 | `... game ... tests/switch/m06-reference.cfg --label reference-handheld` | do not touch (3 min) | M06-01 the VU1 reference path (`PS2X_HLE=0`) |
| 9, 10 | `... game ... tests/switch/m06-wireless.cfg --label wireless-handheld[-2]` | Joy-Cons detached; in S4 walk, turn the camera, + twice | M06-14 / M06-16 wireless |
| 11 | `... game ... tests/switch/m06-soak30.cfg --label soak30-handheld` | do not touch (31 min planned) | M06-12 heap plateau |
| - | `... clean-sd build/evidence/M06/accept` | (with the user's consent) | moves everything but `gtavcs.nro` and the disc files to `/switch/gtavcs_nx_backup/`, records each move |
| 12, 13 | `... game build/evidence/M06/accept tests/switch/m06-play.cfg --label accept-clean[-2]` | (run 12 was told A, wrongly; run 13 B) B (Cross) skips S3; play 7 min; sync buttons, wait, reconnect; close from HOME | M06-18 acceptance on the clean card, M06-14 deliberate disconnect |
| - | (between runs 12 and 13, by hand over FTP) | - | run 12's output moved to `/switch/gtavcs_nx_backup/accept1-*` to clean the card again |
| - | `... restore-sd build/evidence/M06/accept` | - | moves everything back (the run's own output kept beside the backup); the restored tree must equal the tree before cleaning |

Buttons are by position (AGENTS.md): Cross is the bottom button, labelled **B** on the Switch; SDL's names in the
`[input]` lines are positions too (`a` = bottom = B).

Album captures (the Capture button) are read from Sphaira's `album_sd:` FTP mount, `/album_sd:/YYYY/MM/DD/`.
