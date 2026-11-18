# M07 scope decisions

## Docked output mode deferred to M10-13 (user decision, 2026-11-18)

M07-11 asks for no audio drift or cutscene desynchronization "across variable frame rates and both output modes". The
dock is broken (M06's decision of 2026-11-17, docs/milestones/M06/SCOPE_DECISIONS.md, already moved all docked work to
M10-13). Asked whether M07-11's docked part should follow, the user answered "yes, move docked to M10-13".

What changes:

- **M07-11** proves no drift or desynchronization across variable frame rates in handheld mode only.
- **M10-13** gains M07-11's docked part: audio drift and cutscene synchronization in docked mode.

What does not change: nothing is claimed for docked audio or movies. Every other M07 task stays as written; M07 runs
are handheld.
