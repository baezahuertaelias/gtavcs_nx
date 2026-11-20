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

## A milestone for the Pro Controller and docked mode (user decision, 2026-11-19)

Later the same day the user asked: "When all handheld is done, create a milestone to test pro controller and docked".
Asked where it goes, the user chose "After M14, before release". So **M16 — Pro Controller and docked mode** is added
to docs/MILESTONES.md and runs after M14 and before M15. M10-13 (all docked work deferred from M06, M07 and M08) moves
to M16-01 and M13-11 (all Pro Controller work deferred from M06 and M08) to M16-02; the two old IDs stay as "moved"
records. M16-03 collects what M09-M14 defer to it. The deferrals above now land in M16.

## The USB mouse check to M16-04 (user decision, 2026-11-19)

M08-17 asks for touch and mouse positions to agree with Dear ImGui's display, verified on hardware. Review 2 found the
mouse half unverified (no USB mouse was used) and the mapping in `toUiCoordinates` resting on an assumption about
switch-sdl2's mouse coordinates. Asked "Do you have one, or should that part move to M16 (the hardware-we-lack
milestone)?", the user chose "Move it to M16". M08-17 is accepted for the touch screen; **M16-04** verifies the USB
mouse on hardware and fixes the mapping if needed. Nothing is claimed for a mouse in M08.
