"""M07-09 read / decode failure run: a same-size file of zeros as a mod over MOVIES/STINGS.PSS, the m07-readfail run,
then the mod and the folders this created removed again.

    python tools/switch_m07_readfail.py <evidence-dir> [--label readfail-handheld]

The original MOVIES/STINGS.PSS on the SD card is never written. Refuses to start if gamefolder/mods already exists, so
nothing of the person's is touched or removed.
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import switch_m04_session as m04  # noqa: E402

GAME = f"{m04.SD}/gamefolder"
MODS = f"{GAME}/mods"
MOD_DIR = f"{MODS}/MOVIES"
MOD = f"{MOD_DIR}/STINGS.PSS"
STINGS_SIZE = 3014660  # gamefolder/MOVIES/STINGS.PSS (SLUS-21590 USA v1.00)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("evidence")
    ap.add_argument("--label", default="readfail-handheld")
    args = ap.parse_args()
    session = m04.Session(Path(args.evidence))
    ftp = m04.connect()
    if "mods" in m04.names(m04.listing(ftp, GAME)):
        session.say(f"REFUSED: {MODS} already exists; this run only creates and removes its own")
        return 1
    ftp.mkd(MODS)
    ftp.mkd(MOD_DIR)
    session.say(f"created {MOD_DIR}")
    m04.put(session, bytes(STINGS_SIZE), MOD)
    try:
        result = subprocess.run([sys.executable, str(ROOT / "tools/switch_m06_session.py"), "game", args.evidence,
                                 str(ROOT / "tests/switch/m07-readfail.cfg"), "--label", args.label,
                                 "--prompt", "do not touch (4 min)"])
    finally:
        ftp = m04.connect()
        ftp.delete(MOD)
        ftp.rmd(MOD_DIR)
        ftp.rmd(MODS)
        left = "mods" in m04.names(m04.listing(ftp, GAME))
        session.say(f"removed {MOD}, {MOD_DIR}, {MODS}; mods left={left}")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
