# GTA Vice City Stories NX — private beta (M06)

A native Nintendo Switch homebrew build of the recompiled PS2 game. This beta starts a new game from an empty memory
card, plays the opening, and lets you play at Fort Baxter. It is a private build for your own console. It contains
code compiled from your own disc, so do not share it.

## What you need

- A Switch that runs homebrew, with a homebrew menu you can open with **full application memory**: hold **R** while
  starting a game (title override). The album applet (applet mode) does not have enough memory.
- Your own copy of the PS2 disc *Grand Theft Auto: Vice City Stories*, USA, SLUS-21590 v1.00, as an ISO image.
- A PC with Python 3 and this repository.

## Prepare the disc files

1. Check the disc image: `python3 upstream/tools/vcs_disc.py check /path/to/your.iso`.
2. Extract it into a folder: `python tools/extract_disc.py extract /path/to/your.iso gamefolder`. This keeps the
   disc's own file names and writes a hidden `.nxdisc` folder with the sector layout the game needs.
3. Check the folder: `python tools/extract_disc.py verify gamefolder --iso /path/to/your.iso`.

## Copy to the SD card

```text
sdmc:/switch/gtavcs_nx/
  gtavcs.nro                  from this package
  gamefolder/                 the contents of your extracted folder, including .nxdisc
```

Do not put the ISO on the card; the game reads only `gamefolder`. Check the files against `SHA256SUMS`.

## Play

Start `gtavcs.nro` from the homebrew menu opened with full memory. The game shows the legal screen, the opening
flyover and the intro cutscene, then gameplay at Fort Baxter. Controls are by button position, as on the PS2: the
bottom face button is Cross (press it to skip the cutscene), **+** is Start (pause menu), **−** is Select, the sticks
and stick clicks are the PS2's. To quit, press **HOME** and close the software: the game shuts down in order.

The log of the last run is `sdmc:/switch/gtavcs_nx/gamefolder/gtavcs.log`. Your saves and settings go to
`gamefolder/userdata`.

## What was checked (2026-11-17, handheld, stock clocks)

On a real Switch in handheld mode, from an empty test memory card: the legal screen, the flyover, the intro cutscene
and gameplay at Fort Baxter with the 3D world, the player, the HUD and the radar; more than 5 continuous minutes of
play with paired Joy-Cons (on foot and on the bike); closing from HOME. The cutscene also plays to its end with no
sound output at all. Disconnecting the Joy-Cons stops the player (input goes neutral) and reconnecting them works.

Measured frame times at Fort Baxter, handheld, internal resolution 1x (640x448), output 1280x720:

| Run | Game frame median | Game frame p95 |
|---|---|---|
| standing, nobody touching | 43.1 ms (about 19 fps on average) | 67.9 ms |
| playing, on foot and on the bike | 66.6 ms | 264.1 ms |

The frame rate is low. There is no frame-rate target in this beta; performance is milestone M10.

## What is not in this beta

- **Docked mode is untested** (the dock used for testing was broken). The TV output, docking and undocking are not
  checked. Start the game in handheld mode.
- **The Pro Controller is untested.** Only paired Joy-Cons, attached and wireless, were checked.
- **No movies**: the Rockstar logos and the intro movie are skipped (the build has no video decoder). The intro
  cutscene is not a movie and plays.
- **Sound** is the game's own, but it was not checked for correctness.
- No save or load was checked, no dev menu, no widescreen (the picture is 4:3 with side bars), no custom textures or
  audio, and nothing beyond the first scene of the game was checked.
