"""Compare two run results files (the Switch core's nxrun-results.txt, tools/m04_reference.py's reference-results.txt).

    python tools/m04_compare.py <reference-results> <candidate-results> [--json out.json]

Reads the scheduled commands' answers and reports, per schedule time: the area (CGame::currArea, peek 0x489f7c), the
player's position, the game frame count and the camera mode line of gameinfo, plus the first time each run was
outdoors (area 0) with a follow camera, and whether each run stopped in an orderly way. Timing differs between hosts
(load speed): the comparison is of the states reached and their order, not of frame-exact positions.
"""
import argparse
import json
import re
import sys


def load(path):
    rows = []
    for line in open(path, encoding="utf-8", errors="replace"):
        kind, _, rest = line.strip().partition(" ")
        fields = dict(re.findall(r'(\w+)=("(?:[^"\\]|\\.)*"|\S+)', rest))
        fields = {k: v[1:-1].replace('\\"', '"') if v.startswith('"') else v for k, v in fields.items()}
        rows.append((kind, fields))
    return rows


def states(rows):
    out = {}
    for kind, f in rows:
        if kind != "command" or f.get("status") != "ok":
            continue
        at = round(float(f["at"]))
        entry = out.setdefault(at, {})
        reply, command = f["reply"], f["command"]
        if command.startswith("peek 0x489f7c"):
            entry["area"] = int.from_bytes(bytes.fromhex(reply.split()[1]), "little")
        elif command == "player":
            w = reply.split()
            entry["player"] = [round(float(x), 1) for x in w[2:5]]
        elif command == "frames":
            entry["frames"] = int(reply.split()[1])
        elif command == "gameinfo":
            camera = re.search(r"\[CAMERA\s+mode (\d+) ([^\]|]*)", reply)
            entry["camera"] = camera.group(1) + " " + camera.group(2).strip() if camera else "?"
    return out


def first(states_, test):
    for at in sorted(states_):
        if test(states_[at]):
            return at
    return None


def summary(rows):
    s = states(rows)
    stop = [f for k, f in rows if k == "stop"]
    audit = [f for k, f in rows if k == "audit"]
    return {
        "first_outdoors": first(s, lambda e: e.get("area") == 0),
        "first_follow_camera": first(s, lambda e: e.get("camera", "").startswith("4 ")),
        "last_frames": max((e["frames"] for e in s.values() if "frames" in e), default=0),
        "stop": stop[-1].get("reason") if stop else None,
        "audit": audit[-1] if audit else None,
        "errors": sum(1 for k, f in rows if k == "command" and f.get("status") != "ok"),
        "states": s,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference")
    parser.add_argument("candidate")
    parser.add_argument("--json")
    args = parser.parse_args()
    ref, cand = summary(load(args.reference)), summary(load(args.candidate))
    print(f"{'':24}{'reference':>16}{'candidate':>16}")
    for key in ("first_outdoors", "first_follow_camera", "last_frames", "stop", "errors"):
        print(f"{key:24}{str(ref[key]):>16}{str(cand[key]):>16}")
    print("time  reference(area player frames)            candidate(area player frames)")
    for at in sorted(set(ref["states"]) | set(cand["states"])):
        r, c = ref["states"].get(at, {}), cand["states"].get(at, {})
        if "area" in r or "area" in c:
            print(f"{at:4}  {r.get('area')} {r.get('player')} {r.get('frames')}".ljust(46) +
                  f"{c.get('area')} {c.get('player')} {c.get('frames')}")
    same_route = (ref["first_outdoors"] is not None and cand["first_outdoors"] is not None and
                  ref["first_follow_camera"] is not None and cand["first_follow_camera"] is not None)
    print(f"both reached outdoors with a follow camera: {same_route}")
    if args.json:
        with open(args.json, "w") as f:
            json.dump({"reference": {k: v for k, v in ref.items() if k != "states"},
                       "candidate": {k: v for k, v in cand.items() if k != "states"}, "same_route": same_route}, f, indent=2)
    return 0 if same_route and cand["stop"] in ("seconds", "runtime") else 1


if __name__ == "__main__":
    sys.exit(main())
