# M05 independent review 4: findings and resolutions

Review 4 (a fresh independent agent, read-only) examined candidate `6529c15` and returned **REQUEST_CHANGES**. It
confirmed the leak fix and its evidence (the readback identity with round 3 regenerated, the switch-sdl2 source checked
at the `switch-sdl-2.28` branch tip) but found the docked-start path unproven and the records overclaiming. M05-10 and
M05-14 were reopened (`8cca0f0`). No code changed: the proof is round 5 in [hardware-r5/](hardware-r5/) with the
round-4 NROs (`58c458b0…`, `af94eee1…`, both from `dcda9e1`).

| # | Severity | Finding (summary) | Resolution | Proof |
|---|---|---|---|---|
| 1 | Medium | A session started docked and then undocked was never run (round 4's docked start stayed docked; the game ran only from handheld), yet the records stated the result for every dock change | Ran it: the test program and the game, both started docked, four dock changes each | `rhi-docked-start`: window and drawable 1920x1080 at every change, heap within 2 KB across them, 0 failed checks; `m05-dock` (round 5): 1920x1080 at four changes, 0 GL errors, orderly stop; the user: full-screen and sharp on the handheld screen, 4:3 pillarboxed as designed |
| 2 | Medium | The output-size trade-off (a handheld start upscales 720p on the TV; a docked start renders 1080p and downscales it on the panel) conflicts with the 720p / 1080p output targets and had no owner | Recorded as a limitation in AGENTS.md and HARDWARE_RESULTS.md; new task **M06-20** owns mode-appropriate output without the leak (leak-free surface recreation or a fixed drawable with a mode-appropriate render scale), and M06-13 depends on it | MILESTONES.md M06-20; HARDWARE_RESULTS.md "Output size" |
| 3 | Low | `rhi-noresizable` had 7 hand dock changes at 1280x720 plus 1 at 960x540 after the program's resizes, not 9 at 1280x720; heap bound 554,122,688 | Corrected in HARDWARE_RESULTS.md, REVIEW3_RESPONSE.md and evidence.json | `hardware-r4/rhi-noresizable/window.tsv` `mode` rows |
| 4 | Low | The test program does call `SDL_SetWindowSize` (probes, 55-64 s) | Corrected: only the game never calls it; the test program's later heap figures include its probe leaks (148.6 + 148.6 + 91.4 MB) | `src/rhi_tests/switch_main.cpp`; HARDWARE_RESULTS.md round 4 |
| 5 | Low | `game_fps=21.4 frame_ms=46.8` is the t=180.0 s line, not 176 s | Corrected | `hardware-r4/m05-dock-log-excerpt.txt` |
| 6 | Low | The game heap statements reach beyond the data (first interval is the game loading; no sample after ~130 s; the shutdown figure is logged before the context is deleted) | Restated as inference with both rounds' samples; "no growth per dock change" rests on the test program and the churn budget | HARDWARE_RESULTS.md "Heap in the game" |
| 7 | Info | The compare step's failure reason was not tracked | The compare outputs are now tracked (`hardware-r4/*-compare.txt`, `hardware-r5/rhi-docked-start-compare.txt`: `FileNotFoundError … vulkan_hw/index.tsv`) | those files |

**Limitation (unchanged).** The host Vulkan reference was not rebuilt on this host; all 127 readbacks of the round-5
RHI run are byte-identical to round 3's ([hardware-r5/readbacks-vs-round3.txt](hardware-r5/readbacks-vs-round3.txt)).

No finding is open. Requested: a fresh independent review of the new candidate.
