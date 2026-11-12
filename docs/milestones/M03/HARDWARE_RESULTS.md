# M03 real-Switch results

All sessions on 2026-11-12, on the user's Switch (HOS 22.5.0, `hosversionGet` = `00160500`,
Atmosphere), launched through Sphaira nxlink in application mode (`applet=0`), driven by
`tools/switch_m03_session.py run <evidence-dir> --suite disc|runtime`. No emulator result is
used. Records stay outside Git under `build/evidence/M03/`; each folder's `transcript.txt`
holds every step with its hash, the SD read-back of the payload and the crash-report
listings before and after.

These results cover the folder disc, the game paths and the public runtime suites only.
No game code ran.

## Final session: payloads from `1dced20`

Both payloads were built by `tools/build_core_tests.py` from clean commit
`1dced203888418592937316d184ba9af283471bf` (`root_dirty=false`,
`build/evidence/M03/public-build-1dced20.log`). Later commits change only M03 documents and
`evidence.json`, none of which is a payload input.

| Payload | SHA-256 (local and SD read-back) | Bytes | Log SHA-256 | Result |
|---|---|---:|---|---|
| `gtavcs_disc_tests.nro` | `c10537c3a9ff2cc772823a3499d186a8635053cae3a79f1bbe1c2cd36fde2f5b` | 313,541 | `disc-tests-M03.log` `2096daa74ca98053178a41b031cd7e693aa65eda80636f5b712a420723cc9435` | `failures=0` |
| `gtavcs_runtime_tests.nro` | `96067dc46727d8a5d0c1164eff0c29e4d6cf6359b2975c0384cfddc3ad4dbbed` | 3,344,581 | `runtime-tests-M02.log` `32b00159262f478264f07a9df3d10a63bb3f7d118b4ae9e69e69ace9cefd08a9` | 337/337, `failures=0`, `result=0` |

Evidence folders: `hardware-final-disc/` and `hardware-final-runtime/`. Crash reports: 130
entries before and after each run, identical listings. SD game folder: 343 files,
2,162,118,749 bytes, sizes checked over FTP before the first session (`hardware/`).

### Disc suite log

```text
[disc-tests] HOS=00160500 applet=0
PASS resolve device-prefixed game folder
PASS game folder rooted
PASS rooted path is absolute to std::filesystem
PASS device path is the known relative case
PASS std::filesystem finds a rooted file
PASS same file reached through the device path
PASS no cwd-doubled device folder
[load] seconds=0.455 sectors=1066336 mods=0 warnings=0
PASS lowercase request finds the disc file
[case] lowercase host open of SYSTEM.CNF: found
[random] readers=4 reads=4800 size=65536 seconds=11.4
PASS concurrent random reads stay consistent
[whole-disc] bytes=2183856128 seconds=30.6 MiB/s=68.1 sha256=981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740
PASS every sequential read succeeded
PASS whole disc through FolderDisc equals the manifest digest
[shutdown] reads=93 join_and_close_ms=23.2
PASS readers stop and the disc closes during pending reads
[disc-tests] finished failures=0
```

### Runtime suite log (relevant lines)

```text
Total Tests: 337
Passed: 337
PASS folder disc: paths, listing, whole-image and unaligned reads, end of disc, 4 readers over 12 files, mods, corrupted file, empty files, game paths, 15 rejected layouts
M02 runtime suites finished failures=0
[runtime-tests] worker_stack_bytes=8388608 launch_result=00000000 result=0
```

The runtime payload runs the synthetic FolderDisc/GamePaths suite (VERIFICATION.md) on the
Switch's SD card, together with M02's CPU and runtime suites.

## What the results establish

- **Whole disc (M03-12).** Every sector read through `FolderDisc` from the SD card hashes
  to the source image's SHA-256, the same value as the desktop rebuild; 68.1 MiB/s
  sequential with 1 MiB reads.
- **Case (M03-12).** The SD card's file system opens `system.cnf` for `SYSTEM.CNF`, so host
  opens are case-insensitive there. `FolderDisc` does not rely on that: it normalizes the
  request and opens the manifest's exact name.
- **Handle bound (M03-12).** `FolderDisc` holds at most `kMaxOpenFiles` = 8 handles in a
  fixed slot array (`src/vfs/folder_disc.h`), so it cannot hold more by construction. The
  whole-disc pass opened all 341 files in turn through those slots with no failed read,
  and the synthetic suite ran four readers over twelve files on the card. Open handles
  were not counted separately.
- **Concurrency (M03-07, M03-12).** Four readers, 4,800 reads of 64 KiB at random offsets;
  every pair of repeated reads identical.
- **Shutdown with reads pending (M03-07, M03-12).** The harness sets a stop flag while four
  readers are reading; each finishes its current read, then all are joined and the disc
  closed, in 23.2 ms.
- **`sdmc:` paths (M03-13).** `resolveGamePaths` turns the device-prefixed root into
  `/switch/gtavcs_nx/gamefolder`, which `std::filesystem` treats as absolute; the same file
  is reached both ways and no `sdmc:` folder appears under the working directory.
- **Synthetic suite on the card (M03-05, M03-10, M03-11).** All rejected layouts, mods,
  corruption, empty files, game paths and test profiles pass on the Switch file system.
- **Memory.** Allocator use went from 75,728 bytes in use at the start of the disc suite to
  78,656 at its end.

## Earlier sessions, kept for the record

| Folder | Payload commit | Result |
|---|---|---|
| `hardware/` | `4370dce` | Disc suite `failures=0` (log `f5a76471...84ed66`). Superseded: the review changed `FolderDisc` afterwards. |
| `hardware-c5903d1-disc/` | `c5903d1` | Disc suite `failures=0`. |
| `hardware-c5903d1-runtime/` | `c5903d1` | Runtime suite `failures=2`: `FAIL folder disc: empty files hide no sectors`, twice. |

**The `c5903d1` failure was a real test defect found only on the Switch.** The suite removed
its synthetic `mods/` folder with `std::filesystem::remove_all` on a cwd-relative path. On
the Switch that silently did nothing; an FTP listing after the run showed `mods/F0.BIN`
still present, so the later empty-files check loaded the stale mod and read changed data at
sector 8. This is the `sdmc:`/relative-path hazard M03-13 guards against in runtime code.
Commit `0208213` removes the mods one by one with checked POSIX calls and clears stale mods
before building the fixture; the final session passes.

## Limits

- No game code ran; M04-13 connects `FolderDisc` and `GamePaths` to the real boot and
  proves the runtime's use of the userdata paths.
- Live loads of the BEACH and MALL levels are not exercised (moved to M14-02).
