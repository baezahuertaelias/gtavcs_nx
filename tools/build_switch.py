"""The locked devkitPro container shared by the Switch build tools.

M01 built the platform smoke NRO (main.c + core_stub.c) here. Since M04 the root project is the real game, which
needs the overlaid upstream checkout and locked dependencies: build it with tools/build_game.py (the private game NRO)
or tools/build_game.py --public (source CI). This module keeps the image identity the other tools import.
"""
import sys

IMAGE = "gtavcs-nx-toolchain:m01"
PIN = "devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282"


def main():
    print("The root build moved to tools/build_game.py (M04): the platform smoke target no longer exists.",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
