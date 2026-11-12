# User-approved M03 scope decisions — 2026-11-12

Two M03 substeps could not be completed inside M03. The user was shown the reason
for each and answered **"yes to both, go ahead"** to moving them as described below.
Neither is claimed passed here.

## Live BEACH and MALL level loads (from M03-08)

From an empty memory card the game never loaded `BEACH` or `MALL`. A desktop run on
2026-11-12 teleported the player to the east island: the zone's level global
(`0x4CD118`) became 1 (BEACH), but no `BEACH.LVZ` read followed, and the tee recorded
only `MAINLA` level files with 0 mismatches. Early story progress apparently keeps the
loaded level; upstream's own level test started from a later save, and no such save
exists here. No known position selects `MALL`.

Those files are still covered by the byte comparisons in VERIFICATION.md (all 341
files, boundaries, unaligned and random reads, whole image) and by the whole-disc
digest on the Switch in HARDWARE_RESULTS.md.

The live-load check moves to **M14-02**, which already traverses BEACH, MAINLA and
MALL; its task text now names the folder-disc comparison. M03-08 records this move.

## Switch boot from the folder (from M03-09)

On the desktop the real game reaches gameplay with no image opened and the ELF taken
from the extracted tree. Making the Switch boot path use `FolderDisc` and `GamePaths`
needs M04's core adapter, which does not exist yet.

That requirement moves to the new task **M04-13**. M03-09 records this move and its
desktop proof.
