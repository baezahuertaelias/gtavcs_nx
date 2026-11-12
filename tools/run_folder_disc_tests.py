"""Build and run the synthetic FolderDisc/GamePaths suite on the host, in the locked container.

    python tools/run_folder_disc_tests.py <evidence-dir>

Compiles src/vfs and tests/core/folder_disc_tests.cpp with the container's host g++ twice:
once with AddressSanitizer and UndefinedBehaviorSanitizer, once at -O2 without them. Both
logs, the commands and the source commit go to <evidence-dir>. Synthetic data only.
"""
import argparse
from pathlib import Path
import subprocess
import sys

from build_switch import IMAGE

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ("src/vfs/folder_disc.cpp", "src/vfs/game_paths.cpp", "tests/core/folder_disc_tests.cpp")
DRIVER = "int runFolderDiscTests();\nint main() { return runFolderDiscTests() ? 1 : 0; }\n"
VARIANTS = {
    "sanitized": "-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer",
    "optimized": "-O2",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("evidence")
    evidence = Path(parser.parse_args().evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    commit = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    dirty = bool(subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain", "--", *SOURCES], text=True).strip())
    ok = True
    for name, flags in VARIANTS.items():
        script = (
            "set -e; cd /tmp && rm -rf fd && mkdir fd && cd fd && "
            f"printf '%s' '{DRIVER}' > driver.cpp && g++ --version | head -1 && "
            f"g++ -std=c++20 -Wall -Wextra {flags} -I/src/src -I/src/src/vfs -I/src/upstream/third_party/PS2Recomp/ps2xRuntime/include "
            + " ".join(f"/src/{source}" for source in SOURCES)
            + " driver.cpp -o suite && ./suite"
        )
        command = ["docker", "run", "--rm", "--mount", f"type=bind,source={ROOT.as_posix()},target=/src,readonly",
                   IMAGE, "bash", "-c", script]
        result = subprocess.run(command, capture_output=True, text=True)
        log = evidence / f"folder-disc-{name}.log"
        log.write_text(f"commit {commit} sources_dirty={dirty}\nflags {flags}\n$ {' '.join(command)}\n"
                       f"{result.stdout}{result.stderr}exit {result.returncode}\n", encoding="utf-8")
        print(f"{name}: exit={result.returncode} log={log}")
        ok = ok and result.returncode == 0
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
