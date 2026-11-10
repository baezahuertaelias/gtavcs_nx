"""Record metadata-only identity of a private core build and audit its function table.

Runs in the Switch container against the private build volume. Compares every
generated table assignment with the linked ELF symbols and writes the FNV-1a digest
that the private harness must print on the console. Output holds hashes, counts and
public source paths only; never symbol names, game addresses or private content.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ASSIGN = re.compile(r"g_ps2RecompiledFunctionTable\[(\d+)\]\s*=\s*([\w:]+);\s*//\s*0x([0-9a-fA-F]+)")
BASE = re.compile(r"g_ps2RecompiledFunctionTableBase\s*=\s*0x([0-9a-fA-F]+)u")
OWNER = re.compile(r"PS2_GUEST_CC void (\w+)\(")
CASE = re.compile(r"\s*case 0x([0-9a-fA-F]+)u?:")
ANCHOR = "nx_table_digest_anchor"
FNV_OFFSET, FNV_PRIME, MASK = 0xcbf29ce484222325, 0x100000001b3, (1 << 64) - 1
NM = "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-nm"


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def symbols(elf):
    """Map demangled names, without parameter lists, to their distinct addresses."""
    output = subprocess.check_output([NM, "-C", str(elf)], text=True, errors="replace")
    table = {}
    for line in output.splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3 and parts[1] in "tTwW":
            table.setdefault(parts[2].split("(", 1)[0], set()).add(int(parts[0], 16))
    return table


def digest_slots(slots, anchor):
    value = FNV_OFFSET
    for index in sorted(slots):
        data = index.to_bytes(4, "little") + ((slots[index] - anchor) & MASK).to_bytes(8, "little")
        for byte in data:
            value = ((value ^ byte) * FNV_PRIME) & MASK
    return value


def audit_table(register, elf):
    text = Path(register).read_text(encoding="utf-8", errors="replace")
    base = int(BASE.search(text)[1], 16)
    known = symbols(elf)
    assigned, unresolved, ambiguous, misplaced = {}, 0, 0, 0
    for match in ASSIGN.finditer(text):
        index, name, guest = int(match[1]), match[2], int(match[3], 16)
        misplaced += guest != base + index * 4
        addresses = known.get(name) or known.get(name.lstrip(":"))
        if not addresses:
            unresolved += 1
        elif len(addresses) > 1:
            ambiguous += 1
        else:
            assigned[index] = next(iter(addresses))
    anchors = known.get(ANCHOR, set())
    if len(anchors) != 1:
        raise SystemExit(f"Expected one {ANCHOR} symbol")
    return {"slots_assigned": len(assigned), "distinct_targets": len(set(assigned.values())),
            "unresolved_targets": unresolved, "ambiguous_targets": ambiguous,
            "index_address_mismatches": misplaced,
            "expected_digest": f"{digest_slots(assigned, next(iter(anchors))):016x}"}


def reconcile_resume(generated, register):
    """Compare each owner's emitted resume switch with the registration table."""
    text = Path(register).read_text(encoding="utf-8", errors="replace")
    base = int(BASE.search(text)[1], 16)
    table = {base + int(m[1]) * 4: m[2] for m in ASSIGN.finditer(text)}
    owners, targets, jumps = set(), {}, {}
    for path in Path(generated).glob("*.cpp"):
        if path.name == Path(register).name:
            continue
        owner, inside = None, None
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            match = OWNER.match(line)
            stripped = line.strip()
            if match:
                owner = match[1]
                owners.add(owner)
            elif stripped == "switch (ctx->pc) {":
                inside = targets
            elif stripped == "switch (jumpTarget) {":
                inside = jumps
            elif inside is not None and (stripped.startswith("default:") or stripped == "}"):
                inside = None
            elif inside is not None:
                case = CASE.match(line)
                if case:
                    inside.setdefault(int(case[1], 16), set()).add(owner)
    missing = {address: names for address, names in targets.items() if address not in table}
    jump_only = {address: names for address, names in jumps.items() if address not in targets}
    jump_absent = {address: names for address, names in jump_only.items() if address not in table}
    redirected = sum(address in table and table[address] not in names for address, names in targets.items())
    starts = {address for address, name in table.items() if name.endswith(f"_0x{address:x}")}
    return {"owners_with_definitions": len(owners),
            "owners_with_resume_switch": len({name for names in targets.values() for name in names}),
            "switch_targets": len(targets), "targets_in_table_same_owner": len(targets) - len(missing) - redirected,
            "targets_absent_from_table": len(missing), "targets_redirected": redirected,
            "owners_with_absent_targets": len({name for names in missing.values() for name in names}),
            "jump_table_targets": len(jumps), "jump_targets_not_resume_targets": len(jump_only),
            "jump_targets_absent_from_table": len(jump_absent),
            "owners_with_absent_jump_targets": len({name for names in jump_absent.values() for name in names}),
            "owners_with_any_switch": len({name for group in (targets, jumps) for names in group.values() for name in names}),
            "union_of_switch_and_jump_targets": len(targets.keys() | jumps.keys()),
            "table_slots": len(table), "slots_at_owner_start": len(starts),
            "slots_at_owner_start_also_switch_target": len(starts & targets.keys()),
            "slots_neither_owner_start_nor_switch_target": len(table.keys() - starts - targets.keys())}


def compare_upstream(upstream, manifest):
    expected = json.loads(Path(manifest).read_text(encoding="utf-8"))["upstream_inputs"]
    missing = sorted(name for name in expected if not (Path(upstream) / name).is_file())
    changed = sorted(name for name in expected if name not in missing and sha256(Path(upstream) / name) != expected[name])
    return {"public_inputs": len(expected), "missing": missing, "changed": changed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--register", required=True)
    parser.add_argument("--stamp", required=True)
    parser.add_argument("--elf", required=True)
    parser.add_argument("--nro", required=True)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--runtime-library", required=True)
    parser.add_argument("--upstream", required=True)
    parser.add_argument("--public-manifest", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    upstream = compare_upstream(args.upstream, args.public_manifest)
    table = audit_table(args.register, args.elf)
    resume = reconcile_resume(Path(args.register).parent, args.register)
    report = {"candidate_commit": args.candidate, "upstream_vs_public_manifest": upstream, "table": table,
              "resume_reconciliation": resume,
              "sha256": {"register_functions": sha256(args.register), "recompile_stamp": sha256(args.stamp),
                         "archive": sha256(args.archive), "runtime_library": sha256(args.runtime_library),
                         "elf": sha256(args.elf), "nro": sha256(args.nro)},
              "bytes": {"archive": Path(args.archive).stat().st_size, "nro": Path(args.nro).stat().st_size}}
    Path(args.output).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"table": table, "resume": resume, "public_inputs": upstream["public_inputs"],
                      "missing": len(upstream["missing"]), "changed": len(upstream["changed"])}))
    failed = upstream["missing"] or upstream["changed"] or table["unresolved_targets"] or table["ambiguous_targets"] or table["index_address_mismatches"]
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
