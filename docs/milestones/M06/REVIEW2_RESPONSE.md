# M06 review 2 (candidate `f047894`): REQUEST_CHANGES, and what was done

Reopened M06-01, 06, 11, 12, 14, 16, 18 (`9a2e3bf`), fixed, rechecked, ticked again.

| # | Finding | Resolution |
|---|---|---|
| 1 | M06-18's card not "the NRO and gamefolder only" | With the user's consent the card was cleaned by FTP renames (`clean-sd`), the tree recorded before, clean and after; runs 12 and 13 on it; run 13 is the acceptance (about 7 min of S4, HOME close); the card restored identically (`sd-tree-restored.txt` == `sd-tree-before-clean.txt`). No album clips during run 13: by the user's decision the video is run 5's two clips (SCOPE_DECISIONS.md) |
| 2 | Tracked summaries stale and mislabelled | Every `hardware/*-summary.txt` regenerated with the stage windows and copied into the tracked folder; the mislabelled `route-handheld-s2-perf-report.txt` removed |
| 3 | M06-06's docking clause | Recorded under the user's dock decision: SCOPE_DECISIONS.md, MILESTONES.md (M06-06 and M10-13), TASKS.md; nothing claimed |
| 4 | Disconnect rests on one transient | Run 4 described exactly (none for at most 6.6 s, consistent with sliding off the rails); run 13 adds a deliberate sync-button disconnect of wireless Joy-Cons: dual, none with input neutral and the player standing, dual again; BETA_NOTES reworded |
| 5 | M06-12 figures and claims | -0.016 MiB/min over 600-992 s, last bin 960-992 s (4 samples, 606.6 MiB), a plateau of about 8.5 minutes within about 16 minutes of S4; run 5's bounded renderer objects while moving cited; the moving-route soak in M10-07 stated as the implementer's choice; streaming at run 5's close stated as inferred |
| 6 | evidence.json's VU1 range | 296-1,774 |
| 7 | Units and stale figures | 204-213 MiB; AGENTS.md 43.1 / 67.9 ms |
| 8 | Run 10's two clocks | SDL event time vs run time stated; the A presses placed in the cutscene skip; the detached-before-launch difference from the configuration comment noted (the configuration stays as it ran) |
| 9 | Timing method caveats | t_s drift, the windows' use of it, GPU placement by the run's clock and the swap being outside the GPU query: in HARDWARE_RESULTS.md and the summary tool's output |
| 10 | HARDWARE.md stopped at run 7 | Runs 8-13, `clean-sd` / `restore-sd`, the button positions |
| 11 | "about 700 units" | 1,059 net / 1,794 along the samples for run 5; run 13 931 / 2,561 |
| 12 | ImGui touch / mouse mis-scaled in handheld | New task M08-17; noted in TASKS.md |

The session's first instruction for run 12 named the A button for Cross; on the Switch, Cross is the bottom button, B
(the game was right; the instruction was wrong). Recorded in HARDWARE_RESULTS.md.
