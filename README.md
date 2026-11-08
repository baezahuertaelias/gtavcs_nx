# GTA Vice City Stories NX

Nintendo Switch homebrew port project based on
[gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp).

Graphics baseline: SDL2, OpenGL 3.3 core, GLAD, EGL/Mesa nouveau and libnx,
built with devkitPro/devkitA64. Assets resolve beneath
`sdmc:/switch/gtavcs_nx/gamefolder/`.

**Status: unbuilt platform scaffold. No Switch gameplay or performance claim.**
The current core stub clears a screen and streams silence; it is not the game.
No game data or generated game code belongs in this repository.

The repository starts with this bootstrap commit on `main`. All subsequent
milestone work uses its own branch and merges only after all tasks and an
independent review pass. The detailed plan and build instructions are added in
the M00 foundation branch.
