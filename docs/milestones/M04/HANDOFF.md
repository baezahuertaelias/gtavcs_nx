# M04 handoff (2026-11-12)

State of `milestone/m04-runtime-boot` when work moved off the original host.
No M04 task is ticked: every one still needs the Switch session in
[HARDWARE.md](HARDWARE.md). No hardware run has happened for M04.

## Done and pushed

- Core adapter behind `src/core.h` (`src/core/`), SDL2 platform adapter
  (`src/platform/`), libnx link wraps (`src/switch/`), `src/main.c` lifecycle.
- Upstream overlay `patches/upstream/0007-switch-runtime-boot.patch`
  (run split, launcher steps, Switch-safe file replace/remove, stream reader join).
- `tools/build_game.py` (private NRO / `--public` CI), schedules in
  `tests/switch/`, `tools/switch_m04_session.py`, `tools/m04_reference.py`,
  `tools/m04_compare.py`.
- Host tests (`tools/run_core_host_tests.py`) pass; the public CI build passes.
- Desktop null-RHI reference and the desktop build with all 7 overlay patches
  behave the same on `m04-boot.cfg` (area 2, then outdoors by 25 s, clean exit).
- Audits: `unhandled_classification.json`, `boot_os_audit.json`.

## Not in git (host-local, must be recreated on a new host)

Nothing private was committed. The new host needs, from the user's own disc:

1. The Docker image: `docker build -f Dockerfile.switch` (done by `build_game.py`).
2. The private generated EE/VU code and archive in Docker volume
   `gtavcs-nx-m02-core` (`/cache/generated/ee`, `/cache/generated/recompile_stamp.json`,
   `/cache/generated-build/libvcs_generated_cpu.a`), generated with upstream's
   host tooling (`upstream/tools/recompile.py`) from the pinned source as in M02.
   `build_game.py` refuses a stamp from another upstream commit.
3. The desktop reference results (`build/evidence/M04/reference/`): re-run
   `python tools/m04_reference.py` on a desktop build of the pristine pin.
4. Then `python tools/build_game.py` for `build/m04/private/gtavcs.nro`, and
   `python tools/build_core_tests.py` for the public runtime test NRO.

The Switch SD card already holds `sdmc:/switch/gtavcs_nx/gamefolder` from M03.

## Remaining, in order

1. Hardware session per [HARDWARE.md](HARDWARE.md) (`NX_HOST=<switch ip>`);
   the focus and close runs need a person at the console.
2. `m04_compare.py` against the reference; add the Switch log to the
   unhandled-instruction classification.
3. Write `TASKS.md`, `VERIFICATION.md`, `HARDWARE_RESULTS.md`, `evidence.json`;
   tick M04-01..13 only for observed results.
4. `python tools/milestone_gate.py M04`, fresh independent reviewer,
   `review.json`, `--require-review`, CI, merge PR (merge commit), then branch
   `milestone/m05-opengl-rhi`.
