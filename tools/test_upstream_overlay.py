"""Exercise actual Git patch/index round trips and refusal to overwrite edits."""
from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from upstream_overlay import prepare


class OverlayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.seed = self.root / "upstream"
        self.seed.mkdir()
        self.git("init", "-q")
        self.git("config", "user.name", "Overlay fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "core.autocrlf", "false")
        (self.seed / "value.txt").write_text("original\n")
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        base = self.git("rev-parse", "HEAD").decode().strip()
        (self.seed / "value.txt").write_text("ported\n")
        (self.seed / "new.txt").write_text("new overlay file\n")
        self.git("add", ".")
        patch = self.git("diff", "--cached", "--binary")
        self.git("restore", "--staged", "--worktree", ".")
        self.patches = self.root / "patches/upstream"
        self.patches.mkdir(parents=True)
        (self.patches / "0001-fixture.patch").write_bytes(patch)
        self.lock = {"base_commit": base, "patches": [{"path": "0001-fixture.patch",
                     "sha256": hashlib.sha256(patch).hexdigest()}]}
        self.write_lock()
        self.destination = self.root / "build/target"

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.seed), *args], stderr=subprocess.STDOUT)

    def write_lock(self):
        (self.patches / "series.json").write_text(json.dumps(self.lock))

    def apply(self, reverse=False):
        with redirect_stdout(io.StringIO()):
            prepare(self.destination, reverse, self.root)

    def test_roundtrip_new_file_and_idempotence(self):
        self.apply()
        self.assertEqual((self.destination / "value.txt").read_text(), "ported\n")
        self.assertTrue((self.destination / "new.txt").exists())
        self.apply()
        self.apply(reverse=True)
        self.assertEqual((self.destination / "value.txt").read_text(), "original\n")
        self.assertFalse((self.destination / "new.txt").exists())
        self.assertEqual(self.git("status", "--porcelain"), b"")

    def test_unowned_edit_is_preserved_and_rejected(self):
        self.apply()
        (self.destination / "value.txt").write_text("user edit\n")
        with self.assertRaisesRegex(RuntimeError, "changed outside"):
            self.apply(reverse=True)
        self.assertEqual((self.destination / "value.txt").read_text(), "user edit\n")

    def test_bad_patch_does_not_change_tree(self):
        self.lock["patches"][0]["sha256"] = "0" * 64
        self.write_lock()
        with self.assertRaisesRegex(RuntimeError, "checksum mismatch"):
            self.apply()
        self.assertFalse(self.destination.exists())

    def test_destination_escape_is_rejected(self):
        self.destination = self.root / "outside"
        with self.assertRaisesRegex(RuntimeError, "strictly below"):
            self.apply()


if __name__ == "__main__":
    unittest.main()
