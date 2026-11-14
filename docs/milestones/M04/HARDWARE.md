# M04 consolidated Switch session

One session after all implementation and host/desktop proof. Results go to
[HARDWARE_RESULTS.md](HARDWARE_RESULTS.md); raw transcripts and logs stay in
`build/evidence/M04/hw-session/` (private: the game log names guest addresses).

Same device and route as M02/M03: Switch with Atmosphere, Sphaira opened through a
title override (application mode), FTP on port 5000, nxlink available. The
extracted game is already on the card in `sdmc:/switch/gtavcs_nx/gamefolder`
(M03). Set `NX_HOST` if the console's address changed.

## Payloads

| Payload | Built by | What it proves |
|---|---|---|
| `gtavcs.nro` (private, never committed) | `python tools/build_game.py` | The real game: runtime, game layer, SDL2 platform, folder disc, GamePaths |
| `gtavcs_runtime_tests.nro` (public) | `python tools/build_core_tests.py` | Upstream's runtime suites plus `host_fs_tests.cpp`: card format / unformat / rename onto an existing name, card backup, `replaceFile` twice |

## Sequence

Each `game` step writes the schedule to `gamefolder/userdata/nxrun.cfg`, sends the
NRO, waits for the game to end and Sphaira's FTP to return, reads everything
back and deletes `nxrun.cfg` again.

```bash
E=build/evidence/M04/hw-session
python tools/switch_m04_session.py preflight $E
python tools/switch_m04_session.py tests $E
python tools/switch_m04_session.py game $E tests/switch/m04-boot.cfg
python tools/switch_m04_session.py game $E tests/switch/m04-boot.cfg      # repeated launch, same test card
python tools/switch_m04_session.py game $E tests/switch/m04-files.cfg
python tools/switch_m04_session.py game $E tests/switch/m04-focus.cfg \
  --prompt "At about 30 s press HOME, wait 10 s, go back to the game. At about 70 s press the power button (sleep), wait 10 s, wake it and go back."
python tools/switch_m04_session.py game $E tests/switch/m04-close.cfg \
  --prompt "At about 40 s press HOME and choose Close. Then open Sphaira again (title override) so FTP comes back."
python tools/switch_m04_session.py game $E tests/switch/m04-badcfg.cfg --expect-failure "nxrun.cfg:3: unknown directive" \
  --prompt "When the system error dialog appears, read it and press OK."
python tools/m04_compare.py build/evidence/M04/reference/reference-results.txt $E/m04-boot-results.txt
```

The person at the Switch only acts where a step prints `ACTION FOR THE PERSON AT
THE SWITCH`. Until M05 the screen stays black while the game runs (null renderer).
A boot that fails after SDL has started (the bad-configuration step) shows the system's error dialog, which
stays up until someone presses OK; a failure before SDL starts (no gamefolder) keeps the 15-second text screen.
The libnx console cannot draw once Mesa EGL has used the display window (2345-0021 in framebufferBegin, first
M04 session), so the post-SDL path must not use it.

## Required observations

- Preflight: no disc image under `/switch/gtavcs_nx`; manifest and ELF present.
- Runtime suites: `failures=0`, `[host-fs] failures=0`, SD payload hash equals the local one, no new crash report.
- Boot (twice): the ELF SHA-1 line; `cdImage=(none)`; progress lines with game frames and EE cycles rising; the
  schedule's area/player answers reaching outdoors (area 0) and a follow camera as on the desktop reference;
  `stop reason=seconds`; `audit ... iso_opens=0`; live threads at shutdown back to the boot count;
  `[shutdown] core, context, window and SDL closed`; the test card's files unchanged by the second launch.
- Files: `verify problems=0`; the schedule's `setting display.fps` answers `ok 30` then `ok 60`; `vcs.ini` in
  `userdata/tests/m04-files` holding `audio.volume=90` after the three saves (each after the first replaces the file).
- Focus: `focus state=out` / `state=in` pairs, progress resuming after each, a clean end.
- Close: the orderly shutdown lines (game thread joined, stream reader stopped, folder disc closed) after the
  applet's exit request, with no new crash report.
- Bad configuration: the boot fails naming `nxrun.cfg:3`, the system error dialog naming it (the log's
  `failure shown in the system error dialog (00000000)`), no new crash report, a clean return to Sphaira.

Missing or failed observations keep their tasks open.
