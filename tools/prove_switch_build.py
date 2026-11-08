"""Run inside the pinned container: clean clone, incremental/clean/failure proof."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    output = ROOT / "build/evidence/M01/pipeline-proof.log"
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gtavcs-proof-") as directory, output.open("w") as log:
        fresh = Path(directory) / "source"

        def run(command, cwd=ROOT, expected=0):
            result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            log.write("+ " + " ".join(map(str, command)) + "\n" + result.stdout + "\n")
            log.flush()
            if result.returncode != expected:
                raise RuntimeError(f"Exit {result.returncode}, expected {expected}: {command}")
            return result.stdout

        run(["git", "-c", "safe.directory=*", "clone", "--no-local", str(ROOT), str(fresh)])
        run(["git", "-c", "protocol.file.allow=always", "-c", f"submodule.upstream.url={ROOT / 'upstream'}",
             "submodule", "update", "--init"], fresh)
        candidate = run(["git", "rev-parse", "HEAD"], fresh).strip()
        command = ["python3", "tools/build_switch.py", "--inside"]
        run(command + ["--clean"], fresh)
        nro = fresh / "dist/switch/gtavcs_nx/gtavcs.nro"
        first = hashlib.sha256(nro.read_bytes()).hexdigest()
        incremental = run(command, fresh)
        assert "no work to do" in incremental, "Incremental build rebuilt unexpectedly"
        assert hashlib.sha256(nro.read_bytes()).hexdigest() == first
        source = fresh / "src/core_stub.c"
        source.write_text(source.read_text() + "\n#error M01 deliberate failed-build artifact rejection\n")
        run(command, fresh, expected=1)
        assert not nro.exists(), "Failed build retained deployable NRO"
        run(["git", "restore", "src/core_stub.c"], fresh)
        run(command + ["--clean"], fresh)
        assert hashlib.sha256(nro.read_bytes()).hexdigest() == first, "Clean rebuild changed NRO"
        result = {"candidate": candidate, "nro_sha256": first,
                  "result": "PASS fresh clone/submodule init, incremental no-op, failed-build rejection, identical clean NRO rebuild"}
        (output.parent / "pipeline-proof.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
