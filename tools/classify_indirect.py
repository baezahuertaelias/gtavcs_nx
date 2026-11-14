"""Classify M02's indirect-fallback sites against code that actually runs, and check the entry list (M04-12).

    python tools/classify_indirect.py <generated-ee-dir> <out.json> --trace NAME=PATH [--trace ...] [--log NAME=PATH ...]

For every 'control-flow' warning in docs/milestones/M02/generation_metadata.json (a jr / jalr whose targets the
generator could not list; the generator reports each such site twice, so 1,196 events are 598 sites): the owner
function, the address and what the generator emitted there (the
`dispatchGuestBranch` call with the computed target and its GuestBranchKind). Each --trace is a Switch game log of a
run with VCS_NX_BRANCH_TRACE=1 and PS2X_FAST_DISPATCH=0 (tests/switch/m04-branches.cfg): its `[branch-trace] site=`
lines (src/switch/branch_trace.cpp) say which indirect sites ran, how often, how many distinct targets they reached and
how many of those were missing from the function table. Each --log (and each trace) is also searched for the runtime's
`[guest-branch:missing-target]` and `[guest-branch:unlisted-resume]` reports, which cover every branch kind: their
absence on a route means the generator's entry list held every target and resume point that route reached.
The output holds owners, addresses, kinds and counts only (no instruction words or targets).
"""
import argparse
import collections
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
DISPATCH = re.compile(r"dispatchGuestBranch\(rdram, ctx, \w+, 0x([0-9A-Fa-f]+)u, 0x[0-9A-Fa-f]+u, "
                      r"PS2Runtime::GuestBranchKind::(\w+)")
TRACE = re.compile(r"^\[branch-trace\] site=0x([0-9a-f]+) kind=(\w+) count=(\d+) targets=(\d+) missing=(\d+)$", re.M)
TRACE_TOTAL = re.compile(r"^\[branch-trace\] indirect sites=(\d+) dispatches=(\d+) missing=(\d+)$", re.M)
MISSING = re.compile(r"\[guest-branch:missing-target\]")
UNLISTED = re.compile(r"\[guest-branch:unlisted-resume\]")


def parse_pairs(items):
    pairs = []
    for item in items:
        name, _, path = item.partition("=")
        if not name or not path:
            raise SystemExit(f"expected NAME=PATH, got {item!r}")
        pairs.append((name, Path(path).read_text(encoding="utf-8", errors="replace")))
    return pairs


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("generated", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--trace", action="append", default=[], required=True)
    parser.add_argument("--log", action="append", default=[])
    args = parser.parse_args()
    meta = json.loads((ROOT / "docs/milestones/M02/generation_metadata.json").read_text())
    events = [e for e in meta["diagnostic_events"] if e["category"] == "control-flow"]
    # M02's generator reports every such site twice (1,196 events, 598 sites): classify each (owner, address) once
    unique = {}
    for e in events:
        unique.setdefault((e["function"], e["address"]), e)

    emitted = collections.defaultdict(set)
    for path in sorted(args.generated.glob("*.cpp")):
        for match in DISPATCH.finditer(path.read_text(encoding="utf-8", errors="replace")):
            emitted[int(match.group(1), 16)].add(match.group(2))

    traces = {}
    for name, text in parse_pairs(args.trace):
        totals = TRACE_TOTAL.search(text)
        if not totals:
            raise SystemExit(f"{name}: no [branch-trace] summary (run without VCS_NX_BRANCH_TRACE=1?)")
        sites = {int(m.group(1), 16): {"kind": m.group(2), "count": int(m.group(3)), "targets": int(m.group(4)),
                                       "missing": int(m.group(5))} for m in TRACE.finditer(text)}
        if len(sites) != int(totals.group(1)):
            raise SystemExit(f"{name}: {len(sites)} site lines, summary says {totals.group(1)}")
        traces[name] = {"sites": sites, "summary": {"sites": int(totals.group(1)), "dispatches": int(totals.group(2)),
                                                      "missing": int(totals.group(3))}}

    reports = {}
    for name, text in parse_pairs(args.trace) + parse_pairs(args.log):
        reports[name] = {"missing_target": len(MISSING.findall(text)), "unlisted_resume": len(UNLISTED.findall(text))}

    warned = set()
    rows, kinds = [], collections.Counter()
    for e in unique.values():
        address = int(e["address"], 16)
        warned.add(address)
        kind = sorted(emitted.get(address, {"(no dispatch found)"}))
        kinds[" / ".join(kind)] += 1
        ran = {}
        for name, trace in traces.items():
            site = trace["sites"].get(address)
            ran[name] = ({"count": site["count"], "targets": site["targets"], "missing": site["missing"]}
                         if site else None)
        rows.append({"owner": e["function"], "address": e["address"], "emitted": kind, "ran": ran})

    owners = collections.Counter(r["owner"] for r in rows)
    per_trace = {}
    for name, trace in traces.items():
        hit = [r for r in rows if r["ran"][name]]
        per_trace[name] = {
            "warned_sites_ran": len(hit),
            "warned_sites_not_reached": len(rows) - len(hit),
            "warned_owners_ran": len({r["owner"] for r in hit}),
            "warned_sites_with_missing_targets": sum(1 for r in hit if r["ran"][name]["missing"]),
            "other_indirect_sites_ran": sum(1 for a in trace["sites"] if a not in warned),
            "trace_summary": trace["summary"],
        }
    summary = {
        "events": len(events),
        "sites": len(rows),
        "owners": len(owners),
        "emitted_kinds": dict(kinds),
        "without_dispatch": sum(1 for r in rows if r["emitted"] == ["(no dispatch found)"]),
        "per_trace": per_trace,
        "runtime_reports": reports,
        "reading": ("A warned site compiles to dispatchGuestBranch with the computed target, which looks it up in the "
                    "function table. 'ran' is from the trace: sites that ran reached only registered targets when their "
                    "'missing' is 0; sites not reached did not run on this route (the trace sees every indirect "
                    "dispatch with PS2X_FAST_DISPATCH=0). No missing-target or unlisted-resume report in a log means "
                    "the entry list held every branch target and resume point of that run."),
    }
    args.out.write_text(json.dumps({"summary": summary, "owners": dict(owners.most_common()), "sites": rows},
                                   indent=1) + "\n")
    print(json.dumps(summary, indent=2))
    bad = summary["without_dispatch"] or any(t["warned_sites_with_missing_targets"] for t in per_trace.values()) or \
        any(r["missing_target"] or r["unlisted_resume"] for r in reports.values())
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
