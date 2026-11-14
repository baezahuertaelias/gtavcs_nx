"""M05 hardware session: the RHI conformance NRO and OpenGL game runs on the Switch over FTP and nxlink.

    python tools/switch_m05_session.py preflight <evidence-dir>
    python tools/switch_m05_session.py rhi <evidence-dir> [--label NAME] [--prompt TEXT]
    python tools/switch_m05_session.py game <evidence-dir> <run.cfg> [--prompt TEXT]
    python tools/switch_m05_session.py cleanup <evidence-dir>

`rhi` sends the public build/m05/rhi-tests/gtavcs_rhi_tests.nro, waits for it to end (conformance scenes, the M05-09
threading measurement, then 75 s of the window phase: dock / undock by hand from 12 s), reads
sdmc:/switch/gtavcs_nx/rhi-tests back into <evidence-dir>/<label>/ and compares the readbacks with the host Vulkan
reference (build/m05/host/vulkan_hw: upstream's Vulkan backend on the host GPU, tools/rhi_host_tests.py) into <label>-compare.json. `game` is
switch_m04_session.py's game run with the M05 game NRO (build/m04/private/gtavcs.nro, the OpenGL renderer), and also
reads back the run's screenshots (PNG, game pictures: private, kept under build/ only; their hashes go to the
transcript). NX_HOST names the console (default 192.168.1.91).
"""
import argparse
import ftplib
import hashlib
import os
from pathlib import Path
import subprocess
import sys

os.environ.setdefault("NX_HOST", "192.168.1.91")
sys.path.insert(0, str(Path(__file__).resolve().parent))
import switch_m04_session as m04  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
RHI_LOCAL = ROOT / "build/m05/rhi-tests/gtavcs_rhi_tests.nro"
RHI_SD = m04.SD + "/gtavcs_rhi_tests.nro"
RHI_OUT = m04.SD + "/rhi-tests"
REFERENCE = ROOT / "build/m05/host/vulkan_hw"  # upstream Vulkan on the host GPU (tools/rhi_host_tests.py)


def clear_remote(session, folder):
    ftp = m04.connect()
    try:
        for name, size in m04.names(m04.listing(ftp, folder)).items():
            if size is not None:
                ftp.delete(f"{folder}/{name}")
    except ftplib.all_errors:
        pass
    ftp.quit()
    session.say(f"cleared {folder}")


def rhi(session, label, prompt):
    local_sha = hashlib.sha256(RHI_LOCAL.read_bytes()).hexdigest()
    session.say(f"== {label}: rhi tests nro sha256={local_sha}")
    before = m04.crash_list(session, f"crash-before-{label}.txt")
    clear_remote(session, RHI_OUT)
    if not m04.nxlink(session, RHI_LOCAL, RHI_SD) or not m04.wait_back(session, 1800, prompt):
        return False
    out = session.evidence / label
    out.mkdir(parents=True, exist_ok=True)
    ftp = m04.connect()
    files = m04.names(m04.listing(ftp, RHI_OUT))
    ftp.quit()
    for name, size in sorted(files.items()):
        if size is not None:
            m04.fetch(session, f"{RHI_OUT}/{name}", out / name)
    after = m04.crash_list(session, f"crash-after-{label}.txt")
    summary = (out / "summary.txt").read_text() if (out / "summary.txt").exists() else ""
    compare = subprocess.run([sys.executable, ROOT / "tools/rhi_compare.py", REFERENCE, out, "--report",
                              session.evidence / f"{label}-compare.json"], capture_output=True, text=True)
    (session.evidence / f"{label}-compare.txt").write_text(compare.stdout + compare.stderr)
    session.say(f"{label}: summary={summary.splitlines()[:3]} compare_exit={compare.returncode} "
                f"compare_tail={compare.stdout.strip().splitlines()[-1:]} sd_matches_local="
                f"{m04.remote_sha256(RHI_SD) == local_sha} new_crashes={sorted(after - before)}")
    return summary.startswith("DONE") and compare.returncode == 0 and after == before


def game(session, config, prompt):
    m04.NRO_LOCAL = ROOT / "build/m04/private/gtavcs.nro"
    ok = m04.game(session, config, prompt, None)
    profile = m04.profile_of(Path(config).read_text())
    folder = f"{m04.GAME}/userdata/tests/{profile}"
    shots = ROOT / "build/evidence/M05/screenshots" / Path(config).stem
    shots.mkdir(parents=True, exist_ok=True)
    ftp = m04.connect()
    try:
        names = m04.names(m04.listing(ftp, folder))
    except ftplib.all_errors:
        names = {}
    ftp.quit()
    for name, size in sorted(names.items()):
        if size is not None and name.endswith(".png"):
            m04.fetch(session, f"{folder}/{name}", shots / name)  # game pictures: private (build/ is ignored)
    session.say(f"screenshots read back: {len(names)} to {shots.relative_to(ROOT)} (private)")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["preflight", "rhi", "game", "cleanup"])
    ap.add_argument("evidence")
    ap.add_argument("config", nargs="?")
    ap.add_argument("--label", default="rhi")
    ap.add_argument("--prompt")
    a = ap.parse_args()
    session = m04.Session(a.evidence)
    try:
        if a.command == "preflight":
            ok = m04.preflight(session)
        elif a.command == "rhi":
            ok = rhi(session, a.label, a.prompt)
        elif a.command == "game":
            ok = game(session, a.config, a.prompt)
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
