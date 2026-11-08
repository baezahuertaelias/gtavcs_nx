# M01 hardware procedure and evidence

This is a platform smoke test. No recompiled game code is linked, and the audio
is intentionally silent. A clear screen is not evidence of gameplay.

Build with `python tools/build_switch.py --clean`; output is
`dist/switch/gtavcs_nx/gtavcs.nro`. Copy only that NRO to the same SD path.
Provide `sdmc:/switch/gtavcs_nx/gamefolder` explicitly for the successful smoke
test. The scaffold never creates it or extracts/copies assets. No game files are
needed by this stub. Do not move or rename existing user assets or saves to test
missing paths; use a separate test SD/layout for that case.

Launch Sphaira with full application memory (title override), then enable its
nxlink/netloader. Record Sphaira version and the actual launch method. On the
same LAN, with the Switch's observed address substituted:

```powershell
docker run --rm -p 28771:28771 --mount type=bind,source=C:/Users/Gamer/Desktop/gtavcs_nx/dist,target=/dist,readonly gtavcs-nx-toolchain:m01 /opt/devkitpro/tools/bin/nxlink -a SWITCH_IP -r 2 -s -p /switch/gtavcs_nx/gtavcs.nro /dist/switch/gtavcs_nx/gtavcs.nro
```

The nxlink server callback uses TCP 28771 forwarded by Docker. This transport
must be observed working on the host/LAN; upload success alone does not prove
stdout reached the listener. If blocked, preserve the SD logs and record the
actual firewall/network limitation rather than claiming a runtime pass.

Logs: `switch/gtavcs_nx/gtavcs-boot.log` records early startup and missing asset
root; after checked chdir, `gamefolder/gtavcs.log` records SDL/GL and runtime.
Both logs are overwritten per launch: copy them before each new test. A fatal
message appears on a console only before SDL owns the display or after SDL
cleanup. Smoke Plus exits; later milestones restore normal Start semantics.

For every run retain candidate SHA, NRO SHA-256 from `checksums.json`, date,
console model, firmware/CFW, launch method, libnx/SDL/Mesa package lock, GL
version/renderer, controller model and mode. Save raw logs/video privately in
`build/evidence/M01/hardware/`; commit sanitized observations and hashes only.
Do not check a task until observations satisfy it.

| Task | Required observation | Status |
|---|---|---|
| M01-06 | Separate missing-root failure and successful checked chdir; visible and persistent diagnostics | Pending hardware |
| M01-07 | Actual Switch, application mode, GL 3.3 core, visible dark blue clear | Pending hardware |
| M01-08 | Paired Joy-Cons and Pro: initial open, buttons/sticks, disconnect neutral, hotplug/reconnect, Plus orderly exit | Pending hardware |
| M01-09 | audren SDL device, exact S16 stereo 48 kHz, bounded queue <=12288 bytes, clean device/stream close | Pending hardware |
| M01-10 | Repeated dock/undock, 1280x720 and 1920x1080 drawable/viewport, clear presentation, one context | Pending hardware |
| M01-11 | HOME/focus, sleep/resume, no stale queued audio/input, no busy loop or invalid resources; startup failure cleanup | Pending hardware |
| M01-12 | Required hardware logs retained and every observed failure closed | Pending hardware |

Audio `empty_observations` is a count of empty queues observed by the main
thread, including initial fill and lifecycle resets. It is not an audren
underrun counter. Silent samples only validate device streaming in M01.

The user supplied `192.168.1.107` for the first nxlink attempt on 2026-11-08;
this is an execution endpoint, not proof of device identity or test results.
