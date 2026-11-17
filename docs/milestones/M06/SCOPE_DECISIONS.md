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
