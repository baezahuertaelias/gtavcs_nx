# M06 scope decisions

## Pro Controller deferred to M13 (user decision, 2026-11-17)

The user has no Pro Controller. Asked whether to defer the Pro Controller part of M06 or keep M06 open until one is
available, the user chose to defer it ("go with opt 1").

What changes:

- **M06-14** keeps the formal physical connection-state matrix (initial open, hotplug, full disconnect to neutral,
  reconnect, orderly exit), the paired Joy-Con identity and physical connection reporting, and the resolution of
  M01's SDL `connected=1` observation, all proved with paired Joy-Cons (attached and wireless). Only the Pro
  Controller identity and its matrix leave M06.
- **M06-16** proves the minimum beta controller path (Cross, Start, on-foot sticks, L3/R3 reported, Plus as Start,
  deliberate exit) on paired Joy-Cons, attached and wireless. Only the Pro Controller leaves M06.
- **M13-11** (new) owns the deferred work: the Pro Controller's identity, mapping, physical connection-state matrix
  and play, on hardware.

What does not change: nothing is claimed for the Pro Controller. The beta notes (M06-19) state that the Pro
Controller is untested. M13-01 and M13-09 already name the Pro Controller and stay as written.

## Handheld only; docked work deferred to M10-13 (user decision, 2026-11-17)

The user's dock is broken: "can you focus only on handheld? No more dock, ive broken it". M06 is proved in handheld
mode only.

What changes:

- **M06-11** measures S2 and S4 in handheld only.
- **M06-13** (the deferred M01 docking check) leaves M06 entirely.
- **M06-18** is the beta acceptance in handheld only.
- **M06-20** proves, from a handheld start, that the 1280x720 output fills the panel through the crop of the fixed
  1920x1080 surface (src/switch/output.c), the right way up and with nothing cut off. The docked output, dock
  changes and the heap across them leave M06.
- **M10-13** (new) owns all of it: M06-13's docking check, M06-20's docked output, dock changes from both start
  modes with a flat heap, docked S2/S4 timing (M06-11) and the docked acceptance run (M06-18).

What does not change: nothing is claimed for docked mode. The beta notes (M06-19) state that docked play, docking
and the TV output are untested. M10-04, M10-07 and M10-08 already require both modes and stay as written.

## M06-09 without a new desktop run (user decision, 2026-11-17)

A desktop reference run of `tests/switch/m06-route.cfg` was started on this host and stopped by the user: "it wont be
a desktop reference". Offered (1) the existing references or (2) deferring the picture comparison to M14, the user
chose "go with option 1". M06-09 is closed with what exists:

- **Key guest state**, compared exactly: the M04 desktop reference (`build/evidence/M04/reference`, upstream's
  desktop build at the pin, the same Cross schedule) against the Switch's route and silent runs.
- **Pictures**, compared by description, not by pixels: the Switch's S1-S4 screenshots against M05's desktop record
  of the same schedule through upstream's Vulkan backend and the port's OpenGL backend
  (`docs/milestones/M05/desktop/README.md`; those desktop pictures stay on the host that made them).

No pixel comparison of S1-S4 between Switch and desktop is claimed. The RHI's pixel agreement with upstream's Vulkan
backend is M05's conformance claim (127/127 within tolerance on the Switch).
