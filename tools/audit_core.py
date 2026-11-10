"""Emit metadata-only M02 source/generation audit from a private reference build.

Never emits ELF bytes, instructions, generated bodies, asset payloads or saves.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def classify(token):
    if token.startswith("__m"):
        return "vector_layout"
    if "trin" in token or token.startswith("__"):
        return "compiler_or_include_boundary"
    name = token.removeprefix("_mm_")
    if name in ("getcsr", "setcsr"):
        return "floating_point_environment"
    if name.startswith(("load", "store", "stream", "maskmove", "prefetch")):
        return "memory_alignment_and_access"
    if name.startswith(("cast", "cvt", "cvtt")):
        return "bit_cast_or_numeric_conversion"
    if name.startswith(("shuffle", "unpack", "pack", "insert", "extract", "set", "move", "blend")):
        return "lane_order_and_packing"
    if name.startswith(("sll", "srl", "sra")):
        return "shift_signedness_and_width"
    if name.startswith(("and", "or", "xor", "cmp", "test")):
        return "bitwise_or_comparison"
    if name.endswith(("_ps", "_ss", "_pd", "_sd")):
        return "floating_point_arithmetic"
    if name.startswith(("add", "sub", "mul", "min", "max", "abs", "madd", "avg", "sign")):
        return "integer_arithmetic_and_saturation"
    return "requires_manual_classification"


def audit(reference, output):
    summary = load(ROOT / "docs/discovery/summary.json")
    commit = subprocess.check_output(["git", "-C", str(reference), "rev-parse", "HEAD"], text=True).strip()
    if commit != summary["upstream_commit"]:
        raise RuntimeError("Reference checkout does not match pinned source")
    owners = defaultdict(list)
    for function in load(ROOT / "docs/discovery/source_functions.json"):
        owners[function["file"]].append(function)
    records = []
    for candidate in load(ROOT / "docs/discovery/portability_candidates.json"):
        if candidate["category"] != "x86_simd":
            continue
        functions = [f for f in owners[candidate["file"]]
                     if f["line"] <= candidate["line"] <= f["end_line"]]
        owner = min(functions, key=lambda f: f["end_line"] - f["line"]) if functions else None
        records.append({**candidate, "owner": owner["declarator"] if owner else None,
                        "operations": {token: classify(token) for token in candidate["tokens"]}})
    save(output / "simd_candidates.json", {"source": commit,
         "limitation": "Lexical operation families, not numeric conformance; null owner retains parser uncertainty",
         "count": len(records), "records": records})

    log = (reference / "build/recomp.log").read_text(errors="replace")
    events = []
    for level, category, function, address in re.findall(
            r"\[(warning|error)\] ([\w-]+)(?: function=([\w]+))?(?: addr=(0x[0-9a-fA-F]+))?", log):
        events.append({"level": level, "category": category,
                       "function": function or None, "address": address or None})
    counters = {}
    for label in ("Functions discovered", "Generated functions", "Additional entrypoints",
                  "Unhandled instructions", "Indirect fallback promotions"):
        match = re.search(re.escape(label) + r": (\d+)", log)
        counters[label] = int(match[1]) if match else None
    elf = reference / "build/elf/SLUS_215.90"
    elf_sha1 = hashlib.sha1(elf.read_bytes()).hexdigest()
    if elf_sha1 != summary["disc_identity"]["sha1"]:
        raise RuntimeError("Unsupported ELF identity")
    generated = reference / "generated"
    registration = (generated / "ee/register_functions.cpp").read_text()
    assignments = re.findall(r"g_ps2RecompiledFunctionTable\[(\d+)\] = ([\w]+);", registration)
    intrinsic_counts = Counter()
    constructs = Counter()
    cpp_files = list((generated / "ee").glob("*.cpp")) + list((generated / "vu1").glob("*.cpp"))
    for path in cpp_files:
        text = path.read_text(errors="strict")
        intrinsic_counts.update(re.findall(r"\b_mm_[A-Za-z0-9_]+\b", text))
        constructs.update(re.findall(r"\b(?:__int128|__m128i|__m128|__m256|thread_local|__attribute__|reinterpret_cast)\b", text))
    native = {}
    for name, relative in {
        "recompiler": "release/third_party/PS2Recomp/ps2xRecomp/ps2_recomp.exe",
        "analyzer": "release/third_party/PS2Recomp/ps2xAnalyzer/ps2_analyzer.exe",
        "desktop_reference": "vcs.exe",
    }.items():
        data = (reference / "build" / relative).read_bytes()
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        machine = struct.unpack_from("<H", data, pe + 4)[0]
        if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0" or machine != 0x8664:
            raise RuntimeError(f"Expected native x86-64 PE tool: {name}")
        native[name] = {"machine": "AMD64 PE", "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
    save(output / "generation_metadata.json", {
        "source": commit, "elf_sha1": elf_sha1, "stamp": load(generated / "recompile_stamp.json"),
        "report_counts": counters, "diagnostic_counts": dict(Counter(e["level"] for e in events)),
        "diagnostic_events": events,
        "registration_assignments": len(assignments),
        "unique_registered_owner_symbols": len({symbol for _, symbol in assignments}),
        "generated_cpp_files": len(cpp_files), "intrinsic_calls": dict(sorted(intrinsic_counts.items())),
        "constructs": dict(constructs), "native_binaries": native,
        "limitations": ["Diagnostics remain unresolved until their owning tasks validate them",
                        "No generated bodies or instruction bytes are included", "Compilation is not ARM64 execution proof"]})
    print(f"PASS metadata audit: {len(records)} SIMD candidates; {len(cpp_files)} private C++ files; "
          f"{len(assignments)} registrations; diagnostics {dict(Counter(e['level'] for e in events))}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/milestones/M02")
    args = parser.parse_args()
    audit(args.reference.resolve(), args.output.resolve())
