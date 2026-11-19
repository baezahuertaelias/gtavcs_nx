# M08 review 3: finding and answer

M08-08, 09 and 15 were reopened (`e39cd35`) and proven again. New candidate NRO `2a02c7b6` from
`352b29b` (clean tree); `git diff b2b4b1c 352b29b -- src CMakeLists.txt patches` changes only patch 0012's
`openTrainerMenu` (and the series hash).

| # | Finding | Answer |
|---|---|---|
| 1 (medium) | The A that picks "Trainer..." also chose the trainer's selected row | Patch 0012: `ui::openTrainerMenu` seeds the trainer's previous pad state from the current one (the choosing button counts as already pressed) and logs `[trainer] opened from another menu, N page(s) deep, nothing chosen`. Hardware, `trainer-r3` (2a02c7b6): the trainer opened from the menu on Main (`1 page(s) deep`), the person entered Player, closed everything with L3 + R3, reopened it from the menu: `2 page(s) deep, nothing chosen`, `trainer_used=0` in every report, the user: "It went great". Also on 2a02c7b6: `nav-r3` (nav flags as before), `settings-r3`, `relaunch-r3`, `reset-r3` (settings as before); host tests from 352b29b, all PASS. |
