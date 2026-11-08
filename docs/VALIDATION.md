# Scaffold validation — 2026-11-08

- Upstream checked out at `ecf150e53f87e4656dfe81eacd922839d952b282`;
  `git -C upstream status --short` was empty after scaffold creation.
- Upstream's read-only disc checker accepted the local ISO as SLUS-21590 USA
  v1.00, executable SHA-1 `d1832508caf0990c2291c0f255c96a889cce9f88`.
  No game files were extracted or altered.
- GLAD 2.0.8 successfully generated GL 3.3 core C sources with zero optional
  extensions and reproducible bundled specifications.
- `cmake -S . -B build/check-host-guard` correctly failed before selecting a host
  compiler, with the required official Switch toolchain instruction. This is an
  expected guard check, not a cross-configure/build pass.
- Compared `nx_generate_nacp` / `nx_create_nro` arguments against the owning
  devkitPro `NintendoSwitch.cmake` source. Inspected SDL2 Switch video/audio source
  for EGL/window resizing and audren ownership.

Not performed: Switch cross compilation/link, NRO boot, GL context creation,
controller/audio playback, docking, core integration, gameplay or performance.
Docker's Linux engine pipe was unavailable; WSL listed only `docker-desktop`,
and standard `C:/devkitPro` toolchain paths were absent. A devkitPro-equipped
Ubuntu/container environment and subsequent hardware validation remain required.

The provided files are a complete platform template. They are not a tested
production game build and do not satisfy the beta acceptance criteria yet.
