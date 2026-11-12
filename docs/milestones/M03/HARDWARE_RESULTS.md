# M03 real-Switch results

All sessions on 2026-11-12, on the user's Switch (HOS 22.5.0, `hosversionGet` = `00160500`,
Atmosphere), launched through Sphaira nxlink in application mode (`applet=0`), driven by
`tools/switch_m03_session.py run <evidence-dir> --suite disc|runtime`. No emulator result is
used. Records stay outside Git under `build/evidence/M03/`; each folder's `transcript.txt`
holds every step with its hash, the SD read-back of the payload and the crash-report
listings before and after.

These results cover the folder disc, the game paths and the public runtime suites only.
No game code ran.

## Final session: payloads from `54f814b`

Both payloads were built by `tools/build_core_tests.py` from clean commit
`54f814b8a48bf038733f50e86a9dfa1e3fea058e` (bundle manifest `root_dirty=false`, copied to
`bundle-manifest-54f814b.json`, SHA-256 `bfa31c88...f04eb`; `public-build-54f814b.log`).
It holds the SHA-256 verify mode (`c8856a7`), rooted-only game paths and the end-of-disc
empty read (`27274a3`) and the `std::filesystem` removal probe (`54f814b`). Later commits
change only M03 documents, `docs/MILESTONES.md` and `evidence.json`, none of which is a
payload input (the bundle manifest's input list).

| Payload | SHA-256 (local and SD read-back) | Bytes | Log SHA-256 | Result |
|---|---|---:|---|---|
| `gtavcs_disc_tests.nro` | `e55e04e99a9270713a444c55666e5b81bd3e37c9f73ff7c5a0e357a1bdc68b74` | 354,501 | `disc-tests-M03.log` `78a1b76b627f1e6ca2ee26ae369f93793edf8a4d9975ce7efbb205f4d4315175` | `failures=0` |
| `gtavcs_runtime_tests.nro` | `5139078ddcce54c0b0f6eadd08dcd1f5219b4c01b64908b6f26394f9b95eada7` | 3,369,157 | `runtime-tests-M02.log` `0c06b23f4636d3becd19deaaaf0f0ea3f1053459a21f356e20b4e590288a115e` | 337/337, `failures=0`, `result=0` |

Evidence folders: `hardware-54f814b-disc/` and `hardware-54f814b-runtime/`. Both
transcripts end `log finished failures=0 tests_pass=True` and `session end ok=True`. Crash
reports: 130 entries before and after each run, identical listings. The SD game folder is
the one checked over FTP before the first session (`hardware/`: 343 files, 2,162,118,749
bytes); its top-level listing hash is unchanged (`07fb34f4...2ad69`, 27 entries).

The runtime log is byte-identical to the `c8856a7` session's (`0c06b23f...`): the
transcript shows the earlier log archived and deleted (`250`) before the launch, and the
suite's output, including its `[resources]` lines, is deterministic. The runtime changes
between the two payloads (rooted paths, the empty read) print no line of their own.

**Timing.** The session tool now records when FTP is reachable again after the launch.
For the disc suite it was 118 s, with the log already complete (1,950 bytes); the suite's
timed steps add up to about 117 s. For the runtime suite it was 17 s. So in this session
the payload ran and exited, and Sphaira's FTP returned at once. The 496 s settle of the
`c8856a7` disc session is not explained by its transcript; this session bounds the normal
case and shows no such gap. Verify took 74.3 s here, against 59.0 s and 98.0 s in the
`c8856a7` and `27274a3` sessions, at the same 68.0 MiB/s whole-disc read rate, so the
spread is in the hashing pass and was not investigated further.

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
[fs-probe] rooted created=1 listed=2 remove_file=1/0 remove_empty_dir=1/0 rmdir=0/0 remove_all=-1/5 left=file:0,sub:1,tree:1
PASS rooted create, list, file remove and rmdir work
[fs-probe] relative created=1 listed=2 remove_file=1/0 remove_empty_dir=1/0 rmdir=0/0 remove_all=-1/5 left=file:0,sub:1,tree:1
[load] seconds=0.300 sectors=1066336 mods=0 warnings=0
PASS lowercase request finds the disc file
[case] lowercase host open of SYSTEM.CNF: found
[random] readers=4 reads=4800 size=65536 seconds=11.6
PASS concurrent random reads stay consistent
[whole-disc] bytes=2183856128 seconds=30.6 MiB/s=68.0 sha256=981259b841fb2567b06d3c6e16deeb955a2c935526b9ce8d2862bc455d8a7740
PASS every sequential read succeeded
PASS whole disc through FolderDisc equals the manifest digest
PASS portable SHA-256 equals the libnx digest
[verify] problems=0 seconds=74.3
PASS every disc file and raw.bin match the manifest SHA-256
[shutdown] reads=91 join_and_close_ms=8.6
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
Switch's SD card, together with M02's CPU and runtime suites. On the Switch its game-path
cases use a root built from `getcwd`, which there starts with `sdmc:`, so they also cover
stripping the device from a real working directory.

## `std::filesystem` removal on the Switch

The `[fs-probe]` lines record each step on a rooted path and on a cwd-relative one, under
the `m03-disc` test profile's cache folder. Each field is result/error code.

| Step | Rooted | Relative |
|---|---|---|
| `create_directories`, then a recursive listing | works, 2 entries | works, 2 entries |
| `fs::remove` of a file | works | works |
| `fs::remove` of an empty directory | works | works |
| `rmdir` | not needed (directory already removed) | same |
| `fs::remove_all` of a folder with one subfolder and one file | returns -1, error 5 (EIO); the file is gone, both folders remain | same |

So `std::filesystem::remove_all` fails on the Switch whatever the path; the path form is
not the cause. That corrects the reading of the `c5903d1` failure below. Why `remove_all`
fails after removing the file was not established; `fs::remove` of the same now-empty
directories works when called on its own. The port's own code uses no `remove_all`; the
synthetic suite already removes its mods with checked POSIX calls. Upstream's memory-card
format and unformat paths (`ps2xRuntime/src/lib/Kernel/Stubs/MemoryCard.cpp:1372`,
`:1936`) call `std::filesystem::remove_all(root, ec)` and ignore the error, so on the
Switch they would leave part of an old card behind; M04-13, which routes the cards, now
requires a replacement and a hardware check.

The `27274a3` disc session (`hardware-27274a3-disc/`, `failures=1`) is where this was
found: its first probe asserted that a rooted `remove_all` works and failed with
`removed=-1 error=5`. No runtime session ran on `27274a3` payloads.

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
  closed, in 8.6 ms after 91 reads (6.4 ms to 23.2 ms in earlier sessions).
- **`sdmc:` paths (M03-13).** `resolveGamePaths` turns the device-prefixed root into
  `/switch/gtavcs_nx/gamefolder`, which `std::filesystem` treats as absolute; the same file
  is reached both ways. `rootedGamePath` accepts only rooted paths, so no runtime path
  depends on the cwd; the probe above shows the rooted `std::filesystem` operations the
  port relies on work. The proof is that stripping and the `is_absolute` check. The
  check `no cwd-doubled device folder` is only a leftover sentinel: the harness performs
  no `std::filesystem` operation on a device-prefixed path, so it could not create one,
  and that check cannot fail on its own. The harness resolves `GamePaths` with the
  `m03-disc` test profile, and the suite checks that the player's `userdata` is left
  untouched.
- **Synthetic suite on the card (M03-05, M03-10, M03-11).** All rejected layouts, mods,
  corruption, empty files, game paths and test profiles pass on the Switch file system.
- **Integrity check (M03-11).** `FolderDisc::verifyFiles` hashed all 341 disc files and
  `raw.bin` on the SD card against the manifest's SHA-256 in 74.3 s (59.0 s to 98.0 s over
  three sessions with the portable SHA-256 on the Switch CPU) and found no problem. The portable digest
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
| `hardware-c8856a7-disc/` | `c8856a7` | Disc suite `failures=0` (payload `1e06ec97...c994`, log `8735ce4b...4ddb6`), verify 59.0 s. Its log settled 496 s after launch against about 105 s of timed work; the transcript cannot say why. Superseded: `27274a3` and `54f814b` changed paths, the empty read and the probe. |
| `hardware-c8856a7-runtime/` | `c8856a7` | Runtime suite 337/337, `failures=0` (payload `16f9e130...e4752`, log `0c06b23f...115e`). Superseded likewise. |
| `hardware-27274a3-disc/` | `27274a3` | Disc suite `failures=1`: `FAIL rooted create, list, remove_all and exists work`, `removed=-1 error=5`. Found the `remove_all` failure above. The first attempt in this folder was refused by FTP and is recorded as `session error ConnectionRefusedError`. |
| `hardware-9e020e7-disc/` | `9e020e7` | Disc suite `failures=0` (payload `9cf1673a...11cd`, log `5c5be31c...1503`), shutdown 6.4 ms. Superseded: `c8856a7` added the verify mode. Each `9e020e7` folder's transcript shows a repeated first `payload local sha256=` line (10:45:48 then 10:46:05, 10:47:20 then 10:53:47): an FTP connection was refused before Sphaira was ready, and the tool then stopped with a traceback, so the refusal itself is not in the transcript. |
| `hardware-9e020e7-runtime/` | `9e020e7` | Runtime suite 337/337, `failures=0` (payload `56a15dbe...956f`, log `3ade1ced...60c3`), 16 rejected layouts. Superseded likewise. |
| `hardware-final-runtime/` | `1dced20` | Runtime suite 337/337, `failures=0` (payload `96067dc4...dbbed`, log `32b00159...fd08a9`), 15 rejected layouts. Superseded likewise. |

**The `c5903d1` failure was a real test defect found only on the Switch.** The suite removed
its synthetic `mods/` folder with `std::filesystem::remove_all` on a cwd-relative path. On
the Switch that left the mod in place; an FTP listing after the run showed `mods/F0.BIN`
still present, so the later empty-files check loaded the stale mod and read changed data at
sector 8. The failure was first read as a relative-path hazard. The `54f814b` probe shows
`remove_all` fails with EIO on rooted paths too, so the cause is the call, not the path.
The `c5903d1` call's error code was not recorded.
Commit `0208213` removes the mods one by one with checked POSIX calls and clears stale mods
before building the fixture; the final session passes.

## Limits

- No game code ran; M04-13 connects `FolderDisc` and `GamePaths` to the real boot and
  proves the runtime's use of the userdata paths.
- Live loads of the BEACH and MALL levels are not exercised (moved to M14-02).
