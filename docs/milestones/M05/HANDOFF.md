# M05 handoff: where the milestone stands and what to do next

Read this first when resuming M05. Branch `milestone/m05-opengl-rhi` (pushed). The ledger is docs/MILESTONES.md; the
workflow is docs/AUTOMATED_EXECUTION.md; the evidence so far is in this folder.

## State (2026-11-17)

- The OpenGL backend, the GLSL ports and the conformance tests are proven on the Switch: 127/127 readbacks within
  tolerance of upstream's Vulkan backend (101 byte-identical), handheld and docked ([HARDWARE_RESULTS.md](HARDWARE_RESULTS.md)).
- Reviews 1-3 requested changes; all findings are resolved: [REVIEW1_RESPONSE.md](REVIEW1_RESPONSE.md),
  [REVIEW2_RESPONSE.md](REVIEW2_RESPONSE.md), [REVIEW3_RESPONSE.md](REVIEW3_RESPONSE.md).
- Review 3's leak: switch-sdl2 resizes a resizable window at every dock change and every resize leaks the EGL
  surface's buffers. Fixed in `dcda9e1` (window fixed at the starting mode's size, not resizable); round 4
  ([hardware-r4/](hardware-r4/)) shows no growth per dock change. M05-10 and M05-14 are ticked again.

## Next steps

1. `python tools/milestone_gate.py M05`, then a **fresh** independent reviewer on the new candidate (the user
   authorizes it; give it the review 1-3 response files).
2. On approval: record `review.json`, `python tools/milestone_gate.py M05 --require-review`, open the PR (like #5),
   wait for green CI, merge with a merge commit, then cut `milestone/m06-ingame` from the merged main.

## Console notes

Switch at 192.168.1.107 in round 4 (192.168.1.91 before; set `NX_HOST`), Sphaira by title override, FTP port 5000 +
nxlink. It drops off the network when idle; check port 5000 first. A run meant to start docked must show
`operation_mode_start 1` in `caps.txt` (round 4's first docked attempt was not seated). This Windows host cannot run
`tools/rhi_host_tests.py` (needs EGL / a Linux Vulkan driver); compare readback hashes with round 3 instead.
