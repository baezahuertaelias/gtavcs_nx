"""Tests of tools/rhi_compare.py's tolerances on synthetic readbacks."""
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def folder(base, name, images, checks=()):
    d = base / name
    d.mkdir()
    rows = []
    for img_name, kind, w, h, tol, data in images:
        file = img_name.replace("/", ".") + ".bin"
        (d / file).write_bytes(data)
        rows.append(f"{img_name}\t{kind}\t{w}\t{h}\t{tol}\t{file}")
    (d / "index.tsv").write_text("\n".join(rows) + "\n")
    (d / "checks.tsv").write_text("".join(f"{n}\t{r}\t\n" for n, r in checks))
    return d


class RhiCompareTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def run_compare(self, ref, cand):
        return subprocess.run([sys.executable, ROOT / "tools/rhi_compare.py", ref, cand], capture_output=True, text=True)

    def test_tolerances(self):
        a = bytes([10, 20, 30, 40] * 4)
        b = bytes([11, 20, 30, 40] * 4)
        far = bytes([10, 20, 30, 40] * 3 + [200, 20, 30, 40])
        depth = struct.pack("<4f", 0.25, 0.5, 0.75, 1.0)
        depth2 = struct.pack("<4f", 0.25, 0.5, 0.75, 1.0 - 1e-7)
        ref = folder(self.tmp, "ref", [("s/exact", "rgba8", 2, 2, "exact", a), ("s/unorm", "rgba8", 2, 2, "unorm:1", a),
                                       ("s/pixels", "rgba8", 2, 2, "unorm:1,pixels:1", a), ("s/depth", "d32f", 2, 2, "float:1e-6", depth)])
        good = folder(self.tmp, "good", [("s/exact", "rgba8", 2, 2, "exact", a), ("s/unorm", "rgba8", 2, 2, "unorm:1", b),
                                         ("s/pixels", "rgba8", 2, 2, "unorm:1,pixels:1", far), ("s/depth", "d32f", 2, 2, "float:1e-6", depth2)])
        self.assertEqual(self.run_compare(ref, good).returncode, 0)
        bad = folder(self.tmp, "bad", [("s/exact", "rgba8", 2, 2, "exact", b), ("s/unorm", "rgba8", 2, 2, "unorm:1", far),
                                       ("s/pixels", "rgba8", 2, 2, "unorm:1,pixels:1", bytes([200, 20, 30, 40] * 4)),
                                       ("s/depth", "d32f", 2, 2, "float:1e-6", struct.pack("<4f", 0, 0, 0, 0))])
        result = self.run_compare(ref, bad)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout.count("FAIL "), 4)

    def test_missing_image_and_failed_check(self):
        a = bytes(16)
        ref = folder(self.tmp, "ref", [("s/x", "rgba8", 2, 2, "exact", a), ("s/y", "rgba8", 2, 2, "exact", a)])
        cand = folder(self.tmp, "cand", [("s/x", "rgba8", 2, 2, "exact", a)], [("s/check", "FAIL")])
        result = self.run_compare(ref, cand)
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing in candidate", result.stdout)
        self.assertIn("self-check", result.stdout)


if __name__ == "__main__":
    unittest.main()
