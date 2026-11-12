"""Copy the extracted game to the Switch and run the M03 hardware checks over FTP and nxlink.

    python tools/switch_m03_session.py upload <gamefolder> <evidence-dir>
    python tools/switch_m03_session.py run <evidence-dir> [--suite disc|runtime]

Needs Sphaira open in application mode with FTP and nxlink available. `upload` copies only
disc files and .nxdisc into sdmc:/switch/gtavcs_nx/gamefolder, skipping files whose size
already matches, and never deletes anything on the card. Every step goes to transcript.txt.
"""
import argparse
import ftplib
import hashlib
import os
import re
from pathlib import Path
import subprocess
import sys
import time

HOST, PORT = "192.168.1.107", 5000
ROOT = Path(__file__).resolve().parents[1]
SD = "/switch/gtavcs_nx"
BLOCK = 1 << 20
SUITES = {"disc": ("gtavcs_disc_tests.nro", "disc-tests-M03.log"),
          "runtime": ("gtavcs_runtime_tests.nro", "runtime-tests-M02.log")}
PAYLOAD, LOG = SUITES["disc"]


class Session:
    def __init__(self, evidence):
        self.evidence = Path(evidence)
        self.evidence.mkdir(parents=True, exist_ok=True)
        self.transcript = open(self.evidence / "transcript.txt", "a", encoding="utf-8")

    def say(self, text):
        line = time.strftime("%H:%M:%S ") + text
        print(line, flush=True)
        self.transcript.write(line + "\n")
        self.transcript.flush()


def connect():
    ftp = ftplib.FTP()
    ftp.connect(HOST, PORT, timeout=60)
    ftp.login()
    ftp.set_pasv(True)
    return ftp


def listing(ftp, path):
    rows = []
    ftp.retrlines("LIST " + path, rows.append)
    return rows


def entries(ftp, path):
    """Map a remote folder's names to size, or None for a directory."""
    found = {}
    try:
        rows = listing(ftp, path)
    except ftplib.error_temp:
        return found
    for row in rows:
        parts = row.split(None, 8)
        found[parts[8]] = None if row.startswith("d") else int(parts[4])
    return found


def ensure_directory(ftp, path, known):
    parent, name = path.rsplit("/", 1)
    if name not in known.setdefault(parent, entries(ftp, parent)):
        ftp.mkd(path)
        known[parent][name] = None
        known[path] = {}


def upload(session, gamefolder):
    root = Path(gamefolder)
    files = sorted(p for p in root.rglob("*") if p.is_file())
    meta = root / ".nxdisc" / "manifest.tsv"
    listed = {line.split("\t")[4] for line in meta.read_text(encoding="ascii").splitlines() if line.startswith("F\t")}
    selected = [p for p in files if p.relative_to(root).as_posix() in listed or ".nxdisc" in p.parts]
    if len(selected) != len(listed) + 2:
        raise SystemExit(f"Expected {len(listed) + 2} files, found {len(selected)}")
    ftp, known = connect(), {}
    target = f"{SD}/gamefolder"
    total = sum(p.stat().st_size for p in selected)
    done, start, sent = 0, time.time(), 0
    for path in selected:
        relative = path.relative_to(root).as_posix()
        pieces = relative.split("/")
        folder = target
        for piece in pieces[:-1]:
            ensure_directory(ftp, f"{folder}/{piece}", known)
            folder = f"{folder}/{piece}"
        here = known.setdefault(folder, entries(ftp, folder))
        size = path.stat().st_size
        if here.get(pieces[-1]) != size:
            with open(path, "rb") as handle:
                ftp.storbinary(f"STOR {folder}/{pieces[-1]}", handle, BLOCK)
            sent += size
        done += size
        if size > 16 << 20:
            session.say(f"{done * 100 // total}% {relative} {(sent / 1048576) / max(time.time() - start, 0.1):.1f} MiB/s")
    ftp.quit()
    session.say(f"upload done files={len(selected)} bytes={total} sent={sent} seconds={time.time() - start:.0f}")


def fetch(session, remote, local):
    digest, size = hashlib.sha256(), 0
    ftp = connect()
    with open(local, "wb") as handle:
        def sink(chunk):
            nonlocal size
            handle.write(chunk)
            digest.update(chunk)
            size += len(chunk)
        ftp.retrbinary("RETR " + remote, sink)
    ftp.quit()
    session.say(f"read-back {remote} bytes={size} sha256={digest.hexdigest()}")
    return digest.hexdigest()


def record(session, path, name):
    ftp = connect()
    rows = listing(ftp, path)
    ftp.quit()
    (session.evidence / name).write_text("\n".join(rows) + "\n", encoding="utf-8")
    session.say(f"listing {path} entries={len(rows)} sha256={hashlib.sha256(chr(10).join(rows).encode()).hexdigest()} file={name}")
    return rows


def wait_log(session, limit):
    start, last, stable = time.time(), None, 0
    time.sleep(5)
    while time.time() - start < limit:
        try:
            ftp = connect()
            current = entries(ftp, SD).get(LOG)
            ftp.quit()
        except ftplib.all_errors:
            current = None
        stable = stable + 1 if current is not None and current == last else 0
        last = current
        if stable >= 2:
            session.say(f"log stable size={current} after {int(time.time() - start)}s")
            return True
        time.sleep(10)
    session.say(f"log TIMEOUT last={last}")
    return False


def run(session):
    bundle = ROOT / "build/m02/test-bundle"
    expected = hashlib.sha256((bundle / PAYLOAD).read_bytes()).hexdigest()
    session.say(f"payload local sha256={expected}")
    ftp = connect()
    if LOG in entries(ftp, SD):
        ftp.quit()
        fetch(session, f"{SD}/{LOG}", session.evidence / f"previous-{LOG}")
        ftp = connect()
        session.say(f"delete after archive {LOG}: {ftp.delete(SD + '/' + LOG)}")
    ftp.quit()
    baseline = record(session, "/atmosphere/crash_reports", "crash-baseline.txt")
    environment = dict(os.environ, MSYS_NO_PATHCONV="1")
    command = ["docker", "run", "--rm", "--mount", f"type=bind,source={bundle.as_posix()},target=/p,readonly",
               "gtavcs-nx-toolchain:m01", "/opt/devkitpro/tools/bin/nxlink", "-a", HOST, "-r", "2",
               "-p", f"{SD}/{PAYLOAD}", f"/p/{PAYLOAD}"]
    for attempt in range(1, 4):
        result = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=300)
        session.say(f"nxlink attempt={attempt} exit={result.returncode} output={(result.stdout + result.stderr).strip()!r}")
        if result.returncode == 0:
            break
        time.sleep(8)
    else:
        return False
    if not wait_log(session, 1500):
        return False
    time.sleep(3)
    fetch(session, f"{SD}/{LOG}", session.evidence / LOG)
    text = (session.evidence / LOG).read_text(encoding="utf-8", errors="replace")
    finished = re.findall(r"finished failures=(\d+)", text)
    passed = len(finished) == 1 and finished[0] == "0"
    session.say(f"log finished failures={finished[0] if finished else 'missing'} tests_pass={passed}")
    actual = fetch(session, f"{SD}/{PAYLOAD}", session.evidence / "sd-readback.tmp")
    (session.evidence / "sd-readback.tmp").unlink()
    session.say(f"payload sd_matches_local={actual == expected}")
    crashes = record(session, "/atmosphere/crash_reports", "crash-after.txt")
    session.say(f"new_crash_entries={sorted(set(crashes) - set(baseline))}")
    record(session, SD + "/gamefolder", "gamefolder-after.txt")
    return passed and actual == expected and crashes == baseline


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["upload", "run"])
    parser.add_argument("first")
    parser.add_argument("second", nargs="?")
    parser.add_argument("--suite", choices=sorted(SUITES), default="disc")
    args = parser.parse_args()
    global PAYLOAD, LOG
    PAYLOAD, LOG = SUITES[args.suite]
    session = Session(args.second if args.command == "upload" else args.first)
    if args.command == "upload":
        upload(session, args.first)
    else:
        try:
            ok = run(session)
        except (OSError, EOFError, *ftplib.all_errors, subprocess.TimeoutExpired) as error:
            # A refused FTP connection or a dropped link: recorded, never a silent traceback.
            session.say(f"session error {type(error).__name__}: {error}")
            ok = False
        session.say(f"session end ok={ok}")
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
