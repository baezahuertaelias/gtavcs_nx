# M06 — First scene on Switch (beta gate): task record

Branch `milestone/m06-ingame`, cut from main `9a57b92` (M05 merged). Upstream pin unchanged (`ecf150e`). Scope set by
the user on 2026-11-17 ([SCOPE_DECISIONS.md](SCOPE_DECISIONS.md)): handheld only (docked work to M10-13), paired
Joy-Cons only (Pro Controller to M13-11), M06-09's pictures compared without a new desktop run. Hardware results:
[HARDWARE_RESULTS.md](HARDWARE_RESULTS.md); procedure: [HARDWARE.md](HARDWARE.md); beta notes:
[BETA_NOTES.md](BETA_NOTES.md).

## What was built

| Piece | Where |
|---|---|
| Mode-appropriate output on one fixed 1920x1080 window: the picture is drawn at the mode's output size and the layer cropped to it (`nwindowSetCrop`); nothing is reallocated at a mode change | `src/switch/output.{h,c}`, `src/main.c`, `src/platform/platform_switch_sdl2.cpp` (`outputSize`, `glDrawableSize`, `windowSize`, Dear ImGui's display size) |
| GPU time per presented frame: a ring of 8 `GL_TIME_ELAPSED` queries, read without waiting; `PS2X_GL_GPU_TIME=1`, `PS2X_GL_GPU_LOG=<csv>` | `src/rhi/gl/rhi_gl.{h,cpp}` |
| Player 1's physical connection from HID npad style sets (switch-sdl2 never removes a joystick); neutral input when nothing is connected; identity logged; `VCS_NX_INPUT_LOG=1` logs every button / axis change | `src/platform/platform_switch_sdl2.cpp` |
| Heap in use / obtained on the periodic progress line; `PS2X_HLE=0` from nxrun.cfg applied after the run's environment | `src/core/core.cpp` |
| Session tool (empty test card per run, read-back, per-stage median / p95), run configurations | `tools/switch_m06_session.py`, `tests/switch/m06-*.cfg` |
| Private beta package | `tools/package_beta.py`, [BETA_NOTES.md](BETA_NOTES.md) |

## Tasks

| Task | Proof |
|---|---|
| M06-01 | Route: `[gs-hw] renderer ready (NV120 ... Mesa 20.1.0-rc3 ...), scale 1x` and `[hle] world packets ... VU fallback 0`: upstream's presenter, GS, VIF / VU1 and native world submission draw through the OpenGL RHI. Reference path: `PS2X_HLE=0` (the world through the recompiled VU1 programs) did not take effect from nxrun.cfg (static initializer); fixed in `src/core/core.cpp`, then run on the Switch: VU1 programs draw the world through OpenGL, 0 GL errors (reference-handheld). `VCS_RHI=null` stays selectable (M05). |
| M06-02 | Every S1-S4 run reads the disc only through the folder disc (`folder disc 1066336 sectors ... no disc image opened`, `iso_opens=0`); route's `[stream]` lines show the streaming reads (13.6 MB in the first 5 s of S2, then a trickle); every stage's world, models and textures drawn (screenshots, album). The tree on the card was hashed against the manifest in M04 (`verify-files`, problems 0). |
| M06-03 | S2-S4 pictures: opaque world, alpha-tested palms and fences, transparent mission marker, sunset lighting / fog, HUD textures; no missing geometry or wrong colours against M05's desktop record (HARDWARE_RESULTS.md). |
| M06-04 | S3 cutscene and S4: skinned characters animated (the office scene, the player walking and riding), the player's shadow, the bike; no breakage seen. |
| M06-05 | 4:3 picture pillarboxed at 1280x720, nothing cut off (person and screenshots); HUD clock / health / money, radar with north and markers, subtitles and button prompts (album captures); camera follow ped / behind vehicle. |
| M06-06 | `[gs-hw] ... scale 1x`: internal 640x448 in every run, independent of the 1280x720 output (`[video] output 1280x720 on the 1920x1080 surface`). Targets follow the output through the RHI's back-buffer size; docking itself is M10-13. |
| M06-07 | `tests/switch/m06-route.cfg` from an empty test card (the session tool deletes the profile first), Cross every 4 s from 10 s: S4 standing at (-1614.15, -132.33, 14.81) heading 58.0 from 25 s, the same in the silent run (no input) and the desktop reference; `player` every 10 s, `gameinfo` every 30 s, input times in the results. |
| M06-09 | Guest state identical to the desktop reference (HARDWARE_RESULTS.md table); pictures compared by description with M05's desktop record (user decision; no pixel diff claimed). |
| M06-11 | Handheld S2 / S4 median and p95 game, host and GPU frame times, EE phase split, memory (HARDWARE_RESULTS.md, `hardware/*-perf-report.txt`); stock clocks from the album overlay; no FPS target. Docked: M10-13. |
| M06-12 | Soak: heap in use bounded over 10 min of S4 (`hardware/soak-handheld-heap.tsv`); HOME close during play with orderly stop of every worker; seven consecutive launches without hang or crash. |
| M06-13 | Deferred to M10-13 by the user decision of 2026-11-17 (no dock); nothing is claimed for docking. |
| M06-14 | Paired Joy-Cons: initial open (handheld style), disconnect to neutral, reconnect as paired dual, play, orderly HOME exit (run 4); M01's `connected=1` resolved by HID style sets. Pro Controller: M13-11. |
| M06-15 | No FFmpeg in the build (`PS2X_HAS_FFMPEG=0`); `[movie] ...STINGS.PSS;1: built without FFmpeg, skipped` and `INTRO.PSS;1` in every run without `VCS_SKIP_MOVIES`; S1-S4 reached; the `display.skip_intro` setting remains. Limitation in BETA_NOTES.md. |
| M06-16 | SDL physical positions (`a:b1` = south = Cross), Start from Plus (14 presses, pause menu), sticks (run 5), L3 / R3 (run 6), Select from Minus; deliberate exit by HOME close (no Plus-to-exit in the game build). Pro Controller: M13-11. |
| M06-17 | `PS2X_AUDIO=0`: no audio device opened; the S3 cutscene ran ~75-195 s to its end without input and S4 started by 225 s; no audible-sound claim. |
| M06-18 | Run 5: empty test card, NRO and gamefolder only (no ISO on the card), handheld, stock clocks: S1-S3 then 5+ continuous minutes of S4 with the world and HUD visible and the player moving, closed from HOME in order; logs, the person's report and 4 album video clips plus 15 album screenshots (hashes tracked, files private). Docked: M10-13. |
| M06-19 | `python tools/package_beta.py`: the SD layout with the NRO, a placeholder for the user's disc files, BETA_NOTES.md and SHA256SUMS; refuses any other file or a dirty tree; private (the NRO holds code built from the user's disc). |
| M06-20 | Handheld start: `[video] output 1280x720 on the 1920x1080 surface, crop lower-left (00000000)`, the person: upright, 4:3, nothing cut off; 0 GL errors. Docked output and dock changes: M10-13. |
