# M07 review 4 (candidate `5afb432`): REQUEST_CHANGES, and what was done

Reopened M07-05, 08, 09 (`cde38d5`), corrected, ticked again. Text and tool only: no code of the game or the runtime
changed, no new NRO, no new hardware run.

| # | Finding | Resolution |
|---|---|---|
| 1 | resume-handheld's "0 underruns" wrong; the skip omitted | TASKS M07-09, evidence.json and REVIEW3_RESPONSE row 2: 3 underruns, all in windows 0-2 (the two movie windows and a 1,850-frame, 39 ms, hand-over after INTRO.PSS), none after any of the four resumes. TASKS M07-08 and evidence.json: the person skipped INTRO.PSS with Cross (SDL button a, the south position) at 56.3 s, not asked for (367 pictures, 0 dropped) |
| 2 | No person-observations entry; "the user asked" | person-observations.txt: what the person was asked (the session prompt verbatim), what the log shows they did, and that they reported nothing and were not asked afterwards. "The person was asked" in TASKS and REVIEW3_RESPONSE |
| 3 | NRO provenance not recorded | TASKS.md, before the task table: `e3583aa2` from `8f4f337` (series 08a801a8..., root_dirty from the untracked m07-resume.cfg written during the build, committed in `f5eca27`; the build does not read tests/), `6976c8d7` from `ec397f3` (series 085c59f1...; its manifest overwritten); the sources between them; the hash difference at equal size from the overlay folder's name (with the series hash) in the ELF, 162 occurrences of `game-source-08a801a89bd07492` in gtavcs.elf (6976c8d7's ELF no longer exists to compare) |
| 4 | readfail tool | The REFUSED path quits its connection; the cleanup's reconnect and removals are guarded, a failed cleanup says so, keeps `mods left=True` and fails the run without hiding the game run's return code |
| 5 | loop-command.md's limitation | Names indirect calls (`jalr`, function-pointer tables) to 0x10DD50 as outside the scan, and that the radio log's 168 commands agree with it |
