# GTA Vice City Stories NX

Nintendo Switch homebrew port project based on
[gothamor/gta-vcs-recomp](https://github.com/gothamor/gta-vcs-recomp).

Graphics baseline: SDL2, OpenGL 3.3 core, GLAD, EGL/Mesa nouveau and libnx,
built with devkitPro/devkitA64. Assets resolve beneath
`sdmc:/switch/gtavcs_nx/gamefolder/`.

**Status: unbuilt platform scaffold. No Switch gameplay or performance claim.**
The current core stub clears a screen and streams silence; it is not the game.
No game data or generated game code belongs in this repository.

Read the [milestone/task plan](docs/MILESTONES.md),
[discovery dossier](docs/PORT_DISCOVERY.md),
[automated execution runbook](docs/AUTOMATED_EXECUTION.md) and
[build/developer instructions](AGENTS.md).

Each milestone has one branch. Complete and evidence every task before creating
an independent reviewer; merge into main only after approval and passing CI,
then create the next branch. Private-repository branch protection availability
is recorded in [M00 verification](docs/milestones/M00/verification.md).

```bash
git clone --recurse-submodules https://github.com/baezahuertaelias/gtavcs_nx.git
```

Project glue is licensed under GPL-3.0-only; see [LICENSE](LICENSE). Upstream
and bundled GLAD retain their original notices and licenses. The upstream
project is credited above; this is an independent Switch port effort.
