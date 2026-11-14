"""Build and run the M04 core's host-testable pieces in the locked container, with and without sanitizers.

    python tools/run_core_host_tests.py <evidence-dir>

Suites: src/core/run_config.cpp (tests/core/run_config_tests.cpp) and the Switch branch of the overlay's
runtime/ps2_host_fs.h compiled with -D__SWITCH__ (tests/core/host_fs_logic_tests.cpp). Each is built with the
container's host g++ twice - AddressSanitizer + UndefinedBehaviorSanitizer, and -O2 - and run with address
randomization off, as tools/run_folder_disc_tests.py does. Logs, commands and the source commit go to <evidence-dir>.
"""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

from build_switch import IMAGE
from upstream_overlay import prepare

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = {
    "sanitized": "-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer",
    "optimized": "-O2",
}
SUITES = {
    "run_config": ("src/core/run_config.cpp tests/core/run_config_tests.cpp", "-Isrc", "runRunConfigTests", ""),
    "host_fs_logic": ("tests/core/host_fs_logic_tests.cpp", "-I{runtime}/include", "runHostFsLogicTests", "-D__SWITCH__"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("evidence")
    evidence = Path(parser.parse_args().evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    series = (ROOT / "patches/upstream/series.json").read_bytes()
    source = ROOT / ("build/m04/game-source-" + hashlib.sha256(series).hexdigest()[:16])
    prepare(source)
    runtime = "/work/" + source.relative_to(ROOT).as_posix() + "/third_party/PS2Recomp/ps2xRuntime"
    commit = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    dirty = bool(subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain"], text=True).strip())
    ok = True
    for suite, (sources, include, entry, defines) in SUITES.items():
        driver = f"int {entry}();\\nint main() {{ return {entry}() ? 1 : 0; }}\\n"
        for name, flags in VARIANTS.items():
            script = (
                f"set -e; mkdir -p /tmp/t && cd /work && printf '{driver}' > /tmp/t/driver.cpp && g++ --version | head -1 && "
                f"g++ -std=c++20 -Wall -Wextra {flags} {defines} {include.format(runtime=runtime)} {sources} "
                f"/tmp/t/driver.cpp -o /tmp/t/{suite} && setarch -R timeout 600 /tmp/t/{suite}")
            command = ["docker", "run", "--rm", "--network=none", "--security-opt", "seccomp=unconfined",
                       "--mount", f"type=bind,source={ROOT},target=/work", IMAGE, "bash", "-c", script]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            log = evidence / f"{suite}-{name}.log"
            log.write_text(f"commit {commit} dirty={dirty}\n+ {' '.join(command)}\n{result.stdout}\nexit={result.returncode}\n")
            print(f"{'PASS' if result.returncode == 0 else 'FAIL'} {suite} {name} ({log.name})", flush=True)
            ok = ok and result.returncode == 0
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
