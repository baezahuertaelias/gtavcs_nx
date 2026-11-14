# M05 hardware session

Console: the user's Switch (NX_HOST, default 192.168.1.91), Sphaira by title override with FTP on port 5000 and
nxlink. The extracted game is already on the card (M03/M04). Evidence goes to `build/evidence/M05/hw/` (private);
tracked summaries to this folder.

Build first (both NROs):

```bash
python tools/build_game.py            # build/m04/private/gtavcs.nro (private) + build/m05/rhi-tests/gtavcs_rhi_tests.nro
python tools/rhi_host_tests.py        # build/m05/host/vulkan_hw: the reference the Switch readbacks are compared with
```

## Order (runs that need the person first)

| # | Command | Person at the Switch | Proves |
|---|---|---|---|
| 0 | `python tools/switch_m05_session.py preflight build/evidence/M05/hw` | Sphaira open, FTP on, auto-sleep off | reachability, no ISO on the card |
| 1 | `... rhi build/evidence/M05/hw --label rhi-handheld --prompt "..."` | handheld; ~3 min of scenes, then the window phase: at 12 s after the card appears, dock, wait 10 s, undock, wait 10 s, dock, undock; check the card: red top-left, green top-right, blue bottom-left, white bottom-right, yellow frame | M05-02..14 scenes on nouveau, M05-09 bench, M05-10 resize, orientation |
| 2 | `... game build/evidence/M05/hw tests/switch/m05-dock.cfg --prompt "..."` | dock / undock every ~20 s during the 180 s run | M05-10/14 in the game: back buffer follows the drawable |
| 3 | `... game build/evidence/M05/hw tests/switch/m05-gl.cfg` | do not touch (240 s) | M05-01 OpenGL default, M05-11 ImGui, picture through the RHI, A timing |
| 4 | `... game build/evidence/M05/hw tests/switch/m05-gl-worker.cfg` | do not touch (240 s) | M05-09 option B in the game |
| 5 | `... game build/evidence/M05/hw tests/switch/m05-null.cfg` | do not touch (60 s) | M05-01 null still selectable |
| 6 | `... rhi build/evidence/M05/hw --label rhi-docked` | docked from the start, do not touch | the conformance scenes in docked mode (GPU clocks differ) |
| 7 | `... cleanup build/evidence/M05/hw` | — | no nxrun.cfg left |

Screenshots of game runs are game pictures: they stay under `build/evidence/M05/screenshots` (ignored); only their
hashes and what they show go into the tracked records. Nothing here claims ingame acceptance (M06).

## Decision rule for M05-09 (user decision 2026-11-11: adopt the correct, faster option)

Correct = `free_threaded/sampled_by_owner` passes and the game runs without GL errors. Faster = lower main-thread
time for the burst and lower worst / median main-thread frame during it (`bench.tsv`), and in the game equal or
better `perf` frame times with no stall lines (`m05-gl` vs `m05-gl-worker`). If B is not both, A (upstream's
record-on-worker / execute-on-main design) stays the default and B stays opt-in, recorded here with the numbers;
if B wins, AGENTS.md's runtime ownership is updated in M05 and B becomes the default.
