# M04 handoff

Superseded on 2026-11-14: M04's work resumed on a Linux host, the hardware session ran on the candidate and every
task is recorded in [TASKS.md](TASKS.md), [VERIFICATION.md](VERIFICATION.md) and
[HARDWARE_RESULTS.md](HARDWARE_RESULTS.md). What a new host needs is still true:

1. The Docker image (`docker build -f Dockerfile.switch`, done by `build_game.py`).
2. The private generated EE/VU code and archive in the Docker volume `gtavcs-nx-m02-core` (`/cache/generated`,
   `/cache/generated-build/libvcs_generated_cpu.a`): generate with upstream's `tools/recompile.py` from the user's
   disc in an overlaid checkout (`tools/upstream_overlay.py`; patches 0002/0003 change the generator), copy
   `generated/` to `/cache/generated`, and build `cmake/core` in the container into `/cache/generated-build`
   ([VERIFICATION.md](VERIFICATION.md) has the exact identities).
3. The desktop reference: upstream's `tools/build.py --recompile <disc>` in a clone of the pin at
   `build/m02/reference`, then `tools/m04_reference.py` (`SDL_VIDEODRIVER=offscreen` on a host without a display).
4. `python tools/build_game.py` and `python tools/build_core_tests.py`.
