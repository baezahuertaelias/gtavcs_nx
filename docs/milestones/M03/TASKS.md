# M03 execution checklist

Branch `milestone/m03-folder-vfs`. Parent task status lives in
[docs/MILESTONES.md](../../MILESTONES.md). A step here is ticked only when its proof
exists. No parent task is closed yet. Nothing in this file has run on a Switch.

## Disc layout findings

Measured on the user's image, 2026-11-09:

| Measure | Value |
|---|---:|
| Sectors | 1,066,336 |
| Files / directories | 341 / 29 |
| File extents overlapping | 0 |
| Non-zero padding after file data | 0 files |
| Non-file sectors holding data | 6 spans, 935,936 bytes |

The local image is byte-identical when rebuilt from the folder form, so sector reads
need no ISO. Non-file sectors are volume descriptors, path tables, directories and
some padding. They are private disc data and stay in `gamefolder/.nxdisc/raw.bin`.

## M03-01 — Extraction tool

- [x] `tools/extract_disc.py` refuses any disc that fails upstream's identity check, before writing anything.
- [x] Keeps original names, on-disc directory order and every LBA in `.nxdisc/manifest.tsv`.
- [x] Refuses overlapping extents, non-zero padding, unsafe names and names differing only by case.
- [x] Never removes files it did not write; overwriting a different-size disc file needs `--replace`.
- [x] Eight synthetic-disc tests in `tools/test_extract_disc.py` pass.
- [x] Real disc: `verify --iso` rebuilt SHA-256 `981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740`, equal to the image.

## M03-03 — Manifest and coverage

- [x] Private per-file SHA-256 manifest, with file sizes and extents.
- [ ] Metadata-only coverage map committed.
- [ ] Unsupported members documented; each boot-critical format gap resolved (needs M03-02).

## M03-04, M03-05, M03-06 — Folder backend

`src/vfs/folder_disc.{h,cpp}` implements `IPS2DiscImage`.

- [x] `findFile`, `readBytes`, `totalSectors`, `listDirectory`, with listing in on-disc order.
- [x] Normalizes device prefix, separators, case and `;N` versions.
- [x] Rejects traversal, stray device names and malformed versions.
- [x] Load refuses bad versions, unsafe or duplicate paths, overlaps, wrong sizes and missing files, naming the problem.
- [x] Reads across files, padding, gaps and raw sectors match an in-memory reference on the native build.
- [x] End-of-disc reads zero-fill and return false, as `vcs::IsoImage` does.
- [x] Builds for the Switch.
- [ ] Differential comparison through this C++ code against the real ISO (M03-08).
- [ ] Run on a real Switch (M03-12).

## M03-07 — Concurrency

- [x] Bounded pool of 8 open files, no shared seek state, 4 concurrent readers over 12 files on the native build.
- [ ] Async CDVD/IOP streaming completion and cancellation.
- [ ] Clean shutdown during pending reads (M03-12).

## Not started

M03-02, M03-08 to M03-11, M03-13. `src/main.cpp` still looks for an ISO, and no
runtime code uses `FolderDisc` yet; wiring belongs to M03-09 and M04.

The SD card's `gamefolder` holds no game data yet. Copying the extracted 2.0 GiB tree
is needed for M03-12.
