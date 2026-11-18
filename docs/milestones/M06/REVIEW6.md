# M06 review 6 (candidate `e03b04e`): the reviewer's report

Reviewer id: m06-review6-slidingspan (a fresh independent agent; no tracked file edited, no console touched). Verdict:
**REQUEST_CHANGES**, records only; the hardware evidence, the implementation and the package held. Condensed in layout;
the response is REVIEW6_RESPONSE.md.

## Checks the reviewer ran

- `python tools/milestone_gate.py M06`: PASS. `test_upstream_overlay.py`: 4 OK. `test_milestone_gate.py`: 13 OK.
  `validate_project.py`: PASS foundation (183 tasks, 16 milestones). `upstream_overlay.py build/review6/checkout`: PASS,
  9 patches; patch 0009's sha256 matches series.json.
- Patch 0009 in the applied checkout: prune every 1,024 chains with age 3,600, before the `rec` reference is taken;
  `replaySkipped` and `handleBlock` set `lastSeq`.
- NRO compare: `9b89481c` (856af57 package) against `bf18fd1f` (e03b04e package, equal to build/m04/private/gtavcs.nro):
  54,114,501 bytes each, 38 bytes differ, none outside 0x40-0x53 and 0x331e5b0-0x331e5c3 (the GNU build-ID note at
  0x331e5a0); the manifest names e03b04e, not dirty; the package passes `sha256sum -c` and its BETA_NOTES.md equals the
  tracked one.
- Run 17: transcript NRO `9b89481c`, clean exit, `new_crashes=[]`, shutdown at 390.370 s; the log's hash equals the
  read-back; S4 84.6 / 419.9, 59.7 / 146.3, 2.0 / 87.1 ms. 0 GL errors in every `[rhi-stats]` line of the 17 logs.
- Run 16's averages: 6.95 frames/s over the run (8.64 min for 3,600), 5.76 in 700-1,260 s (10.42 min), 0.90 of a
  retention period; heap figures and 3,189 MiB recomputed and matching.

## Findings

1. The prune age "8.6-10.4 minutes" (HARDWARE_RESULTS.md, TASKS.md M06-12, MILESTONES.md M10-07, patch 0009's
   PERFORMANCE.md text and comment) are two average rates, not the span: measured from each `cruise status` answer,
   3,600 cruise frames took 6.7 (from 44 s) to 10.5 minutes (from 480 s) in run 16 and 6.6-9.3 in run 15; the 60 s
   rates range about 4.7-12 frames/s, so "5.8-7 game fps" is wrong as a range. Give the measured span (about
   6.6-10.5 min) or label the figures as average-rate estimates everywhere. The M06-12 conclusion stands.
2. TASKS.md M06-18 cites `(HARDWARE_RESULTS.md, "on the shipped NRO")`; the heading is now "Clean-card acceptance on the
   shipped code".

## Limitations the reviewer stated

Hardware judged from the tracked records; no console, rebuild or desktop run; ELF sections not compared; one chain per
game frame on the Switch untraced (disclosed); M06-01..11, 14-17 and 20 spot-checked.
