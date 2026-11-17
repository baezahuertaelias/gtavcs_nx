"""Package the M06 beta privately (M06-19): the SD card layout with the game NRO, checksums and the notes.

    python tools/package_beta.py [--out build/beta]

Takes the private game NRO (build/m04/private/gtavcs.nro, built by `python tools/build_game.py`) and writes

    <out>/gtavcs_nx-beta-<commit>/
        switch/gtavcs_nx/gtavcs.nro            the game
        switch/gtavcs_nx/gamefolder/PUT_DISC_FILES_HERE.txt
        BETA_NOTES.md                          docs/milestones/M06/BETA_NOTES.md: preparation, limits, measurements
        SHA256SUMS                             every file above
    <out>/gtavcs_nx-beta-<commit>.zip          the same folder

The NRO holds code compiled from the user's own disc: the package is private, for the user's console only. It never
contains the ISO, extracted game files, generated C++, saves or screenshots; the tool refuses any other file and
refuses a dirty working tree, so the commit in the name is the source of the NRO's port code.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NRO = ROOT / "build/m04/private/gtavcs.nro"
NOTES = ROOT / "docs/milestones/M06/BETA_NOTES.md"
ALLOWED = {"switch/gtavcs_nx/gtavcs.nro", "switch/gtavcs_nx/gamefolder/PUT_DISC_FILES_HERE.txt", "BETA_NOTES.md",
           "SHA256SUMS"}
PLACEHOLDER = """Put the files of your own extracted disc here (python tools/extract_disc.py extract <your.iso> <folder>,
then copy that folder's contents, including the hidden .nxdisc folder, into this one). See BETA_NOTES.md.
"""


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=ROOT / "build/beta")
    a = ap.parse_args()
    if git("status", "--porcelain"):
        sys.exit("FAIL working tree not clean: commit first, so the package names the NRO's source commit")
    if not NRO.is_file() or not NOTES.is_file():
        sys.exit(f"FAIL need {NRO.relative_to(ROOT)} (python tools/build_game.py) and {NOTES.relative_to(ROOT)}")
    commit = git("rev-parse", "--short=12", "HEAD")
    name = f"gtavcs_nx-beta-{commit}"
    folder = a.out / name
    if folder.exists():
        shutil.rmtree(folder)
    game = folder / "switch/gtavcs_nx"
    (game / "gamefolder").mkdir(parents=True)
    shutil.copyfile(NRO, game / "gtavcs.nro")
    (game / "gamefolder/PUT_DISC_FILES_HERE.txt").write_text(PLACEHOLDER, encoding="utf-8", newline="\n")
    shutil.copyfile(NOTES, folder / "BETA_NOTES.md")
    files = sorted(p for p in folder.rglob("*") if p.is_file())
    sums = "".join(f"{sha256(p)}  {p.relative_to(folder).as_posix()}\n" for p in files)
    (folder / "SHA256SUMS").write_text(sums, encoding="utf-8", newline="\n")
    found = {p.relative_to(folder).as_posix() for p in folder.rglob("*") if p.is_file()}
    if found != ALLOWED:
        sys.exit(f"FAIL unexpected package contents: {sorted(found ^ ALLOWED)}")
    archive = a.out / f"{name}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in sorted(found):
            z.write(folder / rel, f"{name}/{rel}")
    print(sums, end="")
    print(f"PASS {archive.relative_to(ROOT)} sha256={sha256(archive)} (private: the NRO holds code built from your disc)")


if __name__ == "__main__":
    main()
