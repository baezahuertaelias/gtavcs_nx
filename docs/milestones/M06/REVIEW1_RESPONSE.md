# M06 review 1 (candidate `dc23690`): REQUEST_CHANGES, and what was done

Reopened M06-01, 05, 11, 12, 14, 16, 17, 18, 19, 20 (`7a1f57d`), fixed, rechecked, ticked again.

| # | Finding | Resolution |
|---|---|---|
| 1 | M06-11 stage windows mislabelled (fixed 3-38 s / 100 s+ windows) | `tools/switch_m06_session.py summary` lists the camera at every `gameinfo` answer and cuts timing at `--window` stage boundaries taken from it; HARDWARE_RESULTS.md's Timing table rebuilt: S2 and S3 from the silent run (no input), S4 from the route, silent and play runs, S2 / S4 from the play run |
| 2 | GPU time not per stage; meaning not stated | gpu.csv's frame numbers placed in time through the progress lines' `host_frames`; GPU present-to-present per stage; stated as an upper bound of GPU busy time (it includes idle between the presents); removed from BETA_NOTES.md's "at Fort Baxter" table |
| 3 | Route's s3-050 / s3-070 (and s2-030) are S4 | Stages table relabelled: the route's s2-030 / s3-050 / s3-070 listed under S4, S3 evidence from the silent and reference runs; the config comment says so |
| 4 | Soak leak conclusion overclaimed | Run 7's rising trend stated (+7.9 MiB/min, the 2-minute means); new run 11 (`m06-soak30.cfg`): 998 s of S4 before a 746.6 s suspension; heap in use levels off at ~600 MiB from 480 s, +0.02 MiB/min over 600-998 s; heap_obtained's slower growth and a moving-route soak left to M10-07; MiB used throughout |
| 5 | Pause-menu claim not in the log | Removed: Start pressed 14 times (SDL events); the two `[menu]` lines identified as boot-time messages |
| 6 | Album counts and attribution | 14 screenshots, 4 clips; each attributed to run 4, 5 or 6 by the album's clock offset; M06-18's video stated as run 5's two clips (30 s at most each), the 5 minutes resting on the log |
| 7 | Wireless proof thin | Runs 9 and 10 (`m06-wireless.cfg`, Joy-Cons detached): run 9 logged no input (nothing pressed; resent); run 10 logged sticks, A and Start while the player walks in S4, paired-dual style throughout |
| 8 | Run 6 has no GL-error figure | Statement narrowed to the runs that log `[rhi-stats]` (all but run 6); the run's config left as it ran |
| 9 | Package provenance not enforced | `tools/package_beta.py` requires `manifest.json`'s `root_commit` == HEAD, `root_dirty` false and its `nro_sha256` == the NRO; the old package deleted; runs 1-6's NRO equivalence stated as unverifiable (no manifest kept) |
| 10 | "Upright" not the person's word | The person quoted exactly; orientation from the album captures |
| 11 | Low-severity items | Reference run's ranges (296-1,774 VU1 programs, 1.8-10.1 fps); AGENTS.md's window paragraph rewritten for the fixed surface; m06-route / m06-dock comments; overwritten run 4 / run 8 lists noted (transcript is the record); application mode from `mapped_total`; the `PS2X_HLE_VERIFY` message says what is partly applied |

New hardware runs (NRO `9a9f7e12`, built from the clean `7a1f57d`): 9 wireless-handheld, 10 wireless-handheld-2,
11 soak30-handheld.
