# M07 — Real sound, music, movies and synchronized streaming: task record

Branch `milestone/m07-audio-movies`, cut from main `0aa435e` (M06 merged). Upstream pin unchanged (`ecf150e`); root
patches 0001-0009. Scope: [SCOPE_DECISIONS.md](SCOPE_DECISIONS.md) (handheld only; docked to M10-13). Hardware records:
[hardware/](hardware/) (`person-observations.txt`: what the user saw and heard).

## What was built

| Piece | Where |
|---|---|
| Movies: the locked image's switch-ffmpeg 7.1 (static, `--target-os=horizon`, `--enable-gpl`; the project is GPL) linked; `PS2X_HAS_FFMPEG=1` in every library holding runtime or game code, FFmpeg's include folder on nx_core (vcs_movie.cpp keys on `__has_include`) | `CMakeLists.txt`, [ffmpeg-dependency.txt](ffmpeg-dependency.txt) |
| Optional homebrew-menu icon: the ignored root `icon.jpg` (256x256 JPEG) when present, its hash in the build manifest; never committed | `CMakeLists.txt` (`NX_GAME_ICON`), `tools/build_game.py`, `.gitignore` |
| Run configuration for the boot movies | `tests/switch/m07-movies.cfg` |

## Tasks (in progress)

| Task | Status | Proof so far |
|---|---|---|
| M07-01 | proof gathered | No silence stub in `src/` (no `SDL_QueueAudio`, no silence queue: the scaffold's is gone since M04). The native sound driver's SPU2 output goes through upstream's bounded producer ring (`ps2_audio_out.cpp`: 1 s ring, adaptive 60-250 ms target fill, overflow dropped and counted) to SDL2's callback device (`src/platform/platform_switch_sdl2.cpp` `audioOpen`, allowed changes 0: SDL converts if audren wants another format, the callback always sees S16 stereo). On the Switch: `[platform] audio device 2: 48000 Hz, 2 channels, 1024 frames (switch)`, upstream's rate, so nothing to convert; `[audio] produced 100% of real time; ... underruns 0` after the movies (`hardware/movies-handheld-2-gtavcs.txt`). |
| M07-02 | open | PCM recording exists upstream (`PS2X_AUDIO_DUMP=<wav>`, written on the producer thread, not in the callback); the Switch / desktop comparison is still to run. |
| M07-03 | open | |
| M07-04 | open | |
| M07-05 | open | |
| M07-06 | proof gathered | switch-ffmpeg 7.1-5 and switch-dav1d 1.5.0-1 are in the M01 package lock (`docs/milestones/M01/package-lock.txt`), installed in the digest-pinned image the build checks against the lock; static, `--target-os=horizon`, `--enable-gpl` (GPL like this project and upstream; dav1d BSD); libavcodec 61.19.100, libavutil 59.39.100, libswscale 8.3.100, the 7.1 line upstream's desktop pins (`n7.1`); no host FFmpeg ([ffmpeg-dependency.txt](ffmpeg-dependency.txt)). The game map holds the decoders (1,095 references to `avcodec_send_packet`, `sws_scale`, `dav1d_`). |
| M07-07 | partly | Run 2 (`hardware/movies-handheld-2-*`, NRO `19bd154c` from `5c13f7d`): STINGS.PSS 321 pictures, 0 dropped, 10.8 s; INTRO.PSS whole, 2,527 pictures, 0 dropped, 84.3 s; then S3 / S4; 0 GL errors; the user: "Everything runs good" (run 1). Left: a sync measurement, the movie's memory, in-engine cutscenes. |
| M07-08 | partly | Run 1 closed from HOME 60.8 s into INTRO.PSS: orderly stop. Left: deliberate skip / HOME / sleep / close during a movie, no stale audio after. |
| M07-09 | open | |
| M07-10 | open | |
| M07-11 | open (handheld) | |
