# M03 real-Switch results

Session on 2026-11-12, on the user's Switch (HOS 22.5.0, `hosversionGet` = `00160500`,
Atmosphere), launched through Sphaira nxlink in application mode (`applet=0`). Driven by
`tools/switch_m03_session.py`; no emulator result is used. Records stay outside Git in
`build/evidence/M03/hardware/`; `transcript.txt` holds every step with its hash.

These results cover the folder disc and game paths on the SD card only. No game code ran.

## Payload and data identity

| Item | Value |
|---|---|
| Payload | `gtavcs_disc_tests.nro`, 309,445 bytes |
| Payload SHA-256 (local and SD read-back) | `3d08533cc7955ea2c0b163f639ab4258a0deab6cba40cd05f166d36de7836326` |
| Built from | clean commit `4370dcea0b2a2c61fb846ba76e11bb45ef0a754b` (`root_dirty=false`); later commits change docs only |
| Log `disc-tests-M03.log` SHA-256 | `f5a764714db5e0cccddf0948b867144ab4ad39598816300bd1fb83c7bd84ed66` |
| SD game folder | 343 files, 2,162,118,749 bytes: every manifest file plus `.nxdisc/manifest.tsv` and `raw.bin`, sizes checked over FTP before the run |
| New crash reports | none (130 entries before and after) |

The user copied the folder to the card with robocopy, leaving out desktop memory cards.
The FTP size check then sent 50,625 bytes that the copy had missed.

## Results

```text
[disc-tests] HOS=00160500 applet=0
PASS resolve device-prefixed game folder
PASS game folder rooted
PASS rooted path is absolute to std::filesystem
PASS device path is the known relative case
PASS std::filesystem finds a rooted file
PASS same file reached through the device path
PASS no cwd-doubled device folder
[load] seconds=0.398 sectors=1066336 mods=0 warnings=0
PASS lowercase request finds the disc file
[case] lowercase host open of SYSTEM.CNF: found
[random] readers=4 reads=4800 size=65536 seconds=11.6
PASS concurrent random reads stay consistent
[whole-disc] bytes=2183856128 seconds=30.9 MiB/s=67.4 sha256=981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740
PASS every sequential read succeeded
PASS whole disc through FolderDisc equals the manifest digest
[shutdown] reads=90 join_and_close_ms=21.3
PASS readers stop and the disc closes during pending reads
[disc-tests] finished failures=0
```

## What each result establishes

- **Whole disc (M03-12).** Every sector read through `FolderDisc` from the SD card hashes
  to the source image's SHA-256, the same value as the desktop rebuild. 67.4 MiB/s
  sequential with 1 MiB reads.
- **Case (M03-12).** The SD card's FAT/exFAT layer opens `system.cnf` for `SYSTEM.CNF`,
  so host opens are case-insensitive there. `FolderDisc` does not rely on that: it
  normalizes the request and opens the manifest's exact name.
- **Handle bound (M03-12).** `FolderDisc` holds at most `kMaxOpenFiles` = 8 handles in a
  fixed slot array (`src/vfs/folder_disc.h`); it cannot hold more by construction. The
  whole-disc pass opened all 341 files in turn through those slots with no failed read.
  The run did not count open handles separately.
- **Concurrency (M03-07, M03-12).** Four readers, 4,800 reads of 64 KiB each at random
  offsets, every pair of repeated reads identical.
- **Shutdown during pending reads (M03-07, M03-12).** Four readers mid-read stopped, were
  joined and the disc closed in 21.3 ms.
- **`sdmc:` paths (M03-13).** `resolveGamePaths` turns the device-prefixed root into
  `/switch/gtavcs_nx/gamefolder`, which `std::filesystem` treats as absolute; the same
  file is reached both ways and no `sdmc:` folder appears under the working directory.
- **Memory.** Allocator use stayed at 78,656 bytes in use at the end of the run.

## Limits

- No game code ran; M04 connects `FolderDisc` and `GamePaths` to the real boot.
- Live loads of the BEACH and MALL levels are not exercised here (see TASKS.md).
