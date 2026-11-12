# M03 execution checklist

Branch `milestone/m03-folder-vfs`. Parent task status lives in
[docs/MILESTONES.md](../../MILESTONES.md). A step here is ticked only when its proof
exists. Results are in [VERIFICATION.md](VERIFICATION.md); formats in
[FORMATS.md](FORMATS.md).

## Disc layout findings

Measured on the user's image, 2026-11-09:

| Measure | Value |
|---|---:|
| Sectors | 1,066,336 |
| Files / directories | 341 / 29 |
| File extents overlapping | 0 |
| Non-zero padding after file data | 0 files |
| Non-file sectors holding data | 6 spans, 935,936 bytes |

Non-file sectors are volume descriptors, path tables, directories and some padding.
They are private disc data and stay in `gamefolder/.nxdisc/raw.bin`.

## M03-01 — Extraction tool

- [x] Refuses any disc that fails upstream's identity check, before writing anything.
- [x] Keeps original names, on-disc order and every LBA in `.nxdisc/manifest.tsv`.
- [x] Refuses overlapping extents, non-zero padding, unsafe names and names differing only by case.
- [x] Never removes files it did not write; overwriting a different-size disc file needs `--replace`.
- [x] Real disc rebuilds to the image's SHA-256.

## M03-02, M03-03 — Formats and coverage

- [x] Every disc format catalogued with its run-time owner and structural checks; 0 failures.
- [x] Private per-file SHA-256 manifest and a metadata-only coverage map.
- [x] Unsupported members and boot-critical gaps documented.

## M03-04, M03-05, M03-06 — Folder backend

- [x] `findFile`, `readBytes`, `totalSectors`, `listDirectory`, with listing in on-disc order.
- [x] Normalizes device prefix, separators, case and `;N` versions; rejects traversal, stray devices and bad versions.
- [x] Load refuses bad versions, unsafe or duplicate paths, overlaps, wrong sizes and missing files, naming the problem.
- [x] Reads across files, padding, gaps and raw sectors equal the real image.
- [x] End-of-disc reads zero-fill and return false, as `vcs::IsoImage` does.

## M03-07 — Concurrency

- [x] Bounded pool of 8 open files, no shared seek state; concurrent readers on the native build.
- [x] Upstream's stream reader never cancels a request, recorded in VERIFICATION.md.
- [x] Shutdown with reads pending on real hardware (M03-12): 21.3 ms join and close.

## M03-08 — Differential comparison

- [x] Boot, movies, cutscene, mainland and common streaming, radio music: 0 live mismatches.
- [x] All 341 files, boundaries, unaligned and random reads against the real image: 0 failures.
- [ ] Live `BEACH` and `MALL` level loads. Not reached by the scenario; covered only by byte comparison. M06 play exercises them.

## M03-09 — No ISO

- [x] The game reaches gameplay on desktop with no image opened.
- [x] ELF comes from the extracted tree; no staging or NRO embedding.
- [ ] Switch boot path uses it. M04 owns that code.

## M03-10, M03-13 — Paths

- [x] `resolveGamePaths` puts settings, cache and cards under `gamefolder/userdata`, and needs an existing asset root.
- [x] `rootedGamePath` strips `sdmc:`, keeps rooted and relative paths, refuses other devices and `..`.
- [x] `sdmc:` behaviour proven on hardware (M03-12).

## M03-11 — Mods and failures

- [x] Same-size file at `mods/<disc path>` replaces the disc file; other sizes are refused with a named warning.
- [x] Missing and wrong-size assets fail at load with the file named; a shortened file fails its read, logged and zero-filled.

## M03-12 — Hardware

- [x] Whole-disc digest through `FolderDisc` on the SD card.
- [x] SD case behaviour, concurrent reads, handle bound.
- [x] Clean shutdown during pending reads.

Results: [HARDWARE_RESULTS.md](HARDWARE_RESULTS.md), 2026-11-12, 0 failures.
The harness is `src/core_tests/disc_main.cpp`; `tools/switch_m03_session.py` copies the
tree and runs it.
