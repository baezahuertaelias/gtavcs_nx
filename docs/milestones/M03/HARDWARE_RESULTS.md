# M03 real-Switch results

All sessions on 2026-11-12, on the user's Switch (HOS 22.5.0, `hosversionGet` = `00160500`,
Atmosphere), launched through Sphaira nxlink in application mode (`applet=0`), driven by
`tools/switch_m03_session.py run <evidence-dir> --suite disc|runtime`. No emulator result is
used. Records stay outside Git under `build/evidence/M03/`; each folder's `transcript.txt`
holds every step with its hash, the SD read-back of the payload and the crash-report
listings before and after.

These results cover the folder disc, the game paths and the public runtime suites only.
No game code ran.

## Final session: payloads from `c8856a7`

Both payloads were built by `tools/build_core_tests.py` from clean commit
`c8856a71d937b39f86fbb158c40d82f5c6ffa5d3` (bundle manifest `root_dirty=false`, copied to
`bundle-manifest-c8856a7.json`, SHA-256 `ed09fcb8...d17c73`;
`public-build-c8856a7.log`). That commit adds the SHA-256 verify mode (third review). Later
commits change only M03 documents, `docs/MILESTONES.md`, `evidence.json`,
`tools/extract_disc.py`, its test and `tools/switch_m03_session.py`, none of which is a
payload input (the bundle manifest's input list).

| Payload | SHA-256 (local and SD read-back) | Bytes | Log SHA-256 | Result |
|---|---|---:|---|---|
| `gtavcs_disc_tests.nro` | `1e06ec972a58825d8935bf00ab9b47b9fdb93b4b6a802ef9a1c92379ceb1c994` | 325,829 | `disc-tests-M03.log` `8735ce4bda9f7a2d05a28ee7415f976275421593fa4b56ed7ac8a8bb08c4ddb6` | `failures=0` |
| `gtavcs_runtime_tests.nro` | `16f9e130df4f6ebda14c8c4f298d9dda44508899b8862e31ccbfdf65b01e4752` | 3,365,061 | `runtime-tests-M02.log` `0c06b23f4636d3becd19deaaaf0f0ea3f1053459a21f356e20b4e590288a115e` | 337/337, `failures=0`, `result=0` |

Evidence folders: `hardware-c8856a7-disc/` and `hardware-c8856a7-runtime/`. Both
transcripts end `log finished failures=0 tests_pass=True` and `session end ok=True`. Crash
reports: 130 entries before and after each run, identical listings. The SD game folder is
the one checked over FTP before the first session (`hardware/`: 343 files, 2,162,118,749
bytes); its top-level listing hash is unchanged (`07fb34f4...2ad69`, 27 entries).

The disc transcript saw the log settle 496 s after the launch, while the suite's own timed
steps add up to about 105 s, and earlier disc sessions settled about 20 s after their
timed work. The cause was not identified: the transcript cannot tell a stall in the payload
from Sphaira's FTP returning late. The log itself is complete and ends with
`finished failures=0`. From `c796a19` the session tool records when FTP becomes reachable
after launch and when the log first appears, so a later session can tell the two apart.

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
[load] seconds=0.435 sectors=1066336 mods=0 warnings=0
PASS lowercase request finds the disc file
[case] lowercase host open of SYSTEM.CNF: found
[random] readers=4 reads=4800 size=65536 seconds=11.4
PASS concurrent random reads stay consistent
[whole-disc] bytes=2183856128 seconds=30.6 MiB/s=68.0 sha256=981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740
PASS every sequential read succeeded
PASS whole disc through FolderDisc equals the manifest digest
PASS portable SHA-256 equals the libnx digest
[verify] problems=0 seconds=59.0
PASS every disc file and raw.bin match the manifest SHA-256
[shutdown] reads=92 join_and_close_ms=17.3
PASS readers stop and the disc closes during pending reads
[disc-tests] finished failures=0
```

### Runtime suite log (relevant lines)

```text
Total Tests: 337
Passed: 337
PASS folder disc: paths, listing, whole-image and unaligned reads, end of disc, 4 readers over 12 files, mods, corrupted file, SHA-256 verify, empty files, game paths, 19 rejected layouts
M02 runtime suites finished failures=0
[runtime-tests] worker_stack_bytes=8388608 launch_result=00000000 result=0
```

The runtime payload runs the synthetic FolderDisc/GamePaths suite (VERIFICATION.md) on the
Switch's SD card, together with M02's CPU and runtime suites, including the one-byte
corruption cases for the verify mode.

## What the results establish

- **Whole disc (M03-12).** Every sector read through `FolderDisc` from the SD card hashes
  to the source image's SHA-256, the same value as the desktop rebuild; 68.0 MiB/s
  sequential with 1 MiB reads.
- **Case (M03-12).** The SD card's file system opens `system.cnf` for `SYSTEM.CNF`, so host
  opens are case-insensitive there. `FolderDisc` does not rely on that: it normalizes the
  request and opens the manifest's exact name.
- **Handle bound (M03-12).** Reads hold at most `kMaxOpenFiles` = 8 handles in a fixed
  slot array (`src/vfs/folder_disc.h`). Two other paths open one transient handle of their
  own outside that array: `load` (the manifest, then `raw.bin`) and `verifyFiles` (one
  file at a time). So the bound is 8 read handles plus one while loading or verifying,
  by construction. The whole-disc pass opened all 341 files in turn through the slots
  with no failed read, and the synthetic suite ran four readers over twelve files on the
  card. Open handles were not counted on hardware.
- **Concurrency (M03-07, M03-12).** Four readers, 4,800 reads of 64 KiB at random offsets;
  every pair of repeated reads identical.
- **Shutdown with reads pending (M03-07, M03-12).** The harness sets a stop flag while four
  readers are reading; each finishes its current read, then all are joined and the disc
  closed, in 17.3 ms after 92 reads (6.4 ms and 23.2 ms in the `9e020e7` and `1dced20`
  sessions).
- **`sdmc:` paths (M03-13).** `resolveGamePaths` turns the device-prefixed root into
  `/switch/gtavcs_nx/gamefolder`, which `std::filesystem` treats as absolute; the same file
  is reached both ways. The proof is that stripping and the `is_absolute` check. The
  check `no cwd-doubled device folder` is only a leftover sentinel: the harness performs
  no `std::filesystem` operation on a device-prefixed path, so it could not create one,
  and that check cannot fail on its own. The harness resolves `GamePaths` with the `m03-disc` test profile, and the suite checks that
  the player's `userdata` is left untouched.
- **Synthetic suite on the card (M03-05, M03-10, M03-11).** All rejected layouts, mods,
  corruption, empty files, game paths and test profiles pass on the Switch file system.
- **Integrity check (M03-11).** `FolderDisc::verifyFiles` hashed all 341 disc files and
  `raw.bin` on the SD card against the manifest's SHA-256 in 59.0 s (about 35 MiB/s with
  the portable SHA-256 on the Switch CPU) and found no problem. The portable digest
  equals libnx's `sha256CalculateHash` on a 3 MiB buffer. Load still checks sizes only;
  the verify mode is the explicit check that finds a same-size corrupt file, and wiring
  it into the Switch boot as a user option is M04-13.
- **Memory.** Allocator use went from 75,728 bytes in use at the start of the disc suite to
  78,656 at its end; bytes obtained from the system rose from 81,920 to 1,220,608, which
  includes the 1 MiB verify buffer (the omitted `[resources]` lines).
- **Session result.** At `1dced20` the session tool passed a run on the payload hash and the
  crash listing only, so the `c5903d1` runtime session with `failures=2` still ended
  `session end ok=True`. From `9e020e7` the tool also requires exactly one
  `finished failures=0` line in the log; both final transcripts record
  `tests_pass=True`. From `c8856a7` it also writes connection errors to the transcript
  instead of stopping with a traceback.

## Earlier sessions, kept for the record

| Folder | Payload commit | Result |
|---|---|---|
| `hardware/` | `4370dce` | Disc suite `failures=0` (log `f5a76471...84ed66`). Superseded: the review changed `FolderDisc` afterwards. |
| `hardware-c5903d1-disc/` | `c5903d1` | Disc suite `failures=0`. |
| `hardware-c5903d1-runtime/` | `c5903d1` | Runtime suite `failures=2`: `FAIL folder disc: empty files hide no sectors`, twice. |
| `hardware-final-disc/` | `1dced20` | Disc suite `failures=0` (payload `c10537c3...fde2f5b`, log `2096daa7...9cc9435`). Superseded: `9e020e7` changed `FolderDisc`, the synthetic suite and the harness. |
| `hardware-9e020e7-disc/` | `9e020e7` | Disc suite `failures=0` (payload `9cf1673a...11cd`, log `5c5be31c...1503`), shutdown 6.4 ms. Superseded: `c8856a7` added the verify mode. Each `9e020e7` folder's transcript shows a repeated first `payload local sha256=` line (10:45:48 then 10:46:05, 10:47:20 then 10:53:47): an FTP connection was refused before Sphaira was ready, and the tool then stopped with a traceback, so the refusal itself is not in the transcript. |
| `hardware-9e020e7-runtime/` | `9e020e7` | Runtime suite 337/337, `failures=0` (payload `56a15dbe...956f`, log `3ade1ced...60c3`), 16 rejected layouts. Superseded likewise. |
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
