# M05 independent review 3: findings and resolutions

Review 3 (a fresh independent agent, read-only) examined candidate `bc9111e` and returned **REQUEST_CHANGES** with two
findings. Its first finding reopened M05-10 and M05-14. Fix: `dcda9e1` (code). Proof: round 4 in
[hardware-r4/](hardware-r4/) (RHI-tests NRO `58c458b0…`, game NRO `af94eee1…`, both built from `dcda9e1`); the
measurement that located the leak is [hardware-r3/rhi-leak](hardware-r3/rhi-leak) (`6f3f877`).

| # | Severity | Finding (summary) | Resolution | Proof |
|---|---|---|---|---|
| 1 | High | The heap grows at every window size change (a dock change, or `SDL_SetWindowSize`); the records claimed no unbounded growth (M05-14) and cited the run that shows it (M05-10) | Measured: every SDL2 resize on the Switch leaks the old EGL surface's buffers inside switch-sdl2 / Mesa (+44.1 MB to 1920x1080, +5.4 MB to 1280x720, +15.2 MB at the same size; the RHI's objects return to zero). Cause in switch-sdl2 2.28: `SWITCH_PumpEvents` resizes a window with `SDL_WINDOW_RESIZABLE` at every operation-mode change and `SWITCH_SetWindowSize` recreates the surface each time. `SDL_SetWindowResizable(false)` does not help (no backend hook; `rhi-fixedwin`). The game and the test program now create the window at the starting mode's size without the flag; the game never resizes it (the test program still does in its probes); the compositor scales it. AGENTS.md's runtime-ownership rule changed accordingly | `rhi-noresizable`: 7 dock changes by hand, window 1280x720 at each, heap within 12 KB across them (corrected by review 4); `rhi-noresizable-docked2`: docked start, 1920x1080, heap flat (stayed docked; docked start with undocking: round 5, [REVIEW4_RESPONSE.md](REVIEW4_RESPONSE.md)); `m05-dock`: the game keeps 1280x720 through four changes, 0 GL errors, clean stop; the user saw the picture full-screen and sharp on the TV and the handheld screen. HARDWARE_RESULTS.md, round 4 |
| 2 | Low | Record errors: the two game fps figures swapped; "Two rounds" (three); TASKS.md's "round 2 unless noted" and review-1-only citation; AGENTS.md's status date holding later facts | Corrected: 240 s `m05-gl` `game_fps=21.3 frame_ms=47.0`, 180 s `m05-dock` `18.6 / 53.7` (from `hardware-r1/*-results-excerpt.txt`); the round count (now four); TASKS.md cites rounds 2-4 and reviews 1-3; AGENTS.md's status dated 2026-11-16 | HARDWARE_RESULTS.md, TASKS.md, AGENTS.md |

**Limitation.** The host Vulkan reference was not rebuilt on the round-4 host, so the round-4 RHI readbacks were not
compared with it directly; all 127 of every round-4 RHI run are byte-identical to round 3's, which passed against ANV
and lavapipe ([hardware-r4/readbacks-vs-round3.txt](hardware-r4/readbacks-vs-round3.txt)).

No finding is open. Requested: a fresh independent review of the new candidate.
