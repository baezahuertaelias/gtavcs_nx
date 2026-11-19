# M07 review 4 (candidate `5afb432`): the reviewer's report

Reviewer id: m07-review4-quill (a fresh independent agent; no tracked file edited, no console touched). Verdict:
**REQUEST_CHANGES** (one wrong figure; text only, no hardware rerun needed). Condensed in layout; the response is
REVIEW4_RESPONSE.md.

## Checks the reviewer ran

milestone_gate.py M07 PASS; validate_project.py PASS (183 tasks); test_milestone_gate.py 13 OK; test_upstream_overlay.py
4 OK; test_glsl_shaders.py 3 OK, 1 skipped; overlay into build/: 11 patches applied, hashes match series.json (removed
after). NRO sha256 e3583aa2 = manifest; session logs: drive-4 / stall-4 on 6976c8d7, resume on e3583aa2. Sources
ec397f3 -> 8f4f337 (patches/, src/, CMakeLists.txt, tools/build_game.py) differ only by 0010's comment, its
PS2RECOMP_PATCHES row and the docs-only 0011. Audio stats recomputed for all runs (drive-4 3 underruns in windows 0-1,
0 dropped, filled 52.0 %, queued median 40 ms 23-186; stall-4 0 after windows 0-1, median 44 ms 22-170, 37-72 ms after
each stall; drive-3 21 after windows 0-1; hand-overs 3,375 / 2,339; listen 76,927 dropped, 35,948 after the sleep, up to
288 ms; resume as in finding 1, 41,706 dropped in window 7, 200 / 165 / 141 ms). 0 GL errors in drive-4, stall-4,
resume. Movie margins 0.00-0.07 s, never negative; resume clocks 7.04 -> 7.08, 9.54 -> 10.54 s; suspensions 7.14 /
23.99 / 12.22 / 1.30 s. Own ELF scan: the 12 jal sites; no 0x90500. Reviews 1-3: all findings resolved (spot-checked
for 1-2). Patch 0010: no defect found (lock order, thread hand-off, re-basing, priming, shutdown order, desktop).

## Findings

1. Medium: the resume run's "0 underruns" (TASKS M07-09, evidence.json, REVIEW3_RESPONSE row 2) is wrong: 3 underruns,
   window 0 528,352 frames (STINGS time), window 1 586,912 (INTRO time), window 2 1,850 frames (~39 ms, the hand-over
   after INTRO.PSS was skipped); none after any of the four resumes. M07-08 omits that INTRO.PSS was skipped (367
   pictures, 36.3 s, skipped, after A presses at 56.3 s; the person was not asked to skip).
2. Low: no person-observations.txt entry for resume-handheld; "the user asked to press..." reads as if the user gave
   the instruction. Record what the person was asked, did and reported (or that nothing was reported).
3. Low: NRO provenance not recorded: the manifest's root_dirty=true (likely tests/switch/m07-resume.cfg, committed a
   minute later in f5eca27); 6976c8d7's root commit only in REVIEW1_RESPONSE; the two NROs differ in hash at the same
   size (plausibly the overlay path "game-source-08a801a89bd07492" in the debug info). One line in TASKS or the
   response.
4. Trivial: tools/switch_m07_readfail.py: the REFUSED path leaves the first FTP connection open; a failing
   `m04.connect()` in `finally` hides the run's return code.
5. Trivial: loop-command.md's limitation does not name indirect calls (jalr / function-pointer tables) to 0x10DD50.

## Limitations

No hardware runs or listening; desktop not built or run; thread safety by reading; the NRO hash difference's cause
inferred (no copy of 6976c8d7 locally); queue depth sampled at window ends; PCM / volume / heap / radio / voice figures
accepted from review 3 and spot reads.
