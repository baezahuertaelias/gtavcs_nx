"""Run the real game on the desktop reference with the folder disc, to compare live reads.

Patches only the disposable clone in build/m02/reference, never the upstream submodule.

    python tools/desktop_folder_disc.py apply
    python tools/desktop_folder_disc.py tee <gamefolder> <evidence-dir>      compare every read with the ISO
    python tools/desktop_folder_disc.py folder <gamefolder> <evidence-dir>   boot with no ISO opened
    python tools/desktop_folder_disc.py iso <gamefolder> <evidence-dir>      control run on the plain ISO
    python tools/desktop_folder_disc.py revert

Runs use an empty, separate memory card. PS2X_GS stays at the reference's hardware renderer.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "build/m02/reference"
MAIN = REFERENCE / "src/main.cpp"
VFS = REFERENCE / "src/vfs"

BLOCK_OLD = """        const fs::path isoPath = locateIso(argc, argv);
        if (isoPath.empty())
"""
BLOCK_NEW = """        const char *folderOnly = std::getenv("VCS_FOLDER_ONLY");
        const fs::path isoPath = folderOnly ? fs::path("(none)") : locateIso(argc, argv);
        if (isoPath.empty())
"""
OPEN_OLD = "        if (!iso.open(isoPath))\n"
OPEN_NEW = "        if (!folderOnly && !iso.open(isoPath))\n"
ELF_OLD = """        const fs::path elfPath = data / "cache" / kElfName;
        std::string error;
        if (const int code = stageElf(iso, elfPath, error))
"""
ELF_NEW = """        const fs::path elfPath = folderOnly ? fs::path(folderOnly) / kElfName : data / "cache" / kElfName;
        std::string error;
        if (const int code = folderOnly ? 0 : stageElf(iso, elfPath, error))
"""
DISC_OLD = """        MappedDisc disc(iso);
        ps2_disc::setImage(&disc);
"""
DISC_NEW = """        MappedDisc disc(iso);
        std::unique_ptr<FolderDisc> folder;
        const char *folderRoot = folderOnly ? folderOnly : std::getenv("VCS_FOLDER_TEE");
        if (folderRoot)
        {
            std::string folderError;
            folder = FolderDisc::load(folderRoot, folderError);
            if (!folder)
            {
                std::cerr << "[folder-disc] " << folderError << std::endl;
                return vcs::disc::kExitDiscUnreadable;
            }
        }
        std::unique_ptr<TeeDisc> tee;
        if (!folderOnly && folder)
            tee = std::make_unique<TeeDisc>(disc, *folder, iso, std::getenv("VCS_FOLDER_TEE_LOG"));
        ps2_disc::setImage(folderOnly ? static_cast<IPS2DiscImage *>(folder.get())
                                      : tee ? static_cast<IPS2DiscImage *>(tee.get()) : &disc);
"""


def apply_patch():
    text = MAIN.read_bytes().decode("utf-8").replace("\r\n", "\n")
    for old, new in ((BLOCK_OLD, BLOCK_NEW), (OPEN_OLD, OPEN_NEW), (ELF_OLD, ELF_NEW), (DISC_OLD, DISC_NEW)):
        if text.count(old) != 1:
            raise SystemExit("Reference main.cpp changed: " + old.splitlines()[0])
        text = text.replace(old, new)
    text = '#include "vfs/tee_disc.h"\n#include <memory>\n' + text
    MAIN.write_bytes(text.encode("utf-8"))
    VFS.mkdir(exist_ok=True)
    for source in (ROOT / "src/vfs/folder_disc.h", ROOT / "src/vfs/folder_disc.cpp", ROOT / "tests/desktop/tee_disc.h"):
        shutil.copyfile(source, VFS / source.name)
    print("PASS patched reference; build it with its tools/build.py --quick")


def revert():
    subprocess.check_call(["git", "-C", str(REFERENCE), "checkout", "--", "src/main.cpp"])
    shutil.rmtree(VFS, ignore_errors=True)
    print("PASS reverted reference")


def scenario(game, rig, evidence, log):
    """Boot with the flyover and intro cutscene, then streaming, level changes and the radio."""
    log("boot: flyover and intro cutscene, no input")
    time.sleep(75)
    game.new_game(timeout=240.0)
    log("ingame")
    game.set_time(12)
    game.invincible()
    time.sleep(6)
    log(f"prime={game.prime()} player={game.player()}")
    # 1, 3: east island; 2, 4: mainland. Each crossing changes the loaded level.
    spots = (("east", 575.0, 175.0), ("mainland north", -675.0, 1425.0), ("east south", 1325.0, -325.0), ("mainland", -1425.0, -575.0))
    for name, x, y in spots:
        game.teleport(x, y, -100.0, heading=0.0)
        time.sleep(14)
        log(f"teleport {name}: player={game.player()} frames={game.cmd('frames')} area={game.area()}")
    game.teleport(spots[0][1], spots[0][2], -100.0, heading=0.0)
    time.sleep(3)
    log("ai drive with radio")
    game.autodrive(speed=40.0)
    for _ in range(6):
        time.sleep(15)
        log(f"driving: player={game.player()} frames={game.cmd('frames')}")
    log("done")


def run(mode, gamefolder, evidence):
    evidence = Path(evidence).resolve()
    root = str(Path(gamefolder).resolve())
    evidence.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(REFERENCE / "tools"))
    os.chdir(REFERENCE)
    import rig  # noqa: PLC0415

    log_file = open(evidence / f"{mode}-steps.txt", "w", encoding="utf-8")

    def log(text):
        line = time.strftime("%H:%M:%S ") + text
        print(line, flush=True)
        log_file.write(line + "\n")
        log_file.flush()

    images = sorted(ROOT.glob("*Vice City Stories*.iso"))
    if not images:
        raise SystemExit("No disc image in the repository root")
    env = {"VCS_SKIP_MOVIES": "0", "VCS_ISO": str(images[0])}
    env["VCS_STREAM_LOG"] = "1"
    if mode == "iso":
        pass
    elif mode == "tee":
        env.update({"VCS_FOLDER_TEE": root, "VCS_FOLDER_TEE_LOG": str(evidence / "tee-reads.txt")})
    else:
        env["VCS_FOLDER_ONLY"] = root
    game = rig.Game.start(name=f"m03-{mode}", port=47671, seconds=900, env=env,
                          data_dir=str(evidence / f"{mode}-userdata"))
    try:
        scenario(game, rig, evidence, log)
    finally:
        game.quit()
        time.sleep(3)
        run_log = REFERENCE / "build/run" / f"m03-{mode}.log"
        if run_log.exists():
            shutil.copyfile(run_log, evidence / f"{mode}-run.log")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["apply", "revert", "tee", "folder", "iso"])
    parser.add_argument("gamefolder", nargs="?")
    parser.add_argument("evidence", nargs="?")
    args = parser.parse_args()
    if args.command == "apply":
        apply_patch()
    elif args.command == "revert":
        revert()
    else:
        run(args.command, args.gamefolder, args.evidence)


if __name__ == "__main__":
    main()
