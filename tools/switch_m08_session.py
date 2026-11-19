"""M08 hardware session: switch_m06_session.py's game run, plus the files the developer menu writes below the profile.

    python tools/switch_m08_session.py preflight <evidence-dir>
    python tools/switch_m08_session.py game <evidence-dir> <run.cfg> [--label NAME] [--prompt TEXT] [--keep-card]
                                                                     [--settings vcs.ini]

`game` is switch_m06_session.py's (empty test card unless --keep-card; --settings puts a vcs.ini into the profile
first). Afterwards the frame-time recordings (userdata/tests/<profile>/perf/*.csv) are read back into
<evidence-dir>/<label>/perf/ as well, and the run's log is summarised into <label>-m08.txt: the chord lines, the menu
lines, the touch lines, the settings lines and GL error counts. NX_HOST names the console (default 192.168.1.107).
"""
import argparse
import ftplib
import os
from pathlib import Path
import re
import sys

os.environ.setdefault("NX_HOST", "192.168.1.107")
sys.path.insert(0, str(Path(__file__).resolve().parent))
import switch_m04_session as m04  # noqa: E402
import switch_m06_session as m06  # noqa: E402

PATTERNS = re.compile(r"\[(chord|devmenu|touch|settings|perf\] recording|trainer)|gl_errors|GL error|"
                      r"focus (lost|back)|quit requested|controller physical")


def command_count(text):
    """The control-channel commands a run file schedules (run_config.cpp refuses more than 512 at boot)."""
    count = 0
    for line in text.splitlines():
        words = line.split("#", 1)[0].split()
        if words[:1] == ["at"]:
            count += 1
        elif words[:1] == ["every"] and len(words) >= 5:
            period, start, end = (float(w) for w in words[1:4])
            count += int((end - start) // period) + 1
    return count


def summarise(session, label):
    log = session.evidence / f"{label}-gtavcs.log"
    if not log.exists():
        session.say(f"{label}: no log to summarise")
        return
    lines = [l for l in log.read_text(errors="replace").splitlines() if PATTERNS.search(l)]
    out = session.evidence / f"{label}-m08.txt"
    out.write_text("\n".join(lines) + "\n")
    session.say(f"{label}: {len(lines)} chord / menu / touch / settings lines -> {out.name}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["preflight", "game"])
    ap.add_argument("evidence")
    ap.add_argument("config", nargs="?")
    ap.add_argument("--label")
    ap.add_argument("--prompt")
    ap.add_argument("--keep-card", action="store_true")
    ap.add_argument("--settings")
    a = ap.parse_args()
    session = m04.Session(Path(a.evidence))
    if a.command == "preflight":
        ok = m04.preflight(session)
    else:
        label = a.label or Path(a.config).stem
        scheduled = command_count(Path(a.config).read_text())
        if scheduled > 512:
            session.say(f"{a.config} schedules {scheduled} commands: more than nxrun.cfg's 512, the game would refuse it")
            return 1
        ok = m06.game(session, a.config, label, a.prompt, a.keep_card, a.settings)
        folder = f"{m04.GAME}/userdata/tests/{m04.profile_of(Path(a.config).read_text())}/perf"
        ftp = m04.connect()
        try:
            tree = m04.walk(ftp, folder, depth=1)
        except ftplib.all_errors:
            tree = {}
        ftp.quit()
        for path, kind in sorted(tree.items()):
            if kind != "d":
                (session.evidence / label / "perf").mkdir(parents=True, exist_ok=True)
                m04.fetch(session, path, session.evidence / label / "perf" / path.rsplit("/", 1)[1])
        summarise(session, label)
    session.say(f"RESULT {a.command} {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
