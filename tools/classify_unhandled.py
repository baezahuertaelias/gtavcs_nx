"""Classify M02's unhandled-instruction generation events against the generated code and run logs (M04-12).

    python tools/classify_unhandled.py <generated-ee-dir> <out.json> [--log NAME=PATH ...]

For every 'unhandled-instruction' event in docs/milestones/M02/generation_metadata.json: the owner function, the
address and what the generator emitted there (the exception kind of its `throw std::runtime_error("Unhandled ...")`).
Each --log is a run's game log; an executed site would end the game thread with "Error during program execution:
Unhandled ..." naming its address, so the tool counts such lines per log. Instruction words are not written out (they
are game code); the output holds owners, addresses, kinds and counts only.
"""
import argparse
import collections
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
THROW = re.compile(r'throw std::runtime_error\("Unhandled ([A-Za-z0-9 \-]+?): 0x[0-9A-Fa-f]+ at 0x([0-9A-Fa-f]+)')
RUNTIME = re.compile(r"Unhandled ([A-Za-z0-9 \-]+?): 0x[0-9A-Fa-f]+ at 0x([0-9A-Fa-f]+)")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("generated", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--log", action="append", default=[])
    args = parser.parse_args()
    meta = json.loads((ROOT / "docs/milestones/M02/generation_metadata.json").read_text())
    events = [e for e in meta["diagnostic_events"] if e["category"] == "unhandled-instruction"]
    emitted = {}
    for path in sorted(args.generated.glob("*.cpp")):
        for match in THROW.finditer(path.read_text(encoding="utf-8", errors="replace")):
            emitted.setdefault(int(match.group(2), 16), set()).add(match.group(1))
    logs = {}
    for item in args.log:
        name, _, path = item.partition("=")
        executed = collections.Counter()
        for match in RUNTIME.finditer(Path(path).read_text(encoding="utf-8", errors="replace")):
            executed[int(match.group(2), 16)] += 1
        logs[name] = executed
    rows, kinds = [], collections.Counter()
    for e in events:
        address = int(e["address"], 16)
        kind = sorted(emitted.get(address, {"(no throw found)"}))
        kinds[" / ".join(kind)] += 1
        rows.append({"owner": e["function"], "address": e["address"], "emitted": kind,
                     "executed": {name: counts.get(address, 0) for name, counts in logs.items()}})
    owners = collections.Counter(r["owner"] for r in rows)
    summary = {
        "events": len(rows),
        "owners": len(owners),
        "emitted_kinds": dict(kinds),
        "without_throw": sum(1 for r in rows if r["emitted"] == ["(no throw found)"]),
        "executed_per_log": {name: sum(counts.values()) for name, counts in logs.items()},
        "reading": ("Every site compiles to a throw that ends the game thread with its address in the log; a run "
                    "that reaches its end without such a line executed none of them on its route."),
    }
    args.out.write_text(json.dumps({"summary": summary, "owners": dict(owners.most_common()), "sites": rows},
                                   indent=1) + "\n")
    print(json.dumps(summary, indent=2))
    return 0 if summary["without_throw"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
