"""Extract the user's own disc into a gamefolder that can be read back byte for byte.

The tree keeps the disc's own names. `.nxdisc/manifest.tsv` records every file and
directory with its original LBA, and `.nxdisc/raw.bin` keeps the non-file sectors that
hold data, so sector reads behave as they do on the ISO. Output is private game data
and belongs only in the ignored local gamefolder or on the user's SD card.

    python tools/extract_disc.py extract <iso> <gamefolder> [--replace] [--report out.json]
    python tools/extract_disc.py verify <gamefolder> [--iso <iso>]
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "upstream/tools"))
from vcs_disc import ELF_NAME, SECTOR, IsoImage, check  # noqa: E402

META = ".nxdisc"
FORMAT = "NXDISC\t1"
CHUNK = 1 << 20


class DiscError(RuntimeError):
    pass


def sectors_of(size):
    return (size + SECTOR - 1) // SECTOR


def require_safe(path):
    """Refuse what FolderDisc::load refuses: escapes, separators, lower case and control bytes."""
    parts = path.split("/")
    if not path or any(p in ("", ".", "..") or set(p) & set('\\:;') for p in parts) or \
            any(ord(c) < 0x20 or "a" <= c <= "z" for c in path):
        raise DiscError(f"Unsafe disc path: {path!r}")


def layout(image):
    """Return directories, files, and both in one list in on-disc walk order."""
    dirs, files, ordered = [], [], []

    def walk(record, prefix):
        entry = {"lba": record["lba"], "size": record["size"], "path": prefix.rstrip("/"), "directory": True}
        dirs.append(entry)
        ordered.append(entry)
        for child in image.list_dir(record):
            path = prefix + child["name"]
            require_safe(path)
            if child["dir"]:
                walk(child, path + "/")
            else:
                entry = {"lba": child["lba"], "size": child["size"], "path": path, "directory": False}
                files.append(entry)
                ordered.append(entry)

    walk(image.root, "")
    names = [entry["path"].upper() for entry in dirs + files]
    if len(names) != len(set(names)):
        raise DiscError("Disc holds paths that differ only by case")
    return dirs, files, ordered


def file_extents(files, total):
    """A zero-length file holds no sectors, so it cannot overlap anything (as in FolderDisc)."""
    position = 0
    for entry in sorted(files, key=lambda item: item["lba"]):
        if entry["size"] == 0:
            if entry["lba"] > total:
                raise DiscError(f"File extent past the end of the disc: {entry['path']}")
            continue
        if entry["lba"] < position:
            raise DiscError(f"Overlapping file extent at {entry['path']}")
        position = entry["lba"] + sectors_of(entry["size"])
        if position > total:
            raise DiscError(f"File extent past the end of the disc: {entry['path']}")


def write_atomic(path, chunks):
    temporary = path.with_name(path.name + ".part")
    digest = hashlib.sha256()
    with open(temporary, "wb") as handle:
        for chunk in chunks:
            digest.update(chunk)
            handle.write(chunk)
    os.replace(temporary, path)
    return digest.hexdigest()


def read_range(handle, offset, size):
    handle.seek(offset)
    left = size
    while left:
        chunk = handle.read(min(left, CHUNK))
        if not chunk:
            raise DiscError(f"Unexpected end of image at byte {offset}")
        left -= len(chunk)
        yield chunk


def raw_spans(handle, files, total):
    """Find non-file sectors holding data; everything else on the disc is zero."""
    used = sorted((entry["lba"], sectors_of(entry["size"])) for entry in files)
    spans, position = [], 0
    for lba, count in used + [(total, 0)]:
        for sector in range(position, lba):
            handle.seek(sector * SECTOR)
            if handle.read(SECTOR).count(0) != SECTOR:
                if spans and spans[-1][0] + spans[-1][1] == sector:
                    spans[-1][1] += 1
                else:
                    spans.append([sector, 1])
        position = max(position, lba + count)
    return spans


def tail_must_be_zero(handle, entry):
    used = entry["size"] % SECTOR
    if used:
        handle.seek(entry["lba"] * SECTOR + entry["size"])
        if handle.read(SECTOR - used).count(0) != SECTOR - used:
            raise DiscError(f"Non-zero padding after {entry['path']}")


def check_targets(dest, files, replace):
    for entry in files:
        target = dest / entry["path"]
        if target.exists() and not replace and target.stat().st_size != entry["size"]:
            raise DiscError(f"{target} already exists with another size; pass --replace to overwrite disc files")


def extract(iso, dest, replace=False, identity=check):
    result = identity(str(iso))
    if not result["ok"]:
        raise DiscError(result["message"])
    bytes_total = Path(iso).stat().st_size
    if bytes_total % SECTOR:
        raise DiscError("Image is not a whole number of sectors")
    total = bytes_total // SECTOR
    image = IsoImage(str(iso))
    try:
        dirs, files, ordered = layout(image)
    finally:
        image.f.close()
    file_extents(files, total)
    dest = Path(dest)
    check_targets(dest, files, replace)
    with open(iso, "rb") as handle:
        # Every refusal happens before anything is written.
        for entry in files:
            tail_must_be_zero(handle, entry)
    meta = dest / META
    meta.mkdir(parents=True, exist_ok=True)
    with open(iso, "rb") as handle:
        for entry in files:
            target = dest / entry["path"]
            target.parent.mkdir(parents=True, exist_ok=True)
            entry["sha256"] = write_atomic(target, read_range(handle, entry["lba"] * SECTOR, entry["size"]))
        spans = raw_spans(handle, files, total)

        def raw_chunks():
            for lba, count in spans:
                yield from read_range(handle, lba * SECTOR, count * SECTOR)

        raw_sha256 = write_atomic(meta / "raw.bin", raw_chunks())
        whole = hashlib.sha256()
        for chunk in read_range(handle, 0, bytes_total):
            whole.update(chunk)
    lines = [FORMAT, f"sectors\t{total}", f"volume\t{image.volume_id}", f"elf_sha1\t{result['sha1']}",
             f"iso_sha256\t{whole.hexdigest()}", f"raw_sha256\t{raw_sha256}"]
    # One list: FolderDisc takes each directory's listing order from this order.
    lines += [f"D\t{e['lba']}\t{e['size']}\t-\t{e['path']}" if e["directory"]
              else f"F\t{e['lba']}\t{e['size']}\t{e['sha256']}\t{e['path']}" for e in ordered]
    offset = 0
    for lba, count in spans:
        lines.append(f"R\t{lba}\t{count}\t{offset}")
        offset += count * SECTOR
    manifest = ("\n".join(lines) + "\n").encode("ascii")
    manifest_sha256 = write_atomic(meta / "manifest.tsv", [manifest])
    return {"sectors": total, "files": len(files), "directories": len(dirs), "raw_spans": len(spans),
            "raw_bytes": offset, "file_bytes": sum(f["size"] for f in files),
            "iso_sha256": whole.hexdigest(), "manifest_sha256": manifest_sha256, "elf_sha1": result["sha1"]}


def parse_manifest(dest):
    lines = (Path(dest) / META / "manifest.tsv").read_text(encoding="ascii").splitlines()
    if not lines or lines[0] != FORMAT:
        raise DiscError("Not a version 1 disc manifest")
    fields, files, raw = {}, [], []
    for line in lines[1:]:
        parts = line.split("\t")
        if parts[0] == "F":
            files.append({"lba": int(parts[1]), "size": int(parts[2]), "sha256": parts[3], "path": parts[4]})
        elif parts[0] == "R":
            raw.append((int(parts[1]), int(parts[2]), int(parts[3])))
        elif parts[0] != "D":
            fields[parts[0]] = parts[1]
    return fields, files, raw


def rebuilt_digest(dest, fields, files, raw):
    """Hash the whole disc as the folder form presents it."""
    dest = Path(dest)
    for empty in (f for f in files if f["size"] == 0):
        path = dest / empty["path"]
        if not path.is_file() or path.stat().st_size or hashlib.sha256().hexdigest() != empty["sha256"]:
            raise DiscError(f"File differs from the manifest: {empty['path']}")
    # Zero-length files hold no sectors and take no part in the order, as in FolderDisc.
    spans = [(f["lba"], sectors_of(f["size"]), f) for f in files if f["size"]]
    spans += [(lba, count, offset) for lba, count, offset in raw]
    whole, position = hashlib.sha256(), 0
    zero = bytes(SECTOR)
    with open(dest / META / "raw.bin", "rb") as raw_handle:
        for lba, count, source in sorted(spans, key=lambda span: span[0]):
            if lba < position:
                raise DiscError(f"Overlapping extent at sector {lba}")
            for _ in range(lba - position):
                whole.update(zero)
            if isinstance(source, dict):
                digest, size = hashlib.sha256(), 0
                with open(dest / source["path"], "rb") as handle:
                    for chunk in iter(lambda: handle.read(CHUNK), b""):
                        digest.update(chunk)
                        whole.update(chunk)
                        size += len(chunk)
                if size != source["size"] or digest.hexdigest() != source["sha256"]:
                    raise DiscError(f"File differs from the manifest: {source['path']}")
                whole.update(bytes(count * SECTOR - size))
            else:
                for chunk in read_range(raw_handle, source, count * SECTOR):
                    whole.update(chunk)
            position = lba + count
    total = int(fields["sectors"])
    for _ in range(total - position):
        whole.update(zero)
    return whole.hexdigest()


def verify(dest, iso=None):
    fields, files, raw = parse_manifest(dest)
    digest = rebuilt_digest(dest, fields, files, raw)
    if digest != fields["iso_sha256"]:
        raise DiscError("Folder does not rebuild the recorded disc image")
    if iso is not None:
        whole = hashlib.sha256()
        with open(iso, "rb") as handle:
            for chunk in iter(lambda: handle.read(CHUNK), b""):
                whole.update(chunk)
        if whole.hexdigest() != digest:
            raise DiscError("Folder does not match the given disc image")
    return digest


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    extract_parser = commands.add_parser("extract")
    extract_parser.add_argument("iso")
    extract_parser.add_argument("gamefolder")
    extract_parser.add_argument("--replace", action="store_true")
    extract_parser.add_argument("--report")
    verify_parser = commands.add_parser("verify")
    verify_parser.add_argument("gamefolder")
    verify_parser.add_argument("--iso")
    args = parser.parse_args()
    try:
        if args.command == "extract":
            report = extract(args.iso, args.gamefolder, args.replace)
            if args.report:
                Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("PASS extracted " + json.dumps(report))
        else:
            print("PASS rebuilt disc sha256 " + verify(args.gamefolder, args.iso))
    except DiscError as error:
        sys.exit(f"error: {error}")


if __name__ == "__main__":
    main()
