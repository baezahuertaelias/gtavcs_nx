"""M09 hardware session: switch_m08_session.py's game run, plus what the widescreen checks read back.

    python tools/switch_m09_session.py preflight <evidence-dir>
    python tools/switch_m09_session.py game <evidence-dir> <run.cfg> [--label NAME] [--prompt TEXT] [--keep-card]
                                                                     [--settings vcs.ini]
    python tools/switch_m09_session.py shapes <burst-dir>

`game` is switch_m08_session.py's, with the screenshots kept privately under build/evidence/M09/screenshots/<label>
(game pictures never enter Git). A run file with `burst userdata/tests/m09-burst/...` commands gets those folders
made and emptied first (the runtime's burst does not create its folder) and read back into the screenshots folder
afterwards, then `shapes` runs on each. After every run <label>-m09.txt lists the peekf / setting / screenshot replies,
the display-aspect lines, the camera modes (gameinfo) and the [rhi-stats] target lines, for the task records.

`shapes` reads a burst folder: burst.txt (the runtime's line per presented frame: its display aspect, the frame's
shape, whether the frame drew 3D / the game's menu) and each burst_NNNN.png's lit rectangle (the picture inside the
capture; 4:3 is pillarboxed in a 16:9 capture). A frame passes when the rectangle's shape matches the shape burst.txt
gives it, and a frame of the game's menu without 3D is 4:3 (upstream's regress step "menushape"). NX_HOST names the
console (default 192.168.1.107).
"""
import argparse
import ftplib
import os
from pathlib import Path
import re
import sys

os.environ.setdefault("NX_HOST", "192.168.1.107")
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / "upstream/tools"))
import switch_m04_session as m04  # noqa: E402
import switch_m06_session as m06  # noqa: E402
import switch_m08_session as m08  # noqa: E402
from toolchain import read_png  # noqa: E402  (upstream's standard-library PNG reader)

m06.SHOTS = ROOT / "build/evidence/M09/screenshots"
BURST = re.compile(r"^\s*(?:at|every)\b.*\bburst\s+(userdata/tests/m09-burst/\S+)", re.M)
SUMMARY = re.compile(r'command="(peekf|setting|screenshot|burst|press start|scm)[^"]*"|\[present\] display aspect|'
                     r"\[settings\] display aspect|\[devmenu\] aspect|targets \d+ \(|gl errors [1-9]|\[display\]")
CAMERA = re.compile(r"^command t=([\d.]+) .*CAMERA  mode (\d+) ([a-z /]+?)\s*\|", re.M)


def prepare_bursts(session, folders):
    ftp = m04.connect()
    try:
        for folder in folders:
            path = m04.GAME + "/"
            for part in folder.split("/"):
                path += part + "/"
                try:
                    ftp.mkd(path.rstrip("/"))
                except ftplib.all_errors:
                    pass
            for name in ftp.nlst(path.rstrip("/")):
                name = name.rsplit("/", 1)[-1]
                if name.startswith("burst"):
                    ftp.delete(path + name)
            session.say(f"burst folder ready (empty): {path}")
    finally:
        ftp.quit()


def fetch_bursts(session, label, folders):
    out = []
    for folder in folders:
        local = m06.SHOTS / label / ("burst-" + folder.rsplit("/", 1)[-1])
        local.mkdir(parents=True, exist_ok=True)
        ftp = m04.connect()
        try:
            names = [n.rsplit("/", 1)[-1] for n in ftp.nlst(f"{m04.GAME}/{folder}")]
        finally:
            ftp.quit()
        for name in sorted(names):
            m04.fetch(session, f"{m04.GAME}/{folder}/{name}", local / name)
        session.say(f"{label}: {len(names)} burst files from {folder} -> {local.relative_to(ROOT)}")
        out.append(local)
    return out


def rect_shape(path):
    """The lit rectangle's width / height in a capture (a 4:3 picture is pillarboxed in a 16:9 capture)."""
    w, h, rows = read_png(path)
    lit = lambda x, y: sum(rows[y][x * 3:x * 3 + 3]) > 24  # noqa: E731
    cols = [x for x in range(w) if any(lit(x, y) for y in range(0, h, max(1, h // 24)))]
    if not cols:
        return None, w / h
    return (cols[-1] - cols[0] + 1) / h, w / h


def shapes(folder):
    """One line per frame and a verdict: (lines, frames, mismatched)."""
    folder = Path(folder)
    records = {}
    burst_txt = folder / "burst.txt"
    if burst_txt.exists():
        for line in burst_txt.read_text(errors="replace").splitlines():
            m = re.match(r"(\d{4}) aspect ([\d.]+) frame ([\d.]+).*?\b3d (\d+) menu (\d+)", line)
            if m:
                records[m.group(1)] = (float(m.group(2)), float(m.group(3)), int(m.group(4)), int(m.group(5)))
    lines, bad, frames = [], 0, 0
    for png in sorted(folder.glob("burst_*.png")):
        key = png.stem.split("_")[1]
        shape, capture = rect_shape(png)
        rec = records.get(key)
        frames += 1
        if shape is None or rec is None:
            lines.append(f"{key} rect {'black' if shape is None else f'{shape:.3f}'} record {rec} (not judged)")
            continue
        _, frame, drew3d, menu = rec
        # the picture fills the capture when its shape is the capture's; a dark edge column may shave a pixel or two
        expect = min(frame, capture)
        ok = abs(shape - expect) < 0.06 and not (menu and not drew3d and frame > 1.34)
        bad += 0 if ok else 1
        lines.append(f"{key} rect {shape:.3f} frame {frame:.3f} 3d {drew3d} menu {menu} {'ok' if ok else 'MISMATCH'}")
    lines.append(f"frames {frames} mismatched {bad}")
    return lines, frames, bad


def summarise(session, label):
    log = session.evidence / f"{label}-gtavcs.txt"
    results = session.evidence / f"{label}-results.txt"
    lines = []
    if log.exists():
        lines += [l for l in log.read_text(errors="replace").splitlines() if SUMMARY.search(l)]
    if results.exists():
        text = results.read_text(errors="replace")
        lines += [f"camera t={t} mode {m} {n}" for t, m, n in CAMERA.findall(text)]
    out = session.evidence / f"{label}-m09.txt"
    out.write_text("\n".join(lines) + "\n")
    session.say(f"{label}: {len(lines)} widescreen lines -> {out.name}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["preflight", "game", "shapes"])
    ap.add_argument("evidence")
    ap.add_argument("config", nargs="?")
    ap.add_argument("--label")
    ap.add_argument("--prompt")
    ap.add_argument("--keep-card", action="store_true")
    ap.add_argument("--settings")
    a = ap.parse_args()
    if a.command == "shapes":
        lines, frames, bad = shapes(a.evidence)
        print("\n".join(lines))
        return 0 if frames and not bad else 1
    session = m04.Session(Path(a.evidence))
    if a.command == "preflight":
        ok = m04.preflight(session)
    else:
        label = a.label or Path(a.config).stem
        text = Path(a.config).read_text()
        scheduled = m08.command_count(text)
        if scheduled > 512:
            session.say(f"{a.config} schedules {scheduled} commands: more than nxrun.cfg's 512, the game would refuse it")
            return 1
        folders = sorted(set(BURST.findall(text)))
        if folders:
            prepare_bursts(session, folders)
        ok = m06.game(session, a.config, label, a.prompt, a.keep_card, a.settings)
        m08.summarise(session, label)
        summarise(session, label)
        for local in fetch_bursts(session, label, folders) if folders else []:
            lines, frames, bad = shapes(local)
            report = session.evidence / f"{label}-{local.name}-shapes.txt"
            report.write_text("\n".join(lines) + "\n")
            session.say(f"{label}: {local.name} frames={frames} mismatched={bad} -> {report.name}")
            ok = ok and frames > 0 and bad == 0
    session.say(f"RESULT {a.command} {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
