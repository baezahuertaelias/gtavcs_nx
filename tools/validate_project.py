"""Source-only foundation checks; no game data or Switch required."""
import json
import hashlib
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    errors = []
    plan = (ROOT / "docs/MILESTONES.md").read_text(encoding="utf-8")
    tasks = re.findall(r"^- \[[ xX]\] (M\d{2}-\d{2})\b", plan, re.M)
    if len(tasks) != len(set(tasks)): errors.append("Duplicate milestone task IDs")
    milestones = sorted(set(re.findall(r"^## (M\d{2}) ", plan, re.M)))
    if milestones != [f"M{i:02d}" for i in range(17)]: errors.append("Milestones must be exactly M00-M16")
    for i in range(17):
        milestone = f"M{i:02d}"
        if not any(task.startswith(milestone + "-") for task in tasks): errors.append(f"No tasks for {milestone}")
        if len(re.findall(r"^Branch: `milestone/[^`]+` <!-- " + milestone + " -->$", plan, re.M)) != 1:
            errors.append(f"Missing/duplicate branch for {milestone}")
    required = ["AGENTS.md", "CMakeLists.txt", "src/main.c", "src/core.h", "src/core/core.cpp",
                "src/platform/platform_switch_sdl2.cpp", "src/switch/thread_stack.c",
                "third_party/glad/src/gl.c", "third_party/glad/include/glad/gl.h",
                "docs/PORT_DISCOVERY.md", "docs/AUTOMATED_EXECUTION.md"]
    for name in required:
        if not (ROOT / name).is_file(): errors.append(f"Missing {name}")
    cmake = (ROOT / "CMakeLists.txt").read_text()
    # port-owned sources only: "${upstream}/src/..." names files of the overlaid upstream checkout
    for source in re.findall(r"(?<![}/\w])(?:src|third_party/glad)/[A-Za-z0-9_/.]+\.(?:cpp|c)\b", cmake):
        if not (ROOT / source).is_file(): errors.append(f"Missing CMake source {source}")
    data = ROOT / "docs/discovery"
    summary = json.loads((data / "summary.json").read_text())
    for count, name in (("source_files", "source_files.json"), ("source_function_definitions", "source_functions.json"),
                        ("guest_unique_entry_addresses", "guest_functions.json"), ("parser_gap_nodes", "parser_gaps.json"),
                        ("vu_upload_records", "vu_uploads.json"), ("runtime_macro_call_entries", "runtime_call_macros.json")):
        if summary[count] != len(json.loads((data / name).read_text())): errors.append(f"Inventory count mismatch: {name}")
    for record in json.loads((data / "source_files.json").read_text()):
        path = ROOT / "upstream" / record["path"]
        if not path.is_file(): errors.append(f"Inventory source missing: {record['path']}")
        elif hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest() != record["sha256"]:
            errors.append(f"Inventory source hash drift: {record['path']}")
    entries = json.loads((data / "disc_manifest.json").read_text())["entries"]
    if sum(not x["directory"] for x in entries) != summary["disc_files"]: errors.append("Disc file count mismatch")
    tracked = subprocess.check_output(["git", "ls-files", "--stage"], cwd=ROOT, text=True).splitlines()
    forbidden = {".iso", ".elf", ".nro", ".nacp", ".irx", ".img", ".bin", ".dir", ".vb", ".raw", ".sdt", ".psp", ".xtx", ".lvz", ".scm", ".gxt", ".pss", ".dtz", ".exe", ".dll", ".o", ".a"}
    submodule = False
    for line in tracked:
        meta, name = line.split("\t", 1)
        if meta.startswith("160000 "):
            if name == "upstream" and meta.split()[1] == summary["upstream_commit"]: submodule = True
            else: errors.append(f"Unexpected submodule/revision: {name}")
            continue
        p = Path(name)
        if p.suffix.lower() in forbidden or "generated" in p.parts or p.name.startswith("SLUS_"):
            errors.append(f"Forbidden tracked artifact: {name}")
        if p.parts[0] in ("build", "dist", "gamefolder", "userdata", "secrets") or p.name.startswith(".env"):
            errors.append(f"Forbidden tracked local data: {name}")
    if not submodule: errors.append("Pinned upstream submodule missing")
    if errors:
        print("FAIL\n" + "\n".join(errors))
        return 1
    print(f"PASS foundation: {len(tasks)} unique tasks, 17 milestones, inventory consistency, source paths and tracked-artifact policy")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
