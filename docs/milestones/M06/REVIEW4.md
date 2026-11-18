# M06 review 4 (candidate `856af57`): the reviewer's report

Reviewer id: m06-review4-rslprune (a fresh independent agent; no files edited, no console touched). Verdict:
**REQUEST_CHANGES**. The code was found correct and the hardware evidence supportive; the findings are in the records.
Kept as reported (condensed only in layout); the response is REVIEW4_RESPONSE.md.

## Checks the reviewer ran

- `python tools/milestone_gate.py M06`: PASS (eligible to request independent review).
- `python tools/test_upstream_overlay.py`: 4 tests OK. `python tools/test_milestone_gate.py`: 13 tests OK.
  `python tools/validate_project.py`: PASS foundation (183 unique tasks, 16 milestones).
- `python tools/upstream_overlay.py build/review4/checkout`: PASS applied 9 patches; the 0009 patch's sha256 matches
  series.json.
- The tracked logs of runs 15, 16 and 17 match the hashes the session read back from the card.
- Recomputed: run 15 +15.55 MiB/min over 700-1,260 s, 1,035 MiB at the end; run 16 -0.47 MiB/min, heap_obtained
  +51.2 / +17.1 MiB/min, 1,389 MiB; run 11 -0.016 MiB/min over 600-992 s; the two-minute means of runs 15 and 16; the
  desktop median game time 5.3 / 6.7 ms; the recording counts 32,582, 5,674 (76,041 pruned), 7,864.
- Run 17: 81 `[rhi-stats]` lines with 0 GL errors, driving at 90-150 s and 270-390 s, HOME close at 390.37 s,
  `live_threads=2`, crash lists unchanged, timing 84.6 / 419.9 ms, the transcript's and the package's NRO `9b89481c`.
- The restored tree against the older preflight tree: only run outputs and the NRO size differ.

## Patch 0009

Correct: pruning runs on the processing thread only; no live reference can be pruned (the prune comes before `rec`
is taken, every user sets `lastSeq` first, a nested `handleBlock` shares the chain's seq); `replaySkipped` does not
use its iterator after the fallback. The game thread's `g_confirmed` keeps confirmations for pruned keys, so pruned
but confirmed, unchanged blocks now take the `replaySkipped` fallback from guest RAM (225 fallbacks in the desktop
null-pruned run, 180 Vulkan-pruned, 0 unpatched). Older, unchanged behaviour on that now-live path: the non-World
fallback drops the block without a counter if `flattenCall` fails or the size differs; no harm seen.

## Findings

1. Wrong NRO history: HARDWARE_RESULTS.md and REVIEW3_RESPONSE.md say runs 1-13 used `9a9f7e12`; runs 1-6 used
   `1854e2be`, run 7 `4a15e8..`, run 8 `4e4a8524`; runs 14 and 15 also used `9a9f7e12`.
2. HARDWARE_RESULTS.md says "a player's launch runs the same code as runs 1-6": no longer true with patch 0009 in the
   shipped NRO; only runs 16 and 17 tested it.
3. Run 16's "814, 841, 855 MiB at the same route points" not reproducible (the ~2,050 m point gives about 814, 801-803,
   826); evidence.json's "830.1-843.5 MiB from 480 s" omits the last bin (820.7).
4. Runs 14-16 have no tracked session transcript (only ignored `build/evidence/M06/*.out`), though "the transcript
   records `new_crashes=[]` for all eighteen" is claimed and the transcripts are the only record of runs 15/16's NRO.
5. The desktop private-bytes figures are MiB labelled MB (34.6 MB/min, 1,133 MB unpatched = 33.0 MiB/min, 1,080 MiB;
   391 / 414 / 784 are MiB; 1.43 MB/min = 1.37 MiB/min): HARDWARE_RESULTS.md, TASKS.md, evidence.json,
   REVIEW3_RESPONSE.md and the PERFORMANCE.md text in the patch.
6. `kRecordingAge = 3600u; // chains: a minute at 60 fps, two at 30` assumes 60 / 30 fps; run 16 drove at about 6.3 game
   fps, so 3,600 chains are about 9.5 minutes there, longer than a route loop (~2,850 chains), and the 700-1,260 s
   window covers about one retention period. State the effective retention (or make it time-based); optionally record
   the skip-fallback activity.
7. BETA_NOTES.md's frame times all come from NROs without patch 0009; run 17 (the shipped NRO, 84.6 / 419.9 ms) is
   missing; say which NRO each row comes from.

## Limitations the reviewer stated

Hardware judged from the tracked records and the ignored build/evidence outputs, without the console; the desktop
game not built or run; the visual correctness of the skip-fallback path not verified; one chain per game frame taken
from the patch's comment, not traced.
