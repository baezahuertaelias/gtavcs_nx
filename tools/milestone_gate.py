"""Fail closed before requesting review or merging a milestone.

This validates recorded task/evidence/review structure. It does NOT execute the
task's behavioral tests or prove the independent reviewer actually ran them.
"""
import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def tasks_for(text, milestone):
    return re.findall(r"^- \[([ xX])\] (" + re.escape(milestone) + r"-\d{2})\b(.+)$", text, re.M)


def nonblank(value):
    return isinstance(value, str) and bool(value.strip())


def committed_files(root):
    """Use HEAD, not the index or working tree, to bind evidence to the candidate."""
    result = subprocess.run(["git", "-C", str(root), "ls-tree", "-rz", "HEAD"], capture_output=True)
    if result.returncode: return None
    files = {}
    for entry in result.stdout.split(b"\0"):
        if not entry: continue
        meta, name = entry.split(b"\t", 1)
        mode, kind, oid = meta.decode("ascii").split()
        if mode in ("100644", "100755") and kind == "blob":
            files[name.decode("utf-8")] = oid
    return files


def evidence_error(root, name, files):
    if not nonblank(name) or "\\" in name:
        return "evidence path must be nonblank repository-relative POSIX text"
    parts = name.split("/")
    if PurePosixPath(name).is_absolute() or any(p in ("", ".", "..") or p.lower() == ".git" or ":" in p for p in parts):
        return "unsafe evidence path"
    if files is None or name not in files:
        return "evidence must be a regular file committed in HEAD (not ignored, untracked, Git-internal or a symlink)"
    path = root
    for part in parts:
        path = path / part
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            return "evidence links/junctions are forbidden"
    if not path.resolve().is_relative_to(root.resolve()) or not path.is_file():
        return "evidence must exist inside the repository"
    digest = subprocess.run(["git", "-C", str(root), "hash-object", "--path=" + name, str(path.resolve())], capture_output=True, text=True)
    if digest.returncode or digest.stdout.strip() != files[name]:
        return "evidence contents differ from the committed candidate"
    return None


def validate_tasks(text, milestone, evidence, root):
    errors = []
    tasks = tasks_for(text, milestone)
    if not tasks: return [f"No tasks for {milestone}"]
    ids = [task[1] for task in tasks]
    if len(ids) != len(set(ids)): errors.append("Duplicate task IDs")
    if not isinstance(evidence, dict): return errors + ["Evidence document must be a JSON object"]
    if set(evidence) != set(ids): errors.append("Evidence keys must exactly match milestone task IDs")
    files = committed_files(root)
    for checked, task_id, _ in tasks:
        if checked.lower() != "x": errors.append(f"{task_id} is incomplete")
        records = evidence.get(task_id)
        if not isinstance(records, list) or not records:
            errors.append(f"{task_id} has no evidence records")
            continue
        for record in records:
            if not isinstance(record, dict) or not nonblank(record.get("path")) or not nonblank(record.get("result")):
                errors.append(f"{task_id} needs nonblank text evidence path and observed result")
                continue
            error = evidence_error(root, record["path"], files)
            if error: errors.append(f"{task_id}: {error}")
    return errors


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def validate_review(milestone, review):
    errors = []
    if not isinstance(review, dict): return ["Review document must be a JSON object"]
    if review.get("milestone") != milestone: errors.append("Review milestone mismatch")
    if review.get("verdict") != "APPROVE": errors.append("Independent review has not approved")
    if not nonblank(review.get("reviewer_id")) or review.get("independent") is not True:
        errors.append("Independent reviewer identity/attestation missing")
    if not isinstance(review.get("checks"), list) or not review["checks"] or not all(nonblank(x) for x in review["checks"]):
        errors.append("Independent review checks must be a nonempty list of nonblank text")
    if not isinstance(review.get("limitations"), list) or not all(nonblank(x) for x in review["limitations"]):
        errors.append("Review limitations must be a list of nonblank text (empty only when explicitly none)")
    if not isinstance(review.get("observed_test_results"), list) or not review["observed_test_results"] or not all(nonblank(x) for x in review["observed_test_results"]):
        errors.append("Observed test results must be a nonempty list of nonblank text")
    if review.get("unresolved_findings") != []: errors.append("Unresolved review findings")
    candidate = review.get("candidate_commit", "")
    if not isinstance(candidate, str) or not re.fullmatch(r"[0-9a-f]{40}", candidate): return errors + ["Invalid reviewed commit"]
    if subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", candidate, "HEAD"], capture_output=True).returncode:
        return errors + ["Reviewed commit is not an ancestor of HEAD"]
    # Only the recorded review may be added after the reviewed candidate. Any
    # implementation/task/evidence change invalidates approval and needs review.
    allowed = {f"docs/milestones/{milestone}/review.json"}
    changed = set(git("diff", "--name-only", candidate, "HEAD").splitlines())
    if changed - allowed: errors.append("Candidate changed after review: " + ", ".join(sorted(changed - allowed)))
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("milestone")
    parser.add_argument("--require-review", action="store_true")
    parser.add_argument("--branch", help="Actual PR head branch for detached CI checkouts")
    args = parser.parse_args()
    if not re.fullmatch(r"M\d{2}", args.milestone): parser.error("Milestone must be M00, M01, ...")
    plan = (ROOT / "docs/MILESTONES.md").read_text(encoding="utf-8")
    branch_match = re.search(r"^Branch: `([^`]+)` <!-- " + args.milestone + r" -->$", plan, re.M)
    branch = args.branch or git("branch", "--show-current")
    errors = []
    if not branch_match or branch != branch_match[1]: errors.append("Wrong milestone branch")
    folder = ROOT / "docs/milestones" / args.milestone
    try:
        evidence = json.loads((folder / "evidence.json").read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError) as exc:
        errors.append(f"Missing/invalid task evidence: {exc}")
        evidence = {}
    errors.extend(validate_tasks(plan, args.milestone, evidence, ROOT))
    if git("status", "--porcelain", "--untracked-files=normal"):
        errors.append("Working tree must be clean before review/merge")
    if args.require_review:
        try:
            review = json.loads((folder / "review.json").read_text(encoding="utf-8"))
            errors.extend(validate_review(args.milestone, review))
        except (FileNotFoundError, json.JSONDecodeError) as exc:
            errors.append(f"Missing/invalid independent review: {exc}")
    if errors:
        print("BLOCKED\n" + "\n".join(errors))
        return 1
    print(f"PASS {args.milestone}: " + ("recorded independent approval valid" if args.require_review else "eligible to request independent review"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
