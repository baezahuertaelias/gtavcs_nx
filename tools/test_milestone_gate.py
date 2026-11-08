"""Focused tests for the user's all-tasks-before-review invariant."""
import tempfile
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
        self.evidence = {"M00-01": [{"path": "proof.md", "result": "verified"}]}

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

    def test_source_change_invalidates_approval(self):
        review = {"milestone": "M00", "verdict": "APPROVE", "reviewer_id": "separate-agent",
                  "independent": True, "unresolved_findings": [], "candidate_commit": "a" * 40,
                  "checks": ["checked task evidence"]}
        with patch("milestone_gate.subprocess.run") as run, patch("milestone_gate.git", return_value="src/main.c"):
            run.return_value.returncode = 0
            self.assertTrue(any("changed after review" in x for x in validate_review("M00", review)))

    def test_only_review_record_may_follow_candidate(self):
        review = {"milestone": "M00", "verdict": "APPROVE", "reviewer_id": "separate-agent",
                  "independent": True, "unresolved_findings": [], "candidate_commit": "a" * 40,
                  "checks": ["checked task evidence"]}
        with patch("milestone_gate.subprocess.run") as run, patch("milestone_gate.git", return_value="docs/milestones/M00/review.json"):
            run.return_value.returncode = 0
            self.assertEqual([], validate_review("M00", review))

    def test_nonancestor_review_is_rejected(self):
        review = {"milestone": "M00", "verdict": "APPROVE", "reviewer_id": "separate-agent",
                  "independent": True, "unresolved_findings": [], "candidate_commit": "a" * 40,
                  "checks": ["checked task evidence"]}
        with patch("milestone_gate.subprocess.run") as run:
            run.return_value.returncode = 1
            self.assertTrue(any("not an ancestor" in x for x in validate_review("M00", review)))


if __name__ == "__main__":
    unittest.main()
