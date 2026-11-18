# M06 review 5 (candidate `3d36559`): the reviewer's report

Reviewer id: m06-review5-buildid (a fresh independent agent; no tracked file edited, no console touched). Verdict:
**REQUEST_CHANGES**, records only; every recomputed figure matched and the build-ID claim was confirmed. Condensed in
layout; the response is REVIEW5_RESPONSE.md.

## Checks the reviewer ran

- `python tools/milestone_gate.py M06`: PASS. `python tools/test_upstream_overlay.py`: 4 OK.
  `python tools/test_milestone_gate.py`: 13 OK. `python tools/validate_project.py`: PASS foundation (183 tasks, 16
  milestones). `python tools/upstream_overlay.py build/review5/checkout`: PASS, 9 patches; the series hash matches the
  manifest's `overlay_series_sha256`.
- NRO compare (`9b89481c` from the 856af57 package, `03000b3b` from the 3d36559 package, equal to
  `build/m04/private/gtavcs.nro`): 54,114,501 bytes each, 40 bytes differ in two runs, 0x40-0x53 (the NRO header's
  build ID) and 0x331e5b0-0x331e5c3 (the GNU build-ID note), the same 20-byte IDs in both. The manifest names
  `3d36559`, not dirty; no src / tools change between b309413 and 3d36559.
- NRO history against the transcripts: 1-6 `1854e2be`, 7 and run 8's first attempt `4a15e831`, 8 `4e4a8524`, 9-15
  `9a9f7e12`, 16-17 `9b89481c`; 18 `new_crashes=[]` lines (12 + 2 + 1 + 3).
- Route points by the stated method: run 16 at 2,050 m 624.4 / 814.5 / 800.8 MiB, at 4,000 m 701.4 / 835.6 / 847.4
  (still +11.8 MiB across the window: "flat" is approximate); run 15 640.7 / 794.3 / 903.7 / 1,014.5.
- Heap: run 15 +15.55, run 16 -0.47 MiB/min (700-1,260 s); heap_obtained +51.2 / +17.1 MiB/min, 1,389 MiB; two-minute
  means match. Desktop: +33.02 / +1.37 / +2.66 MiB/min, 1,080 / 391 (max 414) / 784 MiB; recordings and fallbacks
  match.
- Patch 0009: the prune before `rec` is taken; `replaySkipped` sets `lastSeq`; the cruise `frames` counter counts game
  frames (`rig.cpp` `cruiseFrame`).
- BETA_NOTES.md's rows match the summaries; run 17 from its raw perf.csv: n=2,489, median 84.59 ms, p95 418.9-420.6 by
  method. 0 GL errors in every `[rhi-stats]` line of all 17 tracked logs.

## Findings

1. Records still name `9b89481c` as the shipped NRO, though the package ships `03000b3b`: evidence.json (M06-18 run 17,
   M06-11 run 17), HARDWARE_RESULTS.md (run table row 17, the timing row, the acceptance heading and its first
   sentence), HARDWARE.md (run 17's row).
2. The prune age: "about 8.6 minutes" is the whole run's average (6.95 game frames/s); in run 16's 700-1,260 s window
   the rate was 5.76 frames/s, about 10.4 minutes, so the window covers about 0.9 of a retention period, not "about
   one". Give the range. One chain per game frame is still untraced on the Switch. Not blocking: M10-07 owns it.
3. Nit: the patch comment "~8.6 min at the Switch's ~7 driving" lacks a unit.
4. Minor: AGENTS.md's status (2026-11-17) gives only 43.1 / 67.9 ms from an NRO without patch 0009; add run 17 and
   patch 0009.
5. Nit: "1,389 MiB of the 3,344 MB mapped" mixes units; 3,343,908,864 bytes are 3,189 MiB.

## Limitations the reviewer stated

Hardware judged from the tracked records and the ignored raw copies; no console, no desktop build or run; ELF
sections not compared (the NRO byte compare stands in); one RSL chain per game frame not traced on the Switch;
M06-01..11, 14-16 and 20 spot-checked, not fully re-derived.
