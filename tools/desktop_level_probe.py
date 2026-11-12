"""Probe whether the game loads another level from an empty card, on the desktop reference with the folder tee.

    python tools/desktop_folder_disc.py apply      (then build the reference with its tools/build.py --quick)
    python tools/desktop_level_probe.py <gamefolder> <evidence-dir>
    python tools/desktop_folder_disc.py revert

After a new game it teleports between the islands and records, per stop, the player, the
interior area and the level the player's zone asks for (global 0x4CD118: 1 BEACH, 2 MAINLA,
3 MALL; upstream game-internals.md, "Levels"). tee-reads.txt shows which level files were read.
"""
import argparse
import time

import desktop_folder_disc

STOPS = (("east", 575.0, 175.0), ("east again", 575.0, 175.0), ("east south", 1325.0, -325.0),
         ("mainland north", -675.0, 1425.0), ("mainland", -1425.0, -575.0))


def scenario(game, rig, evidence, log):
    log("boot: flyover and intro cutscene, no input")
    time.sleep(75)
    game.new_game(timeout=240.0)
    game.invincible()
    time.sleep(6)

    def probe(tag):
        log(f"{tag}: player={game.player()} area={game.area()} zone_level={game.u32(0x4CD118)}")

    probe("ingame")
    for name, x, y in STOPS:
        game.teleport(x, y, -100.0, heading=0.0)
        for step in range(3):
            time.sleep(5)
            probe(f"{name} +{5 * (step + 1)}s")
    log("done")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("gamefolder")
    parser.add_argument("evidence")
    args = parser.parse_args()
    desktop_folder_disc.scenario = scenario
    desktop_folder_disc.run("tee", args.gamefolder, args.evidence)


if __name__ == "__main__":
    main()
