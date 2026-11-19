# M08 review 4: findings and answers

M08-08, 09 and 15 were reopened (`5388408`) and proven again. New candidate NRO `c81c7428` from `610817c` (clean tree);
`git diff 352b29b 610817c -- src CMakeLists.txt patches` changes only patch 0012's trainer hunks (`openTrainerMenu`,
`readInput`, `g_padIgnored`) and the series hash.

| # | Finding | Answer |
|---|---|---|
| 1 (low) | A held D-pad direction acted in the trainer at the hand-over | Patch 0012: `openTrainerMenu` marks every pad button held at that moment as ignored (`g_padIgnored`), and `readInput` treats an ignored button as up, for both the press edge and the held state the repeat path reads, until it is let go. Hardware, `trainer-r4c` (c81c7428): the person held the Right arrow and pressed A on "Trainer...": `[trainer] opened from another menu, 1 page(s) deep; 2 button(s) held now ignored until let go`, Right held 3.6 s more (`dpright down` 157151 ms, `up` 161617 ms), `trainer_used=0` in every report. Limitation: the trainer was on its main page, where Right changes nothing even without the fix; the Player-page case (Right on God mode) was not reproduced on hardware, and the user declined a further run ("I won't do more test, this is enough"). The fix's effect there follows from the same ignored state (the row's `change` needs `in.left` / `in.right`, which an ignored button cannot raise). |
| 2 (low) | "nothing chosen" asserted before any input | The line now states the mechanism (`N button(s) held now ignored until let go`). M08-08 cites `trainer_used=0` with the trainer reopened two pages deep on Player in `trainer-r3` (God mode, a counted toggle, selected) for the A case, and `trainer-r4c` for held buttons. |

Also on c81c7428: host tests from 610817c, all PASS. Not rerun on c81c7428: the automatic settings / nav runs (their code
is unchanged since 2a02c7b6, where they passed: `nav-r3`, `settings-r3`, `relaunch-r3`, `reset-r3`).
