# M05 independent review 2: findings and resolutions

Review 2 (a fresh independent agent, read-only) examined candidate `588d5f5` (code `75fd248`) and returned
**REQUEST_CHANGES** with four findings, all in the evidence and claims; it reproduced the rendering results and
confirmed review 1's fixes. Its findings reopened M05-02, 05, 10, 11, 12 and 13. Fixes: `9cb098a` (test changes),
`e2f03c9` (records). Proof: round 3 in [hardware-r2/](hardware-r2/), RHI-tests NRO `e0ba4541…` built from `9cb098a`;
the game NRO is unchanged (`39071ced…`, `75fd248`: no game or backend code changed since).

| # | Severity | Finding (summary) | Resolution | Proof |
|---|---|---|---|---|
| 1 | Medium | The band tolerance passed against ANV but the Switch failed it against lavapipe (anisotropic stdev 8.495 > 8); "agree within 6" was false | The spread was measured between every pair of conformant implementations (ANV, lavapipe, iris, nouveau): means within 1.0, spreads within 8.5. The tolerance is now `bands:8,mean:3,stdev:16` (twice that), the false claims are corrected, and a negative control (the same draw without a mip chain) proves the mip-use check and the band tolerance discriminate: its far band's spread is ~109 against 0-17 with mips, on every implementation (`samplers/uses_mips_negative_control`) | `hardware-r2/band-spread-pairs.txt`; the Switch passes against both references: `rhi-r2*-compare.json` (ANV) and `rhi-r2*-compare-lavapipe.json` (lavapipe, 0 failures; trilinear 1.0 / 5.2, anisotropic 0.44 / 8.5) |
| 2 | Low | "`setViewport` is called only by `beginPass`" was false: `render::UiRenderer` calls it with float sizes; 40.25 is a quarter pixel | Corrected: both callers use origin (0, 0); the UI's size is `DisplaySize x FramebufferScale`, which on the Switch is the drawable exactly (window = drawable, scale 1.0); only an offscreen capture can make its width fractional, its origin staying 0. The x origin is called a quarter pixel; nouveau's viewport-clip rounding is labelled a consistent explanation, not a confirmed one | HARDWARE_RESULTS.md (pixel comparison table); `m05-dock` `[video] window=1920x1080 drawable=1920x1080` |
| 3 | Low | The window readback had no edge on a pixel centre, so it could not show the window's lower-left convention deciding ties as the textures do | The window readback now also draws nine rectangles with every edge on a pixel centre (as GS sprites after the half-pixel shift) and a UI quad at half-pixel positions (as anti-aliased fringes); the texture twin's coverage of the rectangles is checked against D3D's top-left rule, so they are real tie cases | `window/half_pixel_edges_top_left_rule` and `window/back_buffer_equals_texture` pass: 0 differing pixels at 1280x720 (handheld, `rhi-r2`) and 1920x1080 (docked, `rhi-r2-docked`) |
| 4 | Low | Stale or inaccurate records: AGENTS.md's 125 / 99, TASKS' 23 scenes, "every dual-source variant" | Corrected: 127 / 101, 24 scenes, the dual-source images named exactly | AGENTS.md, TASKS.md, HARDWARE_RESULTS.md |

The review also noted that a rebuild of the RHI-tests NRO from the same commit differed from the recorded one only in
the 20-byte build ID (its `switch_main.o` debug line strings differed in length): the NROs are not bit-reproducible.
The recorded hashes are those of the NROs actually sent to the console (the session transcripts hash the local file
and read the copy on the SD card back); code identity of a rebuild is shown by comparing with the build ID masked.
Recorded in HARDWARE_RESULTS.md as a limitation.

No finding is open. Requested: a fresh independent review of the new candidate.
