"""Compare two RHI conformance result folders (src/rhi_tests: index.tsv + .bin readbacks + checks.tsv).

    python tools/rhi_compare.py REFERENCE CANDIDATE [--report JSON]

REFERENCE is normally upstream's Vulkan backend (the desktop reference), CANDIDATE the port's OpenGL backend (host Mesa
or the Switch). Every image the reference has must exist in the candidate with the same kind and size, and agree
within the tolerance the scene declared:
    exact                 every byte equal
    unorm:N[,pixels:K]    every 8-bit channel within N; with pixels:K, up to K pixels may exceed N
    float:E               every float within E (r32f / d32f)
    bands:R,mean:M,stdev:S   filtering whose per-pixel result is the implementation's (trilinear / anisotropic LOD
                          and footprint): for every band of R rows and each colour channel, the mean within M and
                          the standard deviation within S of the reference's. A wrong mip selection (an aliased
                          checker where the reference has grey) moves a band's deviation by tens: it fails
    impl:<why>            implementation-defined (filter footprint, sample positions): measured and reported, not
                          failed, unless more than half the pixels differ by more than 32 (the scene is broken)
Both folders' self-checks must pass. Exit 0 when everything agrees. Standard library only.
"""
import argparse
import json
from pathlib import Path
import statistics
import struct
import sys


def load_index(folder):
    rows = {}
    for line in (folder / "index.tsv").read_text().splitlines():
        if not line.strip():
            continue
        name, kind, w, h, tolerance, file = line.split("\t")
        rows[name] = {"kind": kind, "w": int(w), "h": int(h), "tolerance": tolerance, "file": folder / file}
    return rows


def load_checks(folder):
    out = []
    path = folder / "checks.tsv"
    if path.exists():
        for line in path.read_text().splitlines():
            parts = line.split("\t")
            if len(parts) >= 2:
                out.append((parts[0], parts[1], parts[2] if len(parts) > 2 else ""))
    return out


def compare(ref, cand):
    a = ref["file"].read_bytes()
    b = cand["file"].read_bytes()
    result = {"bytes_equal": a == b}
    if len(a) != len(b):
        return dict(result, ok=False, why=f"size {len(a)} != {len(b)}")
    tol = ref["tolerance"]
    pixels = ref["w"] * ref["h"]
    if ref["kind"] == "rgba8":
        worst = 0
        over = {}
        for i in range(pixels):
            d = max(abs(a[4 * i + c] - b[4 * i + c]) for c in range(4))
            worst = max(worst, d)
            if d:
                over[i] = d
        result.update(max_channel_diff=worst, differing_pixels=len(over), pixels=pixels)
        if tol == "exact":
            ok = not over
        elif tol.startswith("unorm:"):
            parts = dict(p.split(":") for p in tol.split(","))
            n = int(parts["unorm"])
            k = int(parts.get("pixels", 0))
            ok = sum(1 for d in over.values() if d > n) <= k
        elif tol.startswith("bands:"):
            parts = dict(p.split(":") for p in tol.split(","))
            rows, mean_tol, dev_tol = int(parts["bands"]), float(parts["mean"]), float(parts["stdev"])
            worst_mean = worst_dev = 0.0
            for band in range(0, ref["h"], rows):
                for c in range(3):
                    va = [a[4 * (y * ref["w"] + x) + c] for y in range(band, min(band + rows, ref["h"])) for x in range(ref["w"])]
                    vb = [b[4 * (y * ref["w"] + x) + c] for y in range(band, min(band + rows, ref["h"])) for x in range(ref["w"])]
                    worst_mean = max(worst_mean, abs(statistics.fmean(va) - statistics.fmean(vb)))
                    worst_dev = max(worst_dev, abs(statistics.pstdev(va) - statistics.pstdev(vb)))
            result.update(band_mean_diff=round(worst_mean, 3), band_stdev_diff=round(worst_dev, 3))
            ok = worst_mean <= mean_tol and worst_dev <= dev_tol
        elif tol.startswith("impl:"):
            ok = sum(1 for d in over.values() if d > 32) <= pixels // 2
        else:
            ok = False
        if over:
            first = min(over)
            result["first_diff"] = {"x": first % ref["w"], "y": first // ref["w"],
                                    "reference": a[4 * first:4 * first + 4].hex(), "candidate": b[4 * first:4 * first + 4].hex()}
    else:
        fa = struct.unpack(f"<{pixels}f", a)
        fb = struct.unpack(f"<{pixels}f", b)
        diffs = [abs(x - y) for x, y in zip(fa, fb)]
        worst = max(diffs) if diffs else 0.0
        result.update(max_abs_diff=worst, differing_pixels=sum(1 for d in diffs if d), pixels=pixels)
        if tol == "exact":
            ok = a == b
        elif tol.startswith("float:"):
            ok = worst <= float(tol[6:])
        elif tol.startswith("impl:"):
            ok = True
        else:
            ok = False
    result["ok"] = ok
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("reference", type=Path)
    ap.add_argument("candidate", type=Path)
    ap.add_argument("--report", type=Path)
    a = ap.parse_args()
    ref = load_index(a.reference)
    cand = load_index(a.candidate)
    report = {"reference": str(a.reference), "candidate": str(a.candidate), "images": {}, "checks": {}}
    failures = []
    for name, r in sorted(ref.items()):
        c = cand.get(name)
        if not c:
            failures.append(f"{name}: missing in candidate")
            report["images"][name] = {"ok": False, "why": "missing"}
            continue
        if (c["kind"], c["w"], c["h"]) != (r["kind"], r["w"], r["h"]):
            failures.append(f"{name}: kind/size differ")
            report["images"][name] = {"ok": False, "why": "kind/size"}
            continue
        res = compare(r, c)
        res["tolerance"] = r["tolerance"]
        report["images"][name] = res
        if not res["ok"]:
            failures.append(f"{name}: {r['tolerance']} {json.dumps({k: v for k, v in res.items() if k != 'ok'})}")
    for name in sorted(set(cand) - set(ref)):
        failures.append(f"{name}: only in candidate")
    for side, folder in (("reference", a.reference), ("candidate", a.candidate)):
        checks = load_checks(folder)
        bad = [c for c in checks if c[1] != "PASS"]
        report["checks"][side] = {"total": len(checks), "failed": [f"{n} {d}" for n, _, d in bad]}
        failures += [f"{side} self-check {n}: {d}" for n, _, d in bad]
    exact = sum(1 for v in report["images"].values() if v.get("bytes_equal"))
    report["summary"] = {"images": len(ref), "bytes_equal": exact, "failures": len(failures)}
    report["failures"] = failures
    if a.report:
        a.report.write_text(json.dumps(report, indent=1) + "\n")
    for f in failures:
        print("FAIL " + f)
    print(f"{len(ref)} images, {exact} byte-identical, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
