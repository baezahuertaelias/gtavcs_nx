# M07 review 3 (candidate `40a03c3`): the reviewer's report

Reviewer id: m07-review3-flint (a fresh independent agent; no tracked file edited, no console touched). Verdict:
**REQUEST_CHANGES**. Condensed in layout; the response is REVIEW3_RESPONSE.md.

## Checks the reviewer ran

milestone_gate.py M07 PASS; validate_project.py PASS (183 tasks); test_milestone_gate.py 13 OK; test_upstream_overlay.py
4 OK; test_glsl_shaders.py 3 OK, 1 skipped; overlay into a disposable build/ folder: 10 patches, 0010 b88b1da7...
matches series.json (removed after). WAV hashes match; pcm_compare.py reproduces pcm-compare-2.txt. Audio stats
recomputed for all 20 runs (drive-4 3 underruns in windows 0-1, 0 after, 0 dropped, mean filled 51.5 %; stall-4 0 after
windows 0-1; drive-3 21 after boot, 472,879 frames; drive-1 169, 42.1 s dropped; hand-overs 3,375 / 2,339 frames;
pre-patch per-stall counts match; listen-handheld 0 underruns but 76,927 frames dropped, 35,948 after the sleep).
Movies 0.00-0.07 s, never negative; 321 / 2,527 pictures; heap 315.4 / 369-370 / 322.7 MiB; skip inputs and HOME clock
as recorded. Voices 32 / 26 / 29 / 12; radio 12 opens (3 at 0), 12 stops (one for file 54), no 0x905xx; noaudio and
readfail as recorded. Own stdlib ELF scan: the same 12 call sites; 0x10DC8C sends 0x80000. NRO 6976c8d7 matches the
candidate (manifest overlay_series_sha256 085c59f1... = series.json; no src / CMake change since its root commit).

Patch 0010: no defect found (lock order m_mutex then g_mutex; `t_filling` thread-local; dump writes on producers;
FolderDisc guarded; g_engine never freed; runEnd joins game thread, then keeper, then platform; audioClose idempotent;
the re-base model matches stall-4's 69 / 17 / 25 %; priming fixed; desktop default off).

## Findings

1. Medium: M07-03 asks for queue depth and latency; only underruns / dropped / filled are reported. The logs hold them
   (drive-4 after windows 0-1: queued median 40 ms, 23-186 ms, target 60-129; stall-4 median 44 ms, 22-170 ms; plus
   SDL's 1024-frame device buffer, 21.3 ms). Fix: record queue depth and latency per run, with the peak after a stall
   or resume.
2. Medium: M07-09 (and M07-08's HOME pause) has no suspend / resume run on the candidate build: HOME during INTRO.PSS
   (mission-handheld) and sleep (listen-handheld) are on NRO 4a4001d0 (the first patch 0010); the 6976c8d7 runs hold no
   `confirmed suspension_ns` line. The sleep run's cost is hidden: 35,948 frames (0.75 s) dropped in the window after
   resume, queued 175-288 ms for ~40 s. Fix: rerun HOME during a movie and sleep during play on the candidate and report
   dropped frames and post-resume latency, or a reasoned case that the revised filler cannot change that path.
3. Low: TASKS M07-08 says "no underrun after either skip", then cites the 49 ms hand-over underrun after the Cross skip.
4. Low: AGENTS.md (M07 status) still says "within 0.05 s"; measured 0.00-0.07 s.
5. Low: TASKS M07-10 cites drive-handheld-2 "0 underruns with patch 0010" unqualified (superseded first patch).
6. Low: the "audio closed" line is misattributed: on 60c49e96 / 6976c8d7 it is printed by ps2AudioShutdown ->
   platform::audioClose after the keeper join; platform::shutdown's later close returns early (g_audio 0,
   platform_switch_sdl2.cpp:630). It is printed by whichever closes first, so it cannot show the order.
7. Low: `runIopTime` does not exist (patch 0010's vcs_sound.cpp comment and PS2RECOMP_PATCHES row, REVIEW1_RESPONSE);
   the 1 s forgiveness is in `PS2Runtime::advanceIopEeCycles` (ps2_runtime.cpp ~1002).
8. Low: the loop-command discovery is not in upstream docs/game-internals.md (which lists 0x90500 loop); both AGENTS.md
   require discoveries there. Fix: a root patch.
9. Trivial: tools/switch_m07_readfail.py prints "removed ..." even when a removal failed; the first FTP connection is
   never closed.

## Limitations

No hardware runs or listening; desktop not built or run; thread safety by reading; clipping / saturation never
exercised by the recorded scenes (peaks ~14k, 0 clipped); M07-02's runs on NRO 4a4001d0 (the argument accepted).
