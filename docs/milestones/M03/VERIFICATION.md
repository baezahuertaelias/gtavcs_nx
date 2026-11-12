# M03 verification

Desktop and tool results, 2026-11-09, with the review fixes of 2026-11-12 (synthetic
suite, extractor tests, level probe). Hardware results are in
[HARDWARE_RESULTS.md](HARDWARE_RESULTS.md). No game data is reproduced here; logs stay in `build/evidence/M03/`.

## Whole-disc identity

`tools/extract_disc.py verify --iso` rebuilds the image from the folder form and hashes
it: `981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740`, equal to the
source image. This covers files, padding, gaps and non-file sectors.

## Extractor tests

`python -m unittest tools.test_extract_disc` builds synthetic ISO images; no game data.
Result at commit `c796a19`: 11 tests, OK. They cover, in this order of the M03-01 claims:

- `test_rejects_wrong_identity_before_writing`: a failed identity check raises before the
  destination folder exists.
- `test_manifest_keeps_original_lba_and_order`: directory and file records keep their
  original LBAs and on-disc walk order; a data gap becomes an `R` record.
- `test_round_trip_rebuilds_every_byte`: extracted files equal the image and `verify`
  rebuilds the image's SHA-256.
- `test_rejects_unexpected_padding`, `test_rejects_overlapping_files`,
  `test_rejects_traversing_names`, `test_rejects_names_the_loader_refuses`: each
  refused before anything is written; the padding and name tests check that the
  destination does not exist afterwards. Since `c8856a7` the extractor refuses what
  `FolderDisc::load` refuses, lower-case names (so also names differing only by case)
  and control bytes, and since `c796a19` anything outside printable ASCII, which the
  ASCII manifest could not hold; `test_safe_names_match_the_loader` checks the rule directly, and
  all 369 paths of the real disc pass it. (Until `9e020e7` padding was checked while
  writing, so a refusal could leave a partial tree; the review found it.)
- `test_empty_files_hold_no_sectors_in_any_order`: zero-length files sharing a file's
  first sector or inside its span, before or after it on the disc, extract and verify,
  as `FolderDisc` loads them.
- `test_detects_changed_file`, `test_keeps_unrelated_files_and_refuses_other_sizes`:
  verification catches a changed byte; re-extraction keeps unrelated files and needs
  `--replace` to overwrite a file of a different size.

On the real disc, `extract_disc.py verify --iso` is the whole-disc identity above.

## Synthetic FolderDisc and GamePaths suite

`python tools/run_folder_disc_tests.py build/evidence/M03/synthetic-c8856a7` compiles `src/vfs`
and `tests/core/folder_disc_tests.cpp` with the locked container's host g++ 12.2.0 and
runs them twice, with address randomization off (`setarch -R`, which needs the container
started with `seccomp=unconfined`): under the container kernel's high-entropy randomization
AddressSanitizer printed `DEADLYSIGNAL` forever. The fixture is generated; no game data.

| Run | Flags | Result | Log SHA-256 |
|---|---|---|---|
| Sanitized | `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all` | PASS, exit 0 | `76d470ef012b40e3f27b0cba728b4e42631f6061664d61c389b7cb74372bb41b` |
| Optimized | `-O2` | PASS, exit 0 | `639553db336ea13066edaaa5ba8c2a64a9a989dbcfcdd4b463fb51cbe43dd0b6` |

Both logs record source commit `c8856a71d937b39f86fbb158c40d82f5c6ffa5d3` with
`sources_dirty=False`, and the full command. Since `c8856a7` that flag covers every build
input (`src/vfs` headers and the `upstream` submodule too), not only the three `.cpp`
files. The earlier `synthetic/` logs are from `9e020e7`. The same suite also runs on the Switch inside
the runtime payload (HARDWARE_RESULTS.md). The one `read failed` line in each log is the
intended shortened-file case; the three verify lines that follow it are the intended
corruption and missing-hash cases.

What the suite checks:

- Path normalization, directory listing, whole-image and unaligned reads, reads across
  padding, gaps and raw sectors, end of disc (M03-04 to M03-06).
- Four concurrent readers over twelve files, more than the eight handle slots (M03-07).
- 19 rejected layouts (M03-05): missing root, wrong version, traversal, lower-case name,
  extent past the end, extents wrapping past 2^64 in `F` and `R` records, a second sector
  count after the entries, overlap, bad number, entry without its parent, duplicate path,
  wrong size, raw data out of order, a file hash that is not 64 lower-case hex digits, a
  bad `raw_sha256`, a second `raw_sha256`, truncated raw data, missing game file.
- Zero-length files inside the raw span and sharing a file's sector, in both manifest
  orders: load succeeds and no sector is hidden.
- Mods (M03-11): a same-size mod replaces the disc file; a wrong-size mod and a directory
  in a mod's place are each refused with a warning naming the path and the reason. A file
  shortened after load fails its read, is logged and zero-filled.
- Verify mode (M03-11): SHA-256 known answers (empty, `abc`, the two-block vector and a
  million `a`); an intact tree verifies; one flipped byte in a file of the right size
  still loads and `verifyFiles` names that file; one flipped byte in `raw.bin` is named;
  a mod over a disc file does not fail verify (the disc copy is hashed); a file recorded
  with `-` is named as having no hash. On the host the same code, at `c796a19`
  with `src/vfs` and `upstream` clean, verified the real extracted tree with 0 problems in
  14.9 s (`build/evidence/M03/host-verify-c796a19/host-verify.log`, SHA-256
  `9a142ade9093dcbcb0af8a4c89ee91e967b6980ed6183cded7eb0cf46e2a9cdd`; the driver
  `verify_main.cpp` is kept beside it).
- `GamePaths` (M03-10, M03-13): `sdmc:` stripping and refusals; a missing game folder is
  fatal; settings, cache, both cards and the card backups folder under `userdata`; a test
  profile puts all of them under `userdata/tests/<name>`; unsafe profile names refused.

**Review finding fixed here.** The suite at the earlier candidate read a 3,396-byte file
into a 2,048-byte buffer. AddressSanitizer reports that heap overflow, and plain builds
crashed. The test now reads into a buffer of the file's size.

**Found on the Switch.** At `c5903d1` the suite passed on the host but failed twice on the
Switch, because `std::filesystem::remove_all` on the relative fixture path left `mods/` in
place there (HARDWARE_RESULTS.md, "Earlier sessions"). The mods are now removed with checked
POSIX calls, and the suite passes on both.

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
card: boot, flyover and intro cutscene with no input, then gameplay at Fort Baxter, four
logged teleports, one unlogged teleport back to the first spot, and an attempted AI drive
(`build/evidence/M03/desktop/tee-steps.txt`).

What the steps actually show: the first two logged teleports had no effect, because the
opening mission still placed the player; the third and fourth reached (1325, -325) and
(-1425, -575). The unlogged return teleport placed the player at (579.4, 175.0, 111.2) on
the east island, and the "AI drive" never moved: the player stayed there for all six
samples over 75 s. So the live reads cover boot, movies, the cutscene, streaming at Fort
Baxter and two teleport destinations, and the radio, not a drive. The folder-only and ISO
runs show the same outcome at every teleport and during the drive; their first sample at
Fort Baxter differs by up to 69 units, as the opening scene moves the player.

| Measure | Result |
|---|---:|
| Read comparisons, at the last 16-read log point | 416 |
| Bytes compared, same point | 69.7 MB |
| File lookups compared, same point | 601 |
| Mismatches | 0 |
| Reads crossing a file end | 70 |
| Unaligned reads | 0 |

Files read include `GAME.DTZ`, `MAINLA.LVZ`, `MAINLA.IMG`, `GTA3PS2.IMG`, `MOCAPPS2.IMG`,
movies, cutscene and music `.VB` streams, all `.SDT` tables and frontend `.PSP` files.

**This comparison found a real defect.** The first run reported one `listDirectory`
mismatch: the manifest wrote directories before files, so a directory's listing order
differed from the disc. The manifest now keeps one walk-ordered list, and the
synthetic test uses a root where a file precedes a directory. The rerun had 0 mismatches.

**Limits.** The scenario never loaded `BEACH` or `MALL`, not even with the player on the
east island; the plain ISO control run behaved the same, so this is the game's state, not
the folder disc. Those files are covered by the comparisons above but not by live play.
The game itself issued no unaligned read in this run.

## Level probe

`tools/desktop_level_probe.py` (2026-11-12, `build/evidence/M03/desktop-levels-clean/`)
reran the question with the tee: after a new game it teleported five times and sampled the
player, the interior area and the level the player's zone asks for (`0x4CD118`) every 5 s.

- Both teleports to the east island were overridden by the opening mission (player stayed
  at Fort Baxter, area 2 at the end); the later ones reached (1325, -325), (-675, 1425) and
  (-1425, -575). The zone level stayed 2 (MAINLA) at every sample.
- Reads at the last 16-read log point: 416, 0 mismatches; level files read up to that
  point: `MAINLA.LVZ`, `MAINLA.IMG` only. The tee then wrote its counters every 16 reads,
  so up to 15 later reads, and the files they touched, are not recorded; mismatches were
  always written at once. Since `c8856a7` it writes after every read and at exit.
- `tee-steps.txt` SHA-256 `79808175b2aa0a73e918c844a59f79176ccc49a8ff792d7eb4448f87ebd7689c`,
  `tee-reads.txt` `90030b28461cdd8bd9301fe7c0d454f224b54c1dc332b3ed25c7b6e394d61fcf`.

An earlier probe on 2026-11-11 recorded zone level 1 at (575, 175) but its step log was
written by two processes at once and is unreliable; it was discarded and is not evidence.
The live BEACH/MALL check moved to M14-02 ([SCOPE_DECISIONS.md](SCOPE_DECISIONS.md)).

## Boot with no ISO opened

With `VCS_FOLDER_ONLY` set the patched reference opens no image and uses `FolderDisc` and
the extracted `SLUS_215.90` directly. It reached gameplay at the same step as the ISO
control, with the same teleport and drive outcomes (the first Fort Baxter sample differs
slightly between runs), and exited its loop normally.

## What this does not establish

- Nothing here ran on a Switch. The Switch results are in [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md).
- Upstream `src/main.cpp` still looks for an ISO. It is not built for the Switch; M04's
  boot code must use `FolderDisc` and `GamePaths` in its place (M04-13).
- No runtime routes settings, cache, cards or backups through `GamePaths` yet. The desktop
  runs used upstream's own data folder. That routing, with Switch proof, is M04-13.
- Async stream cancellation is not exercised in play. Upstream's stream reader runs
  requests to completion and never cancels one, and the CD break and stop calls do
  nothing: `sceCdBreak` and `sceCdStop` return success in the EE stubs
  (`ps2xRuntime/src/lib/Kernel/Stubs/CD.cpp:351`, `:689`) and in the IOP
  (`ps2xIOP/src/emulator/imports/iop_cdvd.cpp:289-292`). The requirement so reduces to
  safe shutdown with reads pending. The hardware harness covers that for threads it owns.
  Upstream's stream reader thread is never joined and reads through
  `ps2_disc::image()`, so destroying a `FolderDisc` under it would be a use-after-free;
  M04-08 now names that order (stop and drain the reader and the CD paths, clear
  `ps2_disc::setImage`, then destroy the disc).
