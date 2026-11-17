# M05 handoff: where the milestone stands and what to do next

Read this first when resuming M05. Branch `milestone/m05-opengl-rhi` (pushed). The ledger is docs/MILESTONES.md; the
workflow is docs/AUTOMATED_EXECUTION.md; the evidence so far is in this folder.

## State (2026-11-16)

- The OpenGL backend, the GLSL ports and the conformance tests are done and proven on the Switch: 127/127 readbacks
  within tolerance of upstream's Vulkan backend (101 byte-identical), handheld and docked, against two references
  ([HARDWARE_RESULTS.md](HARDWARE_RESULTS.md), rounds 1-3).
- Three independent reviews so far, all REQUEST_CHANGES:
  - review 1 (`cb70b74`), 7 findings, resolved: [REVIEW1_RESPONSE.md](REVIEW1_RESPONSE.md);
  - review 2 (`588d5f5`), 4 findings, resolved: [REVIEW2_RESPONSE.md](REVIEW2_RESPONSE.md);
  - **review 3 (`bc9111e`), 2 findings, OPEN** (below). M05-10 and M05-14 are unchecked again in MILESTONES.md.

## Review 3 findings (open)

1. **High: heap grows at every window size change** (docking, or `SDL_SetWindowSize`); the records claimed no
   unbounded growth (M05-14) and cited the run that shows it (M05-10).
2. **Low: record errors** in HARDWARE_RESULTS.md: the two game fps figures are swapped (240 s `m05-gl` = 21.3,
   180 s `m05-dock` = 18.6); "Two rounds" (there are three); TASKS.md's task table still says "round 2 unless noted"
   and cites review 1 only; AGENTS.md's "Current verification status (2026-11-14)" holds 2026-11-16 facts.

## What the leak investigation found ([hardware-r3/rhi-leak](hardware-r3/rhi-leak), `window.tsv` `resize` rows)

The resize memory probe (`resizeProbe` in src/rhi_tests/switch_main.cpp), 10 cycles each, heap from newlib `mallinfo`:

| Variant | Heap leaked per `SDL_SetWindowSize` |
|---|---|
| A: to 1920x1080 / to 1280x720, context current | +44.1 MB / +5.4 MB |
| B: the same with the GL context released around the resize | identical to A (no help) |
| C: to the **same** size (1280x720 to 1280x720) | +15.2 MB |

So every SDL2 window resize on the Switch leaks the window surface's buffers inside switch-sdl2 / Mesa (not the RHI:
its own objects return to zero). A dock change makes SDL resize a resizable window itself, so the game leaks on
every dock / undock (the 3-minute `m05-dock` run ended at 623 MB of heap, the 4-minute `m05-gl` run at 471 MB).

## Next steps

1. **Run the pending test** (built: RHI-tests NRO `a5b70866…`, commit `21377d2`; the send at 18:59 failed because
   Sphaira was not reachable, nothing ran): Switch in Sphaira with FTP on, **start handheld**, then
   `python tools/switch_m05_session.py rhi build/evidence/M05/hw6 --label rhi-fixedwin`. When the card appears:
   dock ~10 s, undock ~20 s, dock ~30 s, undock ~40 s. This build makes the window non-resizable
   (`SDL_SetWindowResizable(false)`) before the docking stretch and logs the heap at every mode change and every
   second (`window.tsv`). Ask the person what the handheld screen shows after undocking (full-screen, sharp?).
2. **If SDL keeps the non-resizable window's size and the heap stays flat across the dock changes:** change
   src/main.c to create the window without `SDL_WINDOW_RESIZABLE` at one fixed size (decide 1920x1080 or 1280x720 from
   the run: the compositor scales the picture to the panel / TV), update AGENTS.md's runtime-ownership text (it says
   the backend resizes a resizable window at dock changes), rebuild (`python tools/build_game.py`), and rerun the game
   dock run (`tests/switch/m05-dock.cfg`: start docked; undock ~40 s, dock ~70 s, undock ~100 s, dock ~130 s) with
   the heap checked flat across the changes (add the heap to main.c's `[video]` change line).
   **If SDL still resizes or the heap still grows:** look for another way (a fixed-size surface through SDL hints,
   or a patched switch-sdl2 in the locked toolchain); do not close M05-10 / M05-14 until the growth is bounded.
3. Fix review 3's record errors (finding 2) and record the leak, its cause and the fix in HARDWARE_RESULTS.md, a
   REVIEW3_RESPONSE.md, TASKS.md and evidence.json; re-tick M05-10 and M05-14 only with the new evidence.
4. `python tools/milestone_gate.py M05`, then a **fresh** independent reviewer on the new candidate (the user
   authorizes it; give it the review 1-3 response files).
5. On approval: record `review.json`, `python tools/milestone_gate.py M05 --require-review`, open the PR (like #5),
   wait for green CI, merge with a merge commit, then cut `milestone/m06-ingame` from the merged main.

## Console notes

Switch at 192.168.1.91, Sphaira by title override, FTP port 5000 + nxlink. It drops off the network when idle;
check port 5000 first. The person may not be able to watch the TV: the logs carry the docking evidence.
