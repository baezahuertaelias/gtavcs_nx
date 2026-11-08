"""Focused tests for the user's all-tasks-before-review invariant."""
import tempfile
import subprocess
import unittest
from pathlib import Path
from milestone_gate import validate_tasks, validate_review
from unittest.mock import patch


class GateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "proof.md").write_text("Observed proof")
        self.git("init", "--initial-branch=main")
        self.git("add", "proof.md")
        self.git("commit", "-m", "test evidence")
        self.evidence = {"M00-01": [{"path": "proof.md", "result": "verified"}]}

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), "-c", "user.name=Gate Test",
            "-c", "user.email=gate-test@example.invalid", "-c", "commit.gpgsign=false",
            *args], text=True, stderr=subprocess.DEVNULL).strip()

    def review(self):
        return {"milestone": "M00", "verdict": "APPROVE", "reviewer_id": "separate-agent",
                "independent": True, "unresolved_findings": [], "candidate_commit": "a" * 40,
                "checks": ["checked task evidence"], "limitations": ["source-only milestone"],
                "observed_test_results": ["governance tests passed"]}

    def test_unfinished_task_blocks_review_even_with_evidence(self):
        self.assertIn("M00-01 is incomplete", validate_tasks("- [ ] M00-01 task", "M00", self.evidence, self.root))

    def test_checked_task_needs_evidence(self):
        self.assertTrue(validate_tasks("- [x] M00-01 task", "M00", {}, self.root))

    def test_missing_or_traversing_evidence_blocks(self):
        for path in ("missing.md", "../outside.md"):
            evidence = {"M00-01": [{"path": path, "result": "claimed"}]}
            self.assertTrue(validate_tasks("- [x] M00-01 task", "M00", evidence, self.root))

    def test_duplicate_and_foreign_task_keys_block(self):
        self.assertTrue(validate_tasks("- [x] M00-01 task\n- [x] M00-01 duplicate", "M00", self.evidence, self.root))
        extra = dict(self.evidence, **{"M00-02": self.evidence["M00-01"]})
        self.assertTrue(validate_tasks("- [x] M00-01 task", "M00", extra, self.root))

    def test_complete_evidenced_task_is_eligible(self):
        self.assertEqual([], validate_tasks("- [x] M00-01 task", "M00", self.evidence, self.root))

    def test_git_internal_untracked_and_ignored_reports_rejected(self):
        (self.root / ".gitignore").write_text("ignored.md\n")
        (self.root / "ignored.md").write_text("ignored")
        (self.root / "untracked.md").write_text("untracked")
        for path in (".git/config", "ignored.md", "untracked.md"):
            evidence = {"M00-01": [{"path": path, "result": "claimed"}]}
            self.assertTrue(validate_tasks("- [x] M00-01 task", "M00", evidence, self.root))

    def test_committed_evidence_mutation_invalidates_record(self):
        (self.root / "proof.md").write_text("Changed after candidate review")
        errors = validate_tasks("- [x] M00-01 task", "M00", self.evidence, self.root)
        self.assertTrue(any("differ from the committed candidate" in x for x in errors))

    def test_git_symlink_mode_rejected_without_host_symlink_privileges(self):
        target = self.git("hash-object", "-w", "proof.md")
        self.git("update-index", "--add", "--cacheinfo", f"120000,{target},link.md")
        self.git("commit", "-m", "test symlink entry")
        (self.root / "link.md").write_text("Observed proof")
        evidence = {"M00-01": [{"path": "link.md", "result": "claimed"}]}
        self.assertTrue(validate_tasks("- [x] M00-01 task", "M00", evidence, self.root))

    def test_malformed_evidence_has_clear_errors(self):
        for document in ([], None, "wrong", {"M00-01": [None]},
                         {"M00-01": [{"path": "proof.md", "result": {"claimed": True}}]},
                         {"M00-01": [{"path": "proof.md", "result": "   "}]}):
            self.assertTrue(validate_tasks("- [x] M00-01 task", "M00", document, self.root))

    def test_malformed_review_has_clear_errors(self):
        cases = [None, [], "wrong"]
        for key, value in (("reviewer_id", {}), ("reviewer_id", "  "), ("checks", [None]),
                           ("checks", [" "]), ("limitations", None), ("observed_test_results", []),
                           ("candidate_commit", {}), ("independent", "true")):
            record = self.review()
            record[key] = value
            cases.append(record)
        for record in cases:
            with patch("milestone_gate.subprocess.run") as run, patch("milestone_gate.git", return_value=""):
                run.return_value.returncode = 0
                self.assertTrue(validate_review("M00", record))

    def test_source_change_invalidates_approval(self):
        review = self.review()
        with patch("milestone_gate.subprocess.run") as run, patch("milestone_gate.git", return_value="src/main.c"):
            run.return_value.returncode = 0
            self.assertTrue(any("changed after review" in x for x in validate_review("M00", review)))

    def test_only_review_record_may_follow_candidate(self):
        review = self.review()
        with patch("milestone_gate.subprocess.run") as run, patch("milestone_gate.git", return_value="docs/milestones/M00/review.json"):
            run.return_value.returncode = 0
            self.assertEqual([], validate_review("M00", review))

    def test_nonancestor_review_is_rejected(self):
        review = self.review()
        with patch("milestone_gate.subprocess.run") as run:
            run.return_value.returncode = 1
            self.assertTrue(any("not an ancestor" in x for x in validate_review("M00", review)))


if __name__ == "__main__":
    unittest.main()
