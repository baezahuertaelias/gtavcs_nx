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
| M06-05 | 4:3 picture pillarboxed at 1280x720, nothing cut off (the person: "It looks good and it has 4:3 ... nothing cut off"; screenshots); HUD clock / health / money, radar with north and markers, subtitles and button prompts (album captures); camera follow ped / behind vehicle. |
| M06-06 | `[gs-hw] ... scale 1x`: internal 640x448 in every run, independent of the 1280x720 output (`[video] output 1280x720 on the 1920x1080 surface`). The clause "recreate targets and camera state on docking" falls under the user's dock decision (SCOPE_DECISIONS.md): moved to M10-13, nothing claimed for it. |
| M06-07 | `tests/switch/m06-route.cfg` from an empty test card (the session tool deletes the profile first), Cross every 4 s from 10 s: S4 standing at (-1614.15, -132.33, 14.81) heading 58.0 from 25 s, the same in the silent run (no input) and the desktop reference; `player` every 10 s, `gameinfo` every 30 s, input times in the results. |
| M06-09 | Guest state identical to the desktop reference (HARDWARE_RESULTS.md table); pictures compared by description with M05's desktop record (user decision; no pixel diff claimed). |
| M06-11 | Handheld S2, S3 and S4 median and p95 game, host and GPU present-to-present times cut at each run's own stage changes (HARDWARE_RESULTS.md, Timing; `hardware/*-summary.txt`); the GPU metric's meaning stated (an upper bound of GPU busy time); EE phase split; memory; stock clocks from the album overlay; no FPS target. Docked: M10-13. |
| M06-12 | Moving play with streaming (review 3): run 15 (`tests/switch/m06-drive.cfg`, a car on upstream's city route at 40 m/s for 21 minutes) found a leak, heap in use +15.6 MiB/min to the end; the desktop reference showed it too (+33 MB/min): upstream's RSL recording cache, keyed by guest address, never pruned. Patch 0009 prunes it (desktop +1.4 MB/min). Run 16 on the patched NRO `9b89481c`: heap in use levels off at about 830-845 MiB from 480 s, -0.5 MiB/min over 700-1,260 s (`hardware/drive-handheld-2-heap.tsv`); heap_obtained's slowing growth (fragmentation) recorded for M10-07. Standing: run 11 levels off at about 600 MiB (`hardware/soak30-handheld-heap.tsv`). HOME close during play with every worker stopped in order; eighteen launches, each clean, no crash report, no hang. |
| M06-13 | Deferred to M10-13 by the user decision of 2026-11-17 (no dock); nothing is claimed for docking. |
| M06-14 | Paired Joy-Cons: initial open attached (handheld style) and detached (paired dual); sliding them off the rails (run 4: none for at most 6.6 s, then dual); a deliberate wireless disconnect with the sync buttons (run 13: dual, then none with input neutral and the player standing, then dual again, play continuing); orderly HOME exit; wireless play (runs 10, 13). M01's `connected=1` resolved by HID style sets. Pro Controller: M13-11. |
| M06-15 | No FFmpeg in the build (`PS2X_HAS_FFMPEG=0`); `[movie] ...STINGS.PSS;1: built without FFmpeg, skipped` and `INTRO.PSS;1` in every run without `VCS_SKIP_MOVIES`; S1-S4 reached; the `display.skip_intro` setting remains. Limitation in BETA_NOTES.md. |
| M06-16 | SDL physical positions (`a:b1` = south = Cross). Attached: Start from Plus pressed 14 times, sticks, A, Select from Minus (run 5), L3 / R3 (run 6). Wireless (run 10): in S4 the left and right stick and two Start presses while the player walks (the A presses were during the cutscene skip; SDL event times are about 2.1 s ahead of the run's clock). The log records SDL's events; that Start opened the pause menu is not logged. Deliberate exit by HOME close (no Plus-to-exit in the game build). Pro Controller: M13-11. |
| M06-17 | `PS2X_AUDIO=0`: no audio device opened; the S3 cutscene ran from before 60 s to after 195 s without input and S4 started by 225 s; no audible-sound claim. |
| M06-18 | Run 17 on the shipped NRO `9b89481c` (with patch 0009), on a card cleaned again to the NRO and the disc files (clean and restored trees, the moves listed; the user's consent), an empty test card, handheld, application mode (`mapped_total=3343908864`): S1-S3, then about 6.0 continuous minutes of S4 from 30 s to the HOME close at 390.4 s, on foot and driving, 0 GL errors, orderly stop, no crash report (HARDWARE_RESULTS.md, "on the shipped NRO"). Run 13 (the same on NRO `9a9f7e12`, about 7 minutes), run 5 and run 12 are supporting runs. Video: run 5's two album clips by the user's decision (SCOPE_DECISIONS.md). Docked: M10-13. |
| M06-19 | `python tools/package_beta.py`: the SD layout with the NRO, a placeholder for the user's disc files, BETA_NOTES.md and SHA256SUMS; refuses any other file, a dirty tree and an NRO whose build manifest names another commit or a dirty tree; private (the NRO holds code built from the user's disc). |
| M06-20 | Handheld start: `[video] output 1280x720 on the 1920x1080 surface, crop lower-left (00000000)`; the person: "It looks good and it has 4:3 ... nothing cut off"; the album captures show the picture the right way up; 0 GL errors. Docked output and dock changes: M10-13. |

## Known issue carried forward

Dear ImGui's display size is the output (1280x720 in handheld) while the SDL2 backend reports touch and mouse
positions in the 1920x1080 window's coordinates (`platform_switch_sdl2.cpp`, `beginFrame`). No menu is in this beta;
M08-17 owns the fix.
