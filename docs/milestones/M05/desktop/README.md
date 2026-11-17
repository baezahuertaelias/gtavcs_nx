# The real game through the OpenGL backend on the host (2026-11-14)

A host check before the Switch session: upstream's desktop build at the pin (`build/m02/reference`, generated from
the user's disc in M02) with the port's OpenGL backend added by the test-only `tests/rhi/desktop-opengl.patch`
(patch 0008 plus an SDL3 GL window / context when `VCS_RHI=opengl`; applied to the reference checkout for this run
and reverted afterwards, never part of the Switch build). The same M05 schedule as the Switch game run
(`tests/switch/m05-gl.cfg`, 240 s from an empty card, Cross presses from 10 s) ran twice through
`python tools/m04_reference.py tests/switch/m05-gl.cfg <dir> --rhi opengl|vulkan`, hidden (`SDL_VIDEODRIVER=offscreen`),
on the host's Intel ADL-N GPU: Mesa iris (OpenGL 4.6 core context asked for as 3.3 core) and Mesa ANV (Vulkan).
Upstream's HW GS renderer (`PS2X_GS=hw`, scale 1x) drew both.

Results:

- OpenGL: `[rhi-stats]` every 5 s (`opengl-renderer-lines.txt`): about 980,000 draws, 42,000 passes and 1,600
  presents; **0 GL errors, 0 KHR_debug messages (synchronous debug output on), 0 incomplete framebuffers, 0
  off-thread calls, 0 refused calls, 0 sampled-target hazards**, 0 deletions pending at each report. Textures rise
  to 1,755 (91 MiB) at 240 s: upstream's content-keyed texture cache, capped at 3,000 entries with LRU eviction
  (`gs_hw.cpp` `kMaxTextures`), not the backend; buffers level off at ~2,940 (8.8 MiB).
- Both runs answered all 84 scheduled control commands; 10 screenshots each (640x448 through `present::capture`,
  i.e. the RHI). They are game pictures and are kept privately (`build/evidence/M05/desktop/*/screenshots`); their
  SHA-256 are in `*-screenshot-sha256.txt`.
- Pictures, compared side by side: the opening flyover at 8 s (sunset street, palms, tower) and Fort Baxter gameplay
  from 60 s to 230 s (world, the player, the HUD clock / health / money, the radar, the blue mission marker) are the
  same in both. The pixel differences (29-93 % of pixels) are the runs' timing: the two runs are not frame-locked,
  so the flyover's camera, a passing truck and the marker's bob are at other phases; no colour, orientation, depth
  or missing-geometry difference was seen. Exact pixel agreement of the backend is the conformance scenes' claim
  (125/125 identical on this GPU), not this run's.

This is not Switch evidence and claims no M06 acceptance: it shows the real renderer's use of the RHI works through
the OpenGL backend before the hardware session.
