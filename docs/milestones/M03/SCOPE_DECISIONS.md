# User-approved M03 scope decisions — 2026-11-12

Three parts of M03 could not be completed inside M03. Each time, the user was shown the
reason and approved the move described below. None of them is claimed passed here.

## Live BEACH and MALL level loads (from M03-08)

Approved with **"yes to both, go ahead"**.

From an empty memory card the game never loaded `BEACH` or `MALL` in any desktop run.
In the scripted scenario the player stood on the east island at (579.4, 175.0) for 75 s
and no `BEACH` file was read. The clean level probe of 2026-11-12 (VERIFICATION.md, "Level
probe") found the opening mission overriding teleports to the east island and the zone
level at 2 (MAINLA) at every sample; only `MAINLA` level files were read, with 0 mismatches.
Upstream's own level test started from a later save, and no such save exists here. No
known position selects `MALL`.

Those files are still covered by the byte comparisons in VERIFICATION.md (all 341 files,
boundaries, unaligned and random reads, whole image) and by the whole-disc digest on the
Switch in HARDWARE_RESULTS.md.

The live-load check moves to **M14-02**, which already traverses BEACH, MAINLA and MALL;
its task text now names the folder-disc comparison. M03-08 records this move.

## Switch boot from the folder (from M03-09)

Approved with **"yes to both, go ahead"**.

On the desktop the real game reaches gameplay with no image opened and the ELF taken from
the extracted tree. Making the Switch boot path use `FolderDisc` and `GamePaths` needs
M04's core adapter, which does not exist yet. That requirement moves to the new task
**M04-13**. M03-09 records this move and its desktop proof.

## Runtime use of the userdata paths (from M03-10)

Approved by selecting **"Yes, move it to M04-13"** after the independent review found that
no runtime routes settings or cards through `GamePaths`.

M03-10 now closes on the path layer: `GamePaths` places settings, cache, both memory
cards and the card backups folder under `gamefolder/userdata`, and a test profile places
all of them under `userdata/tests/<name>`, proven by the synthetic suite. Card metadata
and order are kept by upstream's card layer inside each card folder (`.ps2x-order`,
upstream game-internals.md 10c), so they stay under `userdata` with the card.

The runtime actually using these paths, with Switch proof that settings, cache, cards
(including their order file) and backups land there and that test runs use a test
profile, moves to **M04-13**.
