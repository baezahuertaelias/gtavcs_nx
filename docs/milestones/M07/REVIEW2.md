# M07 review 2 (candidate `f820d89`): the reviewer's report

Reviewer id: m07-review2-basalt (a fresh independent agent; no tracked file edited, no console touched). Verdict:
**REQUEST_CHANGES**. Condensed in layout; the response is REVIEW2_RESPONSE.md.

## Checks the reviewer ran

milestone_gate.py M07 PASS; validate_project.py PASS (183 tasks); test_milestone_gate.py 13 OK; test_upstream_overlay.py
4 OK; test_glsl_shaders.py 3 OK, 1 skipped; upstream_overlay.py into a disposable folder: 10 patches applied, 0010's
sha256 b88b1da7... matches series.json. From the tracked `[audio] produced` lines: drive-4 3 underruns (528,351 +
121,816 frames, windows 0-1), 0 dropped, mean filled 51.5 %; stall-4 3 underruns (windows 0-1), 0 after, filled share
69 / 17 / 25 % after the 2 s game stall; drive-3 21 underruns after boot (472,879 frames = 9.85 s); stall-3 2,995 and
46,271 frames; drive-1 169 underruns; mission-2 hand-over 3,375 frames; movie-sync-2 2,339; readfail 1 (movie time).
Voices 32 / 26 / 29 / 12; radio 168 music commands, no 0x905xx; noaudio "no sound output (host clock)", 0 GL errors;
skip events B x4 at 43.0-44.3 s, Start at 45.5 s. The four WAV hashes match; pcm_compare.py on pcm-handheld-2 reproduces
pcm-compare-2.txt (line endings only). Own stdlib scan of the local ELF (finding 4).

Patch 0010 judged correct as far as could be seen: lock order always Engine::m_mutex then g_mutex, no inversion; the
keeper never touches EE RAM; its disc reads go through the mutex-guarded FolderDisc and it is joined in runEnd before
the stream reader stops and the disc is unset; `t_filling` thread-local; the re-base consistent with runIopTime (a
lead counts the ring's fill too, so the re-base can fire before the runtime forgives and the EE repays up to one target
fill, <= 250 ms, twice: bounded; stall-4 / drive-4 0 dropped); desktop default off; the runEnd order change
unconditional, harmless, recorded. Not measured: a fill holds the engine lock for up to ~100 ms of mixing.

## Findings

1. evidence.json still holds statements review 1 corrected in TASKS.md: M07-01 movies-handheld-2 "underruns 0" (6
   underruns); M07-08 "skipped with B at 33.2 s" (Start), contradicting the next entry; M07-04 "first mission running
   (32 scripts active, mission running)"; M07-03 drive-handheld-2 "0.0 s starved" (688,128 frames = 14.3 s in window
   0); stall-handheld-2 "0 underruns, 0 dropped" (39,694 dropped in window 1); M07-11 rests on stall-handheld-2 (the
   game thread out of the mixing path after the 2 s stall); M07-11 / 07 movie-sync "within 0.05 s" (0.01-0.07);
   M07-10 long route still drive-handheld-2. Fix: align every entry with TASKS.md and the logs; point M07-03 / 10 / 11
   at stall-handheld-4 and drive-handheld-4.
2. readfail never reached S4: the last gameinfo lines (165-225 s) show player controls off, camera mode 17 (flyby /
   cutscene), area 2; the player at -1646.56 -139.23 to 235 s; stopped at 240 s without input: still the S3 cutscene.
   TASKS M07-09, evidence.json and REVIEW1_RESPONSE #6 say S4.
3. `[platform] audio closed` before `[platform] shut down` is in every run, the pre-fix ones too: `platform::shutdown()`
   itself calls `audioClose()` first (platform_switch_sdl2.cpp:327). Cited as proof of the shutdown fix, and "(before
   review 1 it came after the device closed)" is wrong. The code fix is right. Fix: cite the code, or log the join.
4. loop-command.md: at 0x10DC8C the jal is reached through BGEZL at 0x10DC78 whose delay slot sets `lui a0,8`: a0 =
   0x80000 (init); the `lui a0,0x4a` path branches past the jal. The radio log has one 0x80000 and no 0x4A0000. TASKS
   M07-05 repeats 0x4A0000. Conclusion unaffected (no 0x90500 found).
5. "each ended by a stop" is not true: 12 opens and 12 stops, one stop for the cutscene file 54; the last open (file 4
   at 1,380,779 ms) still playing at exit; 3 of the 12 opens are at position 0.
6. Movie sync is 0.00-0.07 s, not 0.01-0.07 (due == clock lines in mission-handheld-2, mission-handheld, readfail);
   0.07 only at a movie's first line; never negative.
7. Minor: drive-4's "3 underruns ... while the boot movies play" (the second of window 1 is the 49-70 ms hand-over;
   likewise mission-handheld-2's 3,375 frames against "0 underruns after boot in every run", M07-04).
   tools/switch_m07_readfail.py creates mods/ and mods/MOVIES before the `try` (a failed put leaves them), and a failed
   delete in `finally` skips both rmd. Fix: inside the try; each cleanup step failure-tolerant.

## Limitations

No hardware runs or listening; user answers taken from person-observations.txt; desktop not built or run; the ELF scan
covers lui+ori/addiu and jal 0x10DD50 only; thread safety by reading, no sanitizer.
