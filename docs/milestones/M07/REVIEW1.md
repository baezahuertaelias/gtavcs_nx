# M07 review 1 (candidate `b07f4f1`): the reviewer's report

Reviewer id: m07-review1-granite (a fresh independent agent; no tracked file edited, no console touched). Verdict:
**REQUEST_CHANGES**. Condensed in layout; the response is REVIEW1_RESPONSE.md.

## Checks the reviewer ran

milestone_gate.py M07 PASS; test_upstream_overlay.py 4 OK; test_milestone_gate.py 13 OK; validate_project.py PASS;
test_glsl_shaders.py 3 OK, 1 skipped; upstream_overlay.py: 10 patches applied, 0010's sha256 matches series.json, its
PROGRESS / PS2RECOMP_PATCHES rows present; pcm_compare.py re-run on the WAVs: the three reports reproduce byte for byte;
audio totals recomputed over every tracked log (drive-2 0 underruns, 688,128 starved frames all in the first window,
39,192 dropped, filler 86.2 %; drive-1 169 underruns, 2,606,686 starved after the first window = 54.3 s, 42.1 s dropped;
stall-2 0 underruns, its only drop right after the movie; listen 0 in 78 windows; mission 0 in 27); movie margins
0.00-0.07 s (0.07 only at the 0.00 s line); heap 315.4 / 369-370 / 322.7 MiB; FFmpeg pins in the M01 lock, the image
digest matching, the root LICENSE GPLv3; icon.png / icon.jpg ignored and untracked, no WAV tracked; --settings puts
vcs.ini after the wipe and the run read it.

## Findings

1. Patch 0010: the keeper fills only under 40 ms and asks for `target - queued`, but the SPU2 hands samples over in
   256-frame blocks, so a fill can land just under the priming target (2,816 frames, 58.7 ms against 60): the ring never
   primes and the keeper never fills again; silent until the game thread pushes. Every patched run's first window shows
   it ("produced 0% ... played 0 ... queued 64 ms"): the "game sound paused under the movie" (M07-08) is this stuck state.
   After an underrun in a fill-only state the same can recur. Fix: fill while not primed and below the target, round
   `want` up to whole 256-frame blocks, decide whether the SPU2 should mix under a movie, test an underrun with the keeper
   as the only producer.
2. After a stall over 1 s the runtime's real-time catch-up forgives the lag the filler already mixed, so `clock()` never
   mixes again: everything after stall-2's "stall main 2000" is "filled 100%", and the game-thread stalls were measured
   with the game thread out of the mixing path; all SPU2 work (music disc reads) stays on the keeper, the ring held at
   ~44 ms, `PS2X_IOP_REALTIME=0` overridden. The filler is on by default on the desktop too, never validated there. Fix:
   Switch-only default or desktop validation; record the lead and the PS2X_IOP_REALTIME interaction; re-run the stalls.
3. Shutdown order: `ps2AudioShutdown` (which joins the keeper) runs in `~PS2Runtime`, after `runEnd`'s
   `platform::shutdown()` closed the device and after the disc was unset; every log shows "[platform] audio closed" before
   "[core] runtime destroyed". The "joined before the device closes" claims are false. Fix: stop the keeper before
   `platform::shutdown`, or correct the claims.
4. M07-08 / evidence.json say the user's skip was B: four SDL `b` presses (east = Circle) at 43.0-44.3 s did not skip;
   `start down` at 45.5 s did. Say Start skipped it.
5. M07-05 seeking / looping and the AUDIO lookup are not evidenced (`VCS_STREAM_LOG` logs only the EE world stream).
6. M07-09 error recovery has no evidence (no failed open, no PS2X_AUDIO=0, no decode / read failure).
7. M07-04 / 07 / 11 overclaim: "mission running" is the opening mission active from S3 in every run; "every bank is
   loaded and played" is a statement about code; simultaneous channels not measured; cutscene sync on the patched build
   rests on "Everything good" to a question that was not about sync; "the user heard no gap" is an inference; the radio
   answer's question wording is not recorded.
8. Minor: drive-1 starved 54.3 s, not 54.2; the pre-patch stall summary "each cost one underrun" (main 250 / 2000 cost 2,
   game 100 cost 0); pcm-handheld-2 and volume WAVs not hashed (00635289..., 018afbeb...); M07-01 quotes one 0-underrun
   line of a run with 6 underruns.

## Limitations

No hardware runs or listening by the reviewer; findings 1 and 2 from code reading and the tracked logs, not reproduced
on hardware.
