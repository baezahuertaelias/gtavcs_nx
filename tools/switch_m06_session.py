"""M06 hardware session: the game NRO from an empty test card through S1-S4 on the Switch over FTP and nxlink.

    python tools/switch_m06_session.py preflight <evidence-dir>
    python tools/switch_m06_session.py game <evidence-dir> <run.cfg> [--label NAME] [--prompt TEXT] [--keep-card]
    python tools/switch_m06_session.py summary <evidence-dir> <label> [--window NAME:FROM:TO ...]
    python tools/switch_m06_session.py cleanup <evidence-dir>
    python tools/switch_m06_session.py clean-sd|restore-sd <evidence-dir>

`game` is switch_m04_session.py's game run with the private game NRO (build/m04/private/gtavcs.nro). Before the run it
deletes the run's test profile (gamefolder/userdata/tests/<profile>) so the game starts from an empty test card, a
new settings file and no cache, unless --keep-card. After it, every file the run left in that profile goes to
<evidence-dir>/<label>/ except screenshots (PNG, game pictures: private, build/evidence/M06/screenshots/<label>) and
the memory cards. `summary` reads a run's perf.csv / gpu.csv / log into <label>-summary.txt: the camera at each gameinfo answer
(where the stages change) and median / p95 game, host and GPU frame time per --window NAME:FROM:TO (M06-11), heap / memory lines, GL errors, movie, controller and stage lines.
NX_HOST names the console (default 192.168.1.107). Nothing here is sent anywhere but the console.
"""
import argparse
import ftplib
import os
from pathlib import Path
import re
import statistics
import sys

os.environ.setdefault("NX_HOST", "192.168.1.107")
sys.path.insert(0, str(Path(__file__).resolve().parent))
import switch_m04_session as m04  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SHOTS = ROOT / "build/evidence/M06/screenshots"


def remove_tree(session, path):
    """Delete a remote folder: files first, then folders, deepest first. A missing folder is nothing to do."""
    ftp = m04.connect()
    try:
        try:
            missing = m04.listing(ftp, path) is None
        except ftplib.error_temp:  # Sphaira answers a missing folder with 450
            missing = True
        if missing:
            session.say(f"nothing to delete at {path}")
            return
        tree = m04.walk(ftp, path, depth=16)
        files = [p for p, kind in tree.items() if kind != "d"]
        folders = sorted((p for p, kind in tree.items() if kind == "d"), key=lambda p: p.count("/"), reverse=True)
        for p in files:
            ftp.delete(p)
        for p in folders:
            ftp.rmd(p)
        try:
            ftp.rmd(path)
        except ftplib.error_perm:
            pass
    finally:
        ftp.quit()
    session.say(f"removed {path}: files={len(files)} folders={len(folders)}")


BACKUP = "/switch/gtavcs_nx_backup"


def clean_sd(session):
    """M06-18's clean layout: move everything but the NRO and the disc files out of sdmc:/switch/gtavcs_nx into BACKUP
    (renames, nothing deleted), listing each move in <evidence>/sd-moves.tsv for restore_sd."""
    moves_file = session.evidence / "sd-moves.tsv"
    if moves_file.exists():
        session.say(f"refusing: {moves_file} exists (restore first)")
        return False
    ftp = m04.connect()
    try:
        top = m04.names(m04.listing(ftp, m04.SD))
        game = m04.names(m04.listing(ftp, m04.GAME))
        moves = [(f"{m04.SD}/{n}", f"{BACKUP}/{n}") for n in sorted(top) if n not in ("gtavcs.nro", "gamefolder")]
        moves += [(f"{m04.GAME}/{n}", f"{BACKUP}/gamefolder/{n}") for n in ("userdata", "gtavcs.log", "mc0", "mc1") if n in game]
        for folder in (BACKUP, f"{BACKUP}/gamefolder"):
            try:
                ftp.mkd(folder)
            except ftplib.all_errors:
                pass
        done = []
        for src, dst in moves:
            ftp.rename(src, dst)
            done.append((src, dst))
            moves_file.write_text("".join(f"{a}\t{b}\n" for a, b in done), encoding="utf-8")
            session.say(f"moved {src} -> {dst}")
    finally:
        ftp.quit()
    m04.record_tree(session, m04.SD, "sd-tree-clean.txt")
    return True


def restore_sd(session):
    moves_file = session.evidence / "sd-moves.tsv"
    if not moves_file.exists():
        session.say("nothing to restore")
        return True
    rows = [line.split("\t") for line in moves_file.read_text(encoding="utf-8").splitlines() if line]
    ftp = m04.connect()
    try:
        game = m04.names(m04.listing(ftp, m04.GAME))
        # the acceptance run made its own userdata (nxrun.cfg, the test profile) and log: keep them beside the backup
        for name in ("userdata", "gtavcs.log"):
            if name in game and any(src == f"{m04.GAME}/{name}" for src, _ in rows):
                ftp.rename(f"{m04.GAME}/{name}", f"{BACKUP}/acceptance-{name}")
                session.say(f"kept the acceptance run's {name} as {BACKUP}/acceptance-{name}")
        for src, dst in reversed(rows):
            parent, name = src.rsplit("/", 1)
            if name in m04.names(m04.listing(ftp, parent)):
                # the run made its own (the game creates gamefolder/mc0, mc1 at boot): keep it beside the backup
                ftp.rename(src, f"{BACKUP}/acceptance-{parent.rsplit('/', 1)[1]}-{name}")
                session.say(f"kept the acceptance run's {src} as {BACKUP}/acceptance-{parent.rsplit('/', 1)[1]}-{name}")
            ftp.rename(dst, src)
            session.say(f"restored {dst} -> {src}")
    finally:
        ftp.quit()
    moves_file.rename(moves_file.with_name("sd-moves-restored.tsv"))
    m04.record_tree(session, m04.SD, "sd-tree-restored.txt")
    return True


def game(session, config, label, prompt, keep_card):
    m04.NRO_LOCAL = ROOT / "build/m04/private/gtavcs.nro"
    text = Path(config).read_text()
    profile = m04.profile_of(text)
    if not profile:
        session.say("the M06 runs need a test profile (empty test card)")
        return False
    folder = f"{m04.GAME}/userdata/tests/{profile}"
    if not keep_card:
        remove_tree(session, folder)
    # a clean card (clean_sd) has no userdata yet: nxrun.cfg goes there
    ftp = m04.connect()
    try:
        ftp.mkd(f"{m04.GAME}/userdata")
        session.say(f"created {m04.GAME}/userdata")
    except ftplib.all_errors:
        pass
    ftp.quit()
    ok = m04.game(session, config, prompt, None)
    # m04.game names repeated runs label-2, label-3, ...: find the label it used from the newest log
    logs = sorted(session.evidence.glob(f"{Path(config).stem}*-gtavcs.log"), key=lambda p: p.stat().st_mtime)
    used = logs[-1].name[: -len("-gtavcs.log")] if logs else Path(config).stem
    if label and label != used:
        for p in session.evidence.glob(f"{used}-*"):
            p.rename(p.with_name(label + p.name[len(used):]))
        used = label
    out = session.evidence / used
    shots = SHOTS / used
    out.mkdir(parents=True, exist_ok=True)
    shots.mkdir(parents=True, exist_ok=True)
    ftp = m04.connect()
    try:
        tree = m04.walk(ftp, folder, depth=1)
    except ftplib.all_errors:
        tree = {}
    ftp.quit()
    count = 0
    for path, kind in sorted(tree.items()):
        name = path.rsplit("/", 1)[1]
        if kind == "d" or "/mc" in path[len(folder):]:
            continue
        target = shots / name if name.lower().endswith(".png") else out / name
        m04.fetch(session, path, target)
        count += 1
    session.say(f"{used}: read back {count} files from {folder} (screenshots private under {shots.relative_to(ROOT)})")
    return ok


def percentile(values, q):
    if not values:
        return float("nan")
    s = sorted(values)
    k = (len(s) - 1) * q
    lo = int(k)
    hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (k - lo)


def read_csv(path, key, time_key=None):
    rows = []
    if not path.exists():
        return rows
    lines = path.read_text(errors="replace").splitlines()
    if not lines:
        return rows
    head = lines[0].split(",")
    for line in lines[1:]:
        cells = line.split(",")
        if len(cells) != len(head):
            continue  # the last line of a closed game may be cut off
        row = dict(zip(head, cells))
        try:
            rows.append((float(row[time_key]) if time_key else 0.0, float(row[key])))
        except (KeyError, ValueError):
            continue
    return rows


def stage_camera(log):
    """(t, camera mode text) of every gameinfo answer: where each stage starts and ends in this run."""
    return [(float(t), m.strip()) for t, m in
            re.findall(r'^\[nxrun\] command t=([\d.]+) at=\S+ command="gameinfo" status=ok reply=".*?\[CAMERA  ([^]]*)\]',
                       log, re.M)]


def present_times(log):
    """(t, presented frames so far) from the progress lines, to place gpu.csv's frame numbers in time."""
    return [(float(t), int(n)) for t, n in re.findall(r"^\[nxrun\] progress t=([\d.]+) host_frames=(\d+)", log, re.M)]


def frame_time(points, frame):
    for (t0, n0), (t1, n1) in zip(points, points[1:]):
        if n0 <= frame <= n1:
            return t0 + (t1 - t0) * ((frame - n0) / (n1 - n0) if n1 > n0 else 0.0)
    return None


def summary(session, label, windows):
    """windows: [(name, from_s, to_s)], chosen from the run's own stage_camera() transitions (printed first)."""
    run = session.evidence / label
    log = (session.evidence / f"{label}-gtavcs.log").read_text(errors="replace")         if (session.evidence / f"{label}-gtavcs.log").exists() else ""
    out = [f"run {label}", "camera at each gameinfo answer (stage transitions):"]
    out.extend(f"  t={t:.0f} {mode}" for t, mode in stage_camera(log))
    perf = read_csv(run / "perf.csv", "frame_ms", "t_s")
    host = read_csv(run / "perf.csv", "host_ms", "t_s")
    points = present_times(log)
    gpu = []
    for frame, ms in read_csv(run / "gpu.csv", "gpu_ms", "frame"):
        t = frame_time(points, int(frame))
        if t is not None:
            gpu.append((t, ms))

    def stats(name, values):
        if not values:
            return f"{name}: no samples"
        return (f"{name}: n={len(values)} median={statistics.median(values):.1f} ms p95={percentile(values, 0.95):.1f} ms "
                f"max={max(values):.1f} ms")

    out.append(stats("game frame (whole run)", [v for _, v in perf]))
    for name, a, b in windows:
        out.append(stats(f"game frame {name} (t {a:g}-{b:g} s)", [v for t, v in perf if a <= t < b]))
        out.append(stats(f"host frame {name} (t {a:g}-{b:g} s)", [v for t, v in host if a <= t < b and v > 0]))
        out.append(stats(f"GPU present-to-present {name} (t {a:g}-{b:g} s)", [v for t, v in gpu if a <= t < b]))
    out.append("GPU present-to-present: GL_TIME_ELAPSED from just after one swap to just before the next (the swap "
               "itself is outside); includes GPU idle time while the CPU prepares the frame, so it bounds GPU busy time "
               "from above")
    out.append("windows apply to perf.csv's t_s, the running sum of game frame times, which drifts a few seconds from "
               "the run's clock (gameinfo, progress); GPU samples are placed by the run's clock")
    for pattern in (r"^\[video\].*$", r"^\[movie\].*$", r"^\[platform\] (?:gamepad|controller|focus|quit).*$",
                    r"^\[resources\].*$", r"^\[rhi\] GPU time.*$", r"^\[shutdown\].*$", r"^\[core\] orderly.*$",
                    r"^\[lifecycle\].*$"):
        out.extend(re.findall(pattern, log, re.M))
    errors = re.findall(r"gl errors (\d+)", log)
    out.append(f"last [rhi-stats] gl errors: {errors[-1] if errors else 'none reported (PS2X_RHI_STATS off)'}")
    info = re.findall(r'^\[nxrun\] command t=([\d.]+) at=\S+ command="(gameinfo|player)" status=(\w+) reply="(.*)"$',
                      log, re.M)
    out.append(f"gameinfo / player answers: {len(info)}")
    for t, command, status, reply in info:
        out.append(f"  t={t} {command} {status}: {reply[:300]}")
    text = "\n".join(out) + "\n"
    (session.evidence / f"{label}-summary.txt").write_text(text, encoding="utf-8")
    print(text)
    return bool(perf)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["preflight", "game", "summary", "cleanup", "clean-sd", "restore-sd"])
    ap.add_argument("evidence")
    ap.add_argument("config", nargs="?", help="game: the run.cfg; summary: the run's label")
    ap.add_argument("--label")
    ap.add_argument("--prompt")
    ap.add_argument("--keep-card", action="store_true")
    ap.add_argument("--window", action="append", default=[], metavar="NAME:FROM:TO",
                    help="summary: a stage window in seconds, from the run's camera transitions (repeatable)")
    a = ap.parse_args()
    session = m04.Session(a.evidence)
    try:
        if a.command == "preflight":
            ok = m04.preflight(session)
        elif a.command == "game":
            ok = game(session, a.config, a.label, a.prompt, a.keep_card)
        elif a.command == "clean-sd":
            ok = clean_sd(session)
        elif a.command == "restore-sd":
            ok = restore_sd(session)
        elif a.command == "summary":
            ok = summary(session, a.config, [(n, float(x), float(y)) for n, x, y in (w.rsplit(":", 2) for w in a.window)])
        else:
            m04.delete(session, f"{m04.GAME}/userdata/nxrun.cfg")
            ok = True
    except (OSError, *ftplib.all_errors) as error:
        session.say(f"ERROR {error}")
        ok = False
    session.say(f"RESULT {a.command} {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
