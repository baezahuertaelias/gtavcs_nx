"""The locked devkitPro container shared by the Switch build tools.

M01 built the platform smoke NRO (main.c + core_stub.c) here. Since M04 the root project is the real game, which
needs the overlaid upstream checkout and locked dependencies: build it with tools/build_game.py (the private game NRO)
or tools/build_game.py --public (source CI). This module keeps the image identity the other tools import.
"""
import os
from pathlib import Path
import subprocess
import sys

IMAGE = "gtavcs-nx-toolchain:m01"
PIN = "devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282"


def reclaim(root, *paths):
    """Give files the container wrote on the /work bind mount back to the host user.

    The container runs as root. Docker Desktop on Windows maps bind-mount ownership to the user, but a Linux
    engine keeps root as owner, so the host-side tools could not write next to the container's output (for
    example build/m04/private/manifest.json). No-op where the host has no POSIX user ids.
    """
    if not hasattr(os, "getuid"):
        return
    root = Path(root).resolve()
    targets = ["/work/" + Path(p).resolve().relative_to(root).as_posix() for p in paths if Path(p).exists()]
    if targets:
        subprocess.run(["docker", "run", "--rm", "--network=none",
                        "--mount", f"type=bind,source={root},target=/work", IMAGE,
                        "chown", "-R", f"{os.getuid()}:{os.getgid()}", *targets], check=True)


def main():
    print("The root build moved to tools/build_game.py (M04): the platform smoke target no longer exists.",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
