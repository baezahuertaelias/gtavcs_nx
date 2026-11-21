# M09 scope decisions

## Docked parts to M16-03 (user decision, 2026-11-19)

M09-06 asks for the memory and GPU cost of 16:9 against 4:3 "in both modes", and M09-08 for hardware acceptance "in
handheld and docked". The dock is broken and the user has no Pro Controller (docs/milestones/M06/SCOPE_DECISIONS.md,
2026-11-17). The user's decision of 2026-11-19 (docs/milestones/M08/SCOPE_DECISIONS.md, "A milestone for the Pro
Controller and docked mode") already places them: "Docked and Pro Controller parts of M09-M14 go to it" (M16;
docs/MILESTONES.md, M16-03 "Repeat in docked mode ... every docked or Pro Controller part that M09-M14 deferred
here, each listed with its source task"). No new question was needed.

What changes:

- **M09-06** is measured in handheld (1280x720 output, internal scale 1x). The docked measurement (1920x1080 output)
  is listed under **M16-03**.
- **M09-08** is accepted in handheld, on paired Joy-Cons. The docked acceptance (the 16:9 picture on a TV, a dock change
  at 16:9 and at 4:3) is listed under **M16-03**.

What does not change: nothing is claimed for docked play. The 16:9 picture is laid out for the output size, which the
renderer sees as its back buffer (src/switch/output.c, M06-20); the docked output's geometry is the same 16:9 shape,
but no docked run backs it. Every other M09 task stays as written.
