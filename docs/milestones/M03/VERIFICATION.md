# M03 verification

Desktop and tool results, 2026-11-09. Hardware results are recorded separately after
the Switch run. No game data is reproduced here; logs stay in `build/evidence/M03/`.

## Whole-disc identity

`tools/extract_disc.py verify --iso` rebuilds the image from the folder form and hashes
it: `981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740`, equal to the
source image. This covers files, padding, gaps and non-file sectors.

## C++ reader against the real image

`tests/desktop/disc_compare.cpp` compares `FolderDisc` with upstream's `vcs::IsoImage`
on the real data. Result: `files=341 comparisons=23449 bytes=12626136345 failures=0`.

- Every file's head, every end with its padding, and unaligned reads across each start and end.
- 20,000 random reads of 1 byte to 1 MiB.
- The whole image sequentially in 1 MiB reads.
- Both end-of-disc failure cases.
- Path lookups by `cdrom0:\...;1` and every directory listing, in order.

## Live comparison during real game execution

`tools/desktop_folder_disc.py` patches only the disposable reference clone. A tee answers
each disc request from the ISO and compares it with `FolderDisc`. The run used an empty
card and a scripted boot, flyover, intro cutscene, gameplay, teleports and an AI drive.

| Measure | Result |
|---|---:|
| Read comparisons, at the last 16-read log point | 416 |
| Bytes compared | 69.7 MB |
| File lookups compared | 601 |
| Mismatches | 0 |
| Reads crossing a file end | 70 |
| Unaligned reads | 0 |

Files read include `GAME.DTZ`, `MAINLA.LVZ`, `MAINLA.IMG`, `GTA3PS2.IMG`, `MOCAPPS2.IMG`,
movies, cutscene and music `.VB` streams, all `.SDT` tables and frontend `.PSP` files.

**This comparison found a real defect.** The first run reported one `listDirectory`
mismatch: the manifest wrote directories before files, so a directory's listing order
differed from the disc. The manifest now keeps one walk-ordered list, and the
synthetic test uses a root where a file precedes a directory. The rerun had 0 mismatches.

**Limits.** The scenario never loaded `BEACH` or `MALL`. Teleports moved the player but
the run did not change the loaded level. The plain ISO control run behaved the same,
so this is the scenario, not the folder disc. Those files are covered by the comparisons
above but not by live play. The game itself issued no unaligned read in this run.

## Boot with no ISO opened

With `VCS_FOLDER_ONLY` set the patched reference opens no image and uses `FolderDisc` and
the extracted `SLUS_215.90` directly. It reached gameplay at the same step as the ISO
control, followed the same scripted route and exited its loop normally.

## What this does not establish

- Nothing here ran on a Switch. The Switch results are in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md).
- Upstream `src/main.cpp` still looks for an ISO. It is not built for the Switch; M04's
  boot code must use `FolderDisc` and `GamePaths` in its place.
- Async stream cancellation is not exercised in play. Upstream's stream reader runs
  requests to completion and never cancels one, so the requirement reduces to safe
  shutdown with reads pending. That is covered by the hardware harness, and full
  cleanup is M04-08.
