# M08 scope decisions

## Pro Controller to M13-11, docked output to M10-13 (user decision, 2026-11-19)

M08-16 asks for hardware acceptance "on paired Joy-Cons, handheld and Pro Controller", and M08-02 for a "readable 720p
handheld and 1080p docked layout". The user has no Pro Controller and the dock is broken (the same facts as M06's
decision of 2026-11-17, docs/milestones/M06/SCOPE_DECISIONS.md, and M07's of 2026-11-18). Asked "Should I defer those
parts the same way as in M06/M07 (Pro Controller -> M13-11, docked -> M10-13), checking the 1080p layout in handheld by
forcing 1920x1080 output instead?", the user chose "Defer both".

What changes:

- **M08-16** is accepted on paired Joy-Cons in handheld mode. Its Pro Controller part moves to **M13-11**.
- **M08-02**'s docked part moves to **M10-13**. The 1080p layout itself is still checked in M08: a test run in handheld
  sets `VCS_NX_OUTPUT=1080` (src/switch/output.c), which draws the docked 1920x1080 output and lets the panel scale it,
  so the menus are laid out at 1920x1080 exactly as docked. What that does not show (the picture on a TV, the dock
  change while the menu is open) is M10-13's.
- **M08-17**'s touch screen is handheld-only hardware anyway (no touch when docked).

What does not change: nothing is claimed for the Pro Controller or for docked play. Every other M08 task stays as
written.
