# M03 real-Switch results

All sessions on 2026-11-12, on the user's Switch (HOS 22.5.0, `hosversionGet` = `00160500`,
Atmosphere), launched through Sphaira nxlink in application mode (`applet=0`), driven by
`tools/switch_m03_session.py run <evidence-dir> --suite disc|runtime`. No emulator result is
used. Records stay outside Git under `build/evidence/M03/`; each folder's `transcript.txt`
holds every step with its hash, the SD read-back of the payload and the crash-report
listings before and after.

These results cover the folder disc, the game paths and the public runtime suites only.
No game code ran.

## Final session: payloads from `9e020e7`

Both payloads were built by `tools/build_core_tests.py` from clean commit
`9e020e7ed1e9a455f26a36041a8540c726dd9d3d` (bundle manifest `root_dirty=false`, copied to
`bundle-manifest-9e020e7.json`, SHA-256 `c445b531...0a22df`;
`public-build-9e020e7.log`). Later commits change only M03 documents and `evidence.json`,
none of which is a payload input.

| Payload | SHA-256 (local and SD read-back) | Bytes | Log SHA-256 | Result |
|---|---|---:|---|---|
| `gtavcs_disc_tests.nro` | `9cf1673ad2e55df359208fdbd58de55cf94c7c86f2aadbd126482df188c911cd` | 313,541 | `disc-tests-M03.log` `5c5be31ca0c178720dfe7a5cac4f0ad34b9c1031729d8607e5dcb97324fc1503` | `failures=0` |
| `gtavcs_runtime_tests.nro` | `56a15dbec46560c355e970b5b97b59067b8d7c70c51e4671541858c3e619956f` | 3,344,581 | `runtime-tests-M02.log` `3ade1ceded68d969d2e022a73b27073031ad4d6b3d95f534be436fafdd5160c3` | 337/337, `failures=0`, `result=0` |

Evidence folders: `hardware-9e020e7-disc/` and `hardware-9e020e7-runtime/`. Both
transcripts end `log finished failures=0 tests_pass=True` and `session end ok=True`. Crash
reports: 130 entries before and after each run, identical listings. Each folder's first
connection attempt was refused because Sphaira's FTP server was not running yet; the
transcript records it, and nothing was sent until the user restarted Sphaira. The SD game
folder is the one checked over FTP before the first session (`hardware/`: 343 files,
2,162,118,749 bytes); its top-level listing hash is unchanged
(`07fb34f4...2ad69`, 27 entries).

### Disc suite log (the two `[resources]` lines omitted)

```text
[disc-tests] HOS=00160500 applet=0
PASS resolve device-prefixed game folder
PASS game folder rooted
PASS test profile keeps the player's userdata untouched
PASS rooted path is absolute to std::filesystem
PASS device path is the known relative case
PASS std::filesystem finds a rooted file
PASS same file reached through the device path
PASS no cwd-doubled device folder
[load] seconds=0.466 sectors=1066336 mods=0 warnings=0
PASS lowercase request finds the disc file
[case] lowercase host open of SYSTEM.CNF: found
[random] readers=4 reads=4800 size=65536 seconds=11.4
PASS concurrent random reads stay consistent
[whole-disc] bytes=2183856128 seconds=30.6 MiB/s=68.2 sha256=981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740
PASS every sequential read succeeded
PASS whole disc through FolderDisc equals the manifest digest
[shutdown] reads=91 join_and_close_ms=6.4
PASS readers stop and the disc closes during pending reads
[disc-tests] finished failures=0
```

### Runtime suite log (relevant lines)

```text
Total Tests: 337
Passed: 337
PASS folder disc: paths, listing, whole-image and unaligned reads, end of disc, 4 readers over 12 files, mods, corrupted file, empty files, game paths, 16 rejected layouts
M02 runtime suites finished failures=0
[runtime-tests] worker_stack_bytes=8388608 launch_result=00000000 result=0
```

The runtime payload runs the synthetic FolderDisc/GamePaths suite (VERIFICATION.md) on the
Switch's SD card, together with M02's CPU and runtime suites. Its 16 rejected layouts
include the second sector count added in `9e020e7`.

## What the results establish

- **Whole disc (M03-12).** Every sector read through `FolderDisc` from the SD card hashes
  to the source image's SHA-256, the same value as the desktop rebuild; 68.2 MiB/s
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
  closed, in 6.4 ms after 91 reads (23.2 ms after 93 reads in the `1dced20` session).
- **`sdmc:` paths (M03-13).** `resolveGamePaths` turns the device-prefixed root into
  `/switch/gtavcs_nx/gamefolder`, which `std::filesystem` treats as absolute; the same file
  is reached both ways and no `sdmc:` folder appears under the working directory. The
  harness resolves `GamePaths` with the `m03-disc` test profile, and the suite checks that
  the player's `userdata` is left untouched.
- **Synthetic suite on the card (M03-05, M03-10, M03-11).** All rejected layouts, mods,
  corruption, empty files, game paths and test profiles pass on the Switch file system.
- **Memory.** Allocator use went from 75,728 bytes in use at the start of the disc suite to
  78,656 at its end (the omitted `[resources]` lines).
- **Session result.** At `1dced20` the session tool passed a run on the payload hash and the
  crash listing only, so the `c5903d1` runtime session with `failures=2` still ended
  `session end ok=True`. From `9e020e7` the tool also requires exactly one
  `finished failures=0` line in the log; both final transcripts record
  `tests_pass=True`.

## Earlier sessions, kept for the record

| Folder | Payload commit | Result |
|---|---|---|
| `hardware/` | `4370dce` | Disc suite `failures=0` (log `f5a76471...84ed66`). Superseded: the review changed `FolderDisc` afterwards. |
| `hardware-c5903d1-disc/` | `c5903d1` | Disc suite `failures=0`. |
| `hardware-c5903d1-runtime/` | `c5903d1` | Runtime suite `failures=2`: `FAIL folder disc: empty files hide no sectors`, twice. |
| `hardware-final-disc/` | `1dced20` | Disc suite `failures=0` (payload `c10537c3...fde2f5b`, log `2096daa7...9cc9435`). Superseded: `9e020e7` changed `FolderDisc`, the synthetic suite and the harness. |
| `hardware-final-runtime/` | `1dced20` | Runtime suite 337/337, `failures=0` (payload `96067dc4...dbbed`, log `32b00159...fd08a9`), 15 rejected layouts. Superseded likewise. |

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
