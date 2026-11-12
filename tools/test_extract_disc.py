"""Synthetic-disc tests for the extractor; no game data is involved."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_disc  # noqa: E402

SECTOR = extract_disc.SECTOR
ROOT_LBA, SUB_LBA, FILE_A, FILE_B = 20, 21, 30, 40


def record(lba, size, name, directory):
    raw = name.encode("ascii")
    length = 33 + len(raw) + (0 if len(raw) % 2 else 1)
    data = bytearray(length)
    data[0] = length
    struct.pack_into("<I", data, 2, lba)
    struct.pack_into("<I", data, 10, size)
    data[25] = 2 if directory else 0
    data[32] = len(raw)
    data[33:33 + len(raw)] = raw
    return bytes(data)


def build_iso(path, tail=b"", name_b="B.BIN;1", overlap=False, root_extra=b"", root_first=False):
    image = bytearray(60 * SECTOR)
    pvd = 16 * SECTOR
    image[pvd] = 1
    image[pvd + 1:pvd + 6] = b"CD001"
    image[pvd + 40:pvd + 72] = b"TESTDISC".ljust(32)
    image[pvd + 156:pvd + 190] = record(ROOT_LBA, SECTOR, "\x00", True)[:34].ljust(34, b"\0")
    root = record(ROOT_LBA, SECTOR, "\x00", True) + record(ROOT_LBA, SECTOR, "\x01", True)
    if root_first:
        root += root_extra
    root += record(FILE_A, 3000, "A.BIN;1", False) + record(SUB_LBA, SECTOR, "SUB", True)
    if not root_first:
        root += root_extra
    image[ROOT_LBA * SECTOR:ROOT_LBA * SECTOR + len(root)] = root
    lba_b = FILE_A + 1 if overlap else FILE_B
    sub = record(SUB_LBA, SECTOR, "\x00", True) + record(ROOT_LBA, SECTOR, "\x01", True)
    sub += record(lba_b, SECTOR + 1, name_b, False)
    image[SUB_LBA * SECTOR:SUB_LBA * SECTOR + len(sub)] = sub
    image[FILE_A * SECTOR:FILE_A * SECTOR + 3000] = bytes((i * 7) & 0xff for i in range(3000))
    image[FILE_B * SECTOR:FILE_B * SECTOR + SECTOR + 1] = bytes((i * 11 + 1) & 0xff for i in range(SECTOR + 1))
    image[FILE_B * SECTOR + SECTOR + 1:FILE_B * SECTOR + SECTOR + 1 + len(tail)] = tail
    image[50 * SECTOR + 9] = 0x5a  # data in a gap between files
    Path(path).write_bytes(image)
    return bytes(image)


def accept(_):
    return {"ok": True, "sha1": "0" * 40, "message": "test"}


class ExtractDiscTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.iso = self.root / "disc.iso"
        self.dest = self.root / "gamefolder"

    def tearDown(self):
        self.temporary.cleanup()

    def test_round_trip_rebuilds_every_byte(self):
        original = build_iso(self.iso)
        report = extract_disc.extract(self.iso, self.dest, identity=accept)
        self.assertEqual((report["files"], report["directories"]), (2, 2))
        self.assertEqual((self.dest / "A.BIN").read_bytes(), original[FILE_A * SECTOR:FILE_A * SECTOR + 3000])
        self.assertTrue((self.dest / "SUB/B.BIN").is_file())
        self.assertEqual(extract_disc.verify(self.dest, self.iso), report["iso_sha256"])

    def test_manifest_keeps_original_lba_and_order(self):
        build_iso(self.iso)
        extract_disc.extract(self.iso, self.dest, identity=accept)
        lines = (self.dest / ".nxdisc/manifest.tsv").read_text(encoding="ascii").splitlines()
        records = [line.split("\t") for line in lines if line[:2] in ("D\t", "F\t")]
        self.assertEqual([(r[0], r[1], r[4]) for r in records], [("D", "20", ""), ("F", "30", "A.BIN"),
                                                                 ("D", "21", "SUB"), ("F", "40", "SUB/B.BIN")])
        self.assertTrue(any(line.startswith("R\t50\t1\t") for line in lines))

    def test_rejects_wrong_identity_before_writing(self):
        build_iso(self.iso)
        reject = lambda _: {"ok": False, "message": "other disc"}  # noqa: E731
        with self.assertRaisesRegex(extract_disc.DiscError, "other disc"):
            extract_disc.extract(self.iso, self.dest, identity=reject)
        self.assertFalse(self.dest.exists())

    def test_rejects_unexpected_padding(self):
        build_iso(self.iso, tail=b"\x01")
        with self.assertRaisesRegex(extract_disc.DiscError, "Non-zero padding"):
            extract_disc.extract(self.iso, self.dest, identity=accept)
        self.assertFalse(self.dest.exists())

    def test_empty_files_hold_no_sectors_in_any_order(self):
        # Zero-length files sharing A.BIN's first sector and inside its span, before and after it in the walk.
        for extra in (record(FILE_A, 0, "Y.BIN;1", False), record(FILE_A + 1, 0, "Z.BIN;1", False)):
            for first in (True, False):
                with self.subTest(extra=extra[33:40], first=first):
                    build_iso(self.iso, root_extra=extra, root_first=first)
                    dest = self.root / f"out-{len(list(self.root.iterdir()))}"
                    report = extract_disc.extract(self.iso, dest, identity=accept)
                    self.assertEqual(report["files"], 3)
                    self.assertEqual(extract_disc.verify(dest, self.iso), report["iso_sha256"])

    def test_rejects_overlapping_files(self):
        build_iso(self.iso, overlap=True)
        with self.assertRaisesRegex(extract_disc.DiscError, "Overlapping"):
            extract_disc.extract(self.iso, self.dest, identity=accept)

    def test_rejects_traversing_names(self):
        build_iso(self.iso, name_b="..;1")
        with self.assertRaisesRegex(extract_disc.DiscError, "Unsafe"):
            extract_disc.extract(self.iso, self.dest, identity=accept)

    def test_rejects_names_the_loader_refuses(self):
        # Lower case (which also covers names differing only by case) and control bytes:
        # FolderDisc::load refuses both, so the extractor must not write such a tree.
        for name in ("a.bin;1", "A\x01.BIN;1"):
            with self.subTest(name=name):
                build_iso(self.iso, root_extra=record(FILE_A, 3000, name, False))
                with self.assertRaisesRegex(extract_disc.DiscError, "Unsafe"):
                    extract_disc.extract(self.iso, self.dest, identity=accept)
                self.assertFalse(self.dest.exists())

    def test_safe_names_match_the_loader(self):
        for path in ("SYSTEM.CNF", "AUDIO/MUSIC_1.VB", "A B-~!.BIN"):
            extract_disc.require_safe(path)
        for path in ("a.bin", "DIR/x", "A\x1f", "A\tB", "A:B", "A;1", "../A", "A\\B"):
            with self.assertRaises(extract_disc.DiscError):
                extract_disc.require_safe(path)

    def test_detects_changed_file(self):
        build_iso(self.iso)
        extract_disc.extract(self.iso, self.dest, identity=accept)
        data = bytearray((self.dest / "A.BIN").read_bytes())
        data[10] ^= 1
        (self.dest / "A.BIN").write_bytes(data)
        with self.assertRaisesRegex(extract_disc.DiscError, "differs"):
            extract_disc.verify(self.dest)

    def test_keeps_unrelated_files_and_refuses_other_sizes(self):
        build_iso(self.iso)
        self.dest.mkdir()
        keep = self.dest / "userdata.txt"
        keep.write_text("mine", encoding="ascii")
        (self.dest / "A.BIN").write_bytes(b"x")
        with self.assertRaisesRegex(extract_disc.DiscError, "--replace"):
            extract_disc.extract(self.iso, self.dest, identity=accept)
        extract_disc.extract(self.iso, self.dest, replace=True, identity=accept)
        self.assertEqual(keep.read_text(encoding="ascii"), "mine")


if __name__ == "__main__":
    unittest.main()
