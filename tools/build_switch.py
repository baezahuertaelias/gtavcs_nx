"""Build only the platform smoke NRO with the immutable devkitPro container."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
IMAGE = "gtavcs-nx-toolchain:m01"
PIN = "devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282"


def run(args):
    print("+ " + " ".join(map(str, args)), flush=True)
    return subprocess.check_output(args, cwd=ROOT, stderr=subprocess.STDOUT, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--inside", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    staged = ROOT / "dist/switch/gtavcs_nx/gtavcs.nro"
    staged.unlink(missing_ok=True)
    logdir = ROOT / "build/evidence/M01"
    logdir.mkdir(parents=True, exist_ok=True)
    try:
        if not args.inside:
            print(run(["docker", "build", "-f", "Dockerfile.switch", "-t", IMAGE, "."]), flush=True)
            command = ["docker", "run", "--rm", "--network=none", "--mount",
                       f"type=bind,source={ROOT},target=/work", "-w", "/work",
                       IMAGE, "python3", "tools/build_switch.py", "--inside"]
            if args.clean:
                command.append("--clean")
            subprocess.run(command, cwd=ROOT, check=True)
            return 0
        packages = run(["dkp-pacman", "-Q"])
        if packages != (ROOT / "docs/milestones/M01/package-lock.txt").read_text():
            raise RuntimeError("Toolchain package lock mismatch")
        build = ROOT / "build/switch"
        if args.clean and build.exists():
            if build.is_symlink() or build.resolve() != ROOT / "build/switch":
                raise RuntimeError("Unsafe build directory")
            shutil.rmtree(build)
        commands = [
            ["cmake", "-S", ".", "-B", "build/switch", "-G", "Ninja",
             "-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake", "-DCMAKE_BUILD_TYPE=Release"],
            ["cmake", "--build", "build/switch", "--parallel", str(os.cpu_count() or 1), "--target", "gtavcs_nro"],
            ["python3", "tools/inspect_switch.py"],
            ["cmake", "--install", "build/switch", "--prefix", "dist"],
        ]
        with (logdir / "build.log").open("w") as log:
            for command in commands:
                output = run(command)
                print(output, flush=True)
                log.write(output)
                log.flush()
        hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in build.glob("gtavcs.*") if p.is_file()}
        (logdir / "checksums.json").write_text(json.dumps({"image": PIN, "sha256": hashes}, indent=2) + "\n")
        return 0
    except (subprocess.CalledProcessError, RuntimeError) as error:
        staged.unlink(missing_ok=True)
        output = getattr(error, "output", "") or str(error)
        print(output, file=sys.stderr)
        with (logdir / "failure.log").open("a") as log:
            log.write(output + "\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
