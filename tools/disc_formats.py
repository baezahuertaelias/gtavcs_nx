"""Catalogue the disc's internal formats as metadata only, and check each file's structure.

    python tools/disc_formats.py <gamefolder> --out <coverage.json>

Output holds counts, sizes and verdicts per format: no file content, member names or
hashes of game data. Owner says what consumes the format at run time; see
docs/milestones/M03/FORMATS.md. An exit status of 1 means a file failed its check.
"""
import argparse
import json
from pathlib import Path
import struct
import sys
import zlib

SECTOR = 2048
OWNERS = {
    "DTZ": "guest code; upstream may inflate natively, the guest relocates the GTAG image",
    "LVZ": "guest code; level WRLD image, inflated as for DTZ",
    "IMG": "guest streaming through sector reads; no generic archive reader",
    "DIR": "guest code; index for its IMG",
    "SCM": "guest code; mission script",
    "GXT": "guest code; text tables",
    "XTX": "guest code; rasters uploaded through the GS path",
    "PSP": "guest code; frontend layouts despite the name",
    "ICO": "guest code; save icon written to the memory card",
    "PSS": "native movie player",
    "VB": "native sound driver stream",
    "RAW": "native sound driver bank",
    "SDT": "native sound driver table",
    "BIN": "native sound driver lookup",
    "IRX": "IOP HLE; modules are identified, not executed as guest IOP code",
    "CNF": "boot metadata",
    "90": "recompiled executable; identity checked by SHA-1",
}


class Bad(ValueError):
    pass


def require(condition, why):
    if not condition:
        raise Bad(why)


def inflated(path, magic):
    decoder, total, head = zlib.decompressobj(), 0, b""
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            data = decoder.decompress(chunk)
            if len(head) < 4:
                head += data[:4]
            total += len(data)
    require(decoder.eof, "zlib stream does not end")
    require(head[:4] == magic, "unexpected inflated magic")
    return {"inflated_bytes": total}


def gxt(path, _):
    data = Path(path).read_bytes()
    require(data[:4] == b"TABL", "missing TABL")
    size = struct.unpack_from("<I", data, 4)[0]
    require(size % 12 == 0 and 8 + size <= len(data), "invalid table size")
    entries = [struct.unpack_from("<I", data, 8 + i + 8)[0] for i in range(0, size, 12)]
    require(all(offset < len(data) for offset in entries), "table offset past the end")
    return {"tables": len(entries)}


def magic_check(prefix, why):
    def check(path, _):
        with open(path, "rb") as handle:
            require(handle.read(len(prefix)) == prefix, why)
        return {}
    return check


def sound_table(path, _):
    data = Path(path).read_bytes()
    require(len(data) % 12 == 0, "sound table is not 12-byte records")
    bank = Path(path).with_suffix(".RAW")
    require(bank.is_file(), "no matching RAW bank")
    limit = bank.stat().st_size
    records = [struct.unpack_from("<III", data, i) for i in range(0, len(data), 12)]
    require(all(offset <= limit for offset, _, _ in records), "sound offset past the bank")
    return {"sounds": len(records)}


def mocap_index(path, _):
    data = Path(path).read_bytes()
    require(len(data) % 32 == 0, "index is not 32-byte records")
    image = Path(path).with_suffix(".IMG")
    limit = image.stat().st_size / SECTOR
    records = [struct.unpack_from("<II", data, i) for i in range(0, len(data), 32)]
    require(all(offset + size <= limit for offset, size in records), "member past the end of its IMG")
    return {"members": len(records)}


def img(path, _):
    with open(path, "rb") as handle:
        head = handle.read(8)
    size = Path(path).stat().st_size
    return {"sector_aligned": size % SECTOR == 0, "header": classify_img(head)}


def classify_img(head):
    if head[:4] == b"xet\0":
        return "xet raster"
    if head[:4] == b"RESE" or head[:5] == b"RESET":
        return "romfs"
    return "no self-describing header"


def blocks(path, _):
    size = Path(path).stat().st_size
    require(size % 16 == 0, "ADPCM stream is not 16-byte blocks")
    return {}


def raw_bank(path, _):
    return {}


CHECKS = {
    "DTZ": (inflated, b"GATG"), "LVZ": (inflated, b"DLRW"), "GXT": (gxt, None), "SDT": (sound_table, None),
    "DIR": (mocap_index, None), "IMG": (img, None), "VB": (blocks, None), "RAW": (raw_bank, None),
    "PSS": (magic_check(b"\0\0\1\xba", "not an MPEG program stream"), None),
    "XTX": (magic_check(b"xet\0", "missing xet magic"), None),
    "IRX": (magic_check(b"\x7fELF", "not an ELF module"), None),
    "90": (magic_check(b"\x7fELF", "not an ELF"), None),
    "PSP": (magic_check(b"<", "unexpected layout header"), None),
}


def merge(summary, details):
    for key, value in details.items():
        if isinstance(value, bool) or isinstance(value, str):
            bucket = summary.setdefault(key, {})
            bucket[str(value)] = bucket.get(str(value), 0) + 1
        else:
            summary[key] = summary.get(key, 0) + value


def catalogue(root):
    root = Path(root)
    formats, failures = {}, []
    files = [p for p in root.rglob("*") if p.is_file() and ".nxdisc" not in p.parts and "mods" not in p.parts and "userdata" not in p.parts]
    for path in sorted(files):
        extension = path.suffix.lstrip(".").upper()
        entry = formats.setdefault(extension, {"files": 0, "bytes": 0, "owner": OWNERS.get(extension, "unclassified"), "checked": 0})
        entry["files"] += 1
        entry["bytes"] += path.stat().st_size
        check = CHECKS.get(extension)
        if not check:
            continue
        try:
            merge(entry, check[0](path, check[1]))
            entry["checked"] += 1
        except Bad as error:
            failures.append({"format": extension, "file_index": entry["files"], "reason": str(error)})
    return {"formats": formats, "failures": failures, "files": len(files)}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("gamefolder")
    parser.add_argument("--out")
    args = parser.parse_args()
    report = catalogue(args.gamefolder)
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8")
    print(f"{report['files']} files, {len(report['formats'])} formats, {len(report['failures'])} failures")
    for failure in report["failures"]:
        print("FAIL", failure)
    sys.exit(1 if report["failures"] else 0)


if __name__ == "__main__":
    main()
