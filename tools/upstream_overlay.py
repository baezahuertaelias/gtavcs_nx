"""Prepare a private, pinned upstream checkout and reversibly apply root patches.

Never edits the registered upstream submodule. Destinations must be below build/.
Changes are staged in the disposable checkout, never committed to the submodule.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(directory, *args):
    result = subprocess.run(["git", "-C", str(directory), *map(str, args)],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace").strip())
    return result.stdout


def digest(data):
    return hashlib.sha256(data).hexdigest()


def patch_path(root, entry):
    directory = (root / "patches/upstream").resolve()
    path = directory / entry["path"]
    if path.is_symlink() or not path.resolve().is_relative_to(directory) or not path.is_file():
        raise RuntimeError("Invalid patch path")
    if digest(path.read_bytes()) != entry["sha256"]:
        raise RuntimeError(f"Patch checksum mismatch: {entry['path']}")
    return path


def prepare(destination, reverse=False, root=ROOT):
    root = root.resolve()
    destination = destination.resolve()
    build = (root / "build").resolve()
    if destination == build or not destination.is_relative_to(build):
        raise RuntimeError("Destination must be strictly below this workspace's build/")
    lock = json.loads((root / "patches/upstream/series.json").read_text())
    base = lock["base_commit"]
    if git(root / "upstream", "rev-parse", "HEAD").decode().strip() != base:
        raise RuntimeError("Pinned upstream HEAD mismatch")
    for entry in lock["patches"]:
        patch_path(root, entry)
    stamp_path = destination.parent / (destination.name + ".overlay.json")
    if not destination.exists():
        if reverse:
            raise RuntimeError("No checkout to reverse")
        destination.parent.mkdir(parents=True, exist_ok=True)
        git(root, "-c", "core.autocrlf=false", "clone", "--no-hardlinks", "--no-checkout",
            root / "upstream", destination)
        git(destination, "config", "core.autocrlf", "false")
        git(destination, "checkout", "--detach", base)
    if git(destination, "rev-parse", "HEAD").decode().strip() != base:
        raise RuntimeError("Disposable checkout HEAD mismatch")
    if git(destination, "ls-files", "--others", "--exclude-standard").strip():
        raise RuntimeError("Unowned untracked files in checkout")
    diff = git(destination, "diff", "--binary", "HEAD")
    stamp = json.loads(stamp_path.read_text()) if stamp_path.exists() else None
    if stamp:
        if stamp["base_commit"] != base or stamp["diff_sha256"] != digest(diff):
            raise RuntimeError("Checkout changed outside maintained overlay; refusing to overwrite")
        for entry in stamp["patches"]:
            patch_path(root, entry)
    elif diff:
        raise RuntimeError("Unowned tracked changes in checkout")
    if not reverse and stamp:
        if stamp["patches"] != lock["patches"]:
            raise RuntimeError("Patch series changed; reverse the old overlay first")
        print(f"PASS unchanged overlay: {destination}")
        return
    entries = list(reversed(stamp["patches"])) if reverse and stamp else lock["patches"]
    if reverse and not stamp:
        raise RuntimeError("No recorded overlay to reverse")
    applied = []
    try:
        for entry in entries:
            path = patch_path(root, entry)
            flags = ["--reverse"] if reverse else []
            git(destination, "apply", "--index", "--check", *flags, path)
            git(destination, "apply", "--index", *flags, path)
            applied.append(entry)
    except RuntimeError:
        for entry in reversed(applied):
            flags = [] if reverse else ["--reverse"]
            git(destination, "apply", "--index", *flags, patch_path(root, entry))
        raise
    if reverse:
        if git(destination, "diff", "--binary", "HEAD"):
            raise RuntimeError("Reversal did not restore the pinned tracked tree")
        stamp_path.unlink()
    else:
        stamp_path.write_text(json.dumps({"base_commit": base, "patches": entries,
            "diff_sha256": digest(git(destination, "diff", "--binary", "HEAD"))}, indent=2) + "\n")
    print(f"PASS {'reversed' if reverse else 'applied'} {len(entries)} patches: {destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--reverse", action="store_true")
    args = parser.parse_args()
    try:
        prepare(args.destination, args.reverse)
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        parser.exit(1, f"FAIL overlay: {error}\n")


if __name__ == "__main__":
    main()
