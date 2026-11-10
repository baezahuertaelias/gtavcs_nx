"""Inspect a CPU-test Switch artifact set; emit metadata only, never game bytes.

Example (inside the pinned container):
  python3 tools/inspect_cpu.py --build-dir /cache/runtime-build \
      --stem gtavcs_runtime_tests --compile-commands /cache/runtime-build/compile_commands.json \
      --allow-root /work --allow-root /cache/upstream --allow-root /cache/deps \
      --allow-root /cache/runtime-build
"""
import argparse
import hashlib
import json
import mmap
from pathlib import Path
import re
import shlex
import struct
import sys


class InspectionError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InspectionError(message)


def region(data, offset, size, name):
    require(offset >= 0 and size >= 0 and offset <= len(data) and
            size <= len(data) - offset, f"Truncated/out-of-range {name}")
    return offset, offset + size


def unpack(data, pattern, offset, name):
    region(data, offset, struct.calcsize(pattern), name)
    return struct.unpack_from(pattern, data, offset)


def page_up(value):
    return (value + 0xfff) & ~0xfff


def inspect_elf(data):
    header = unpack(data, "<16sHHIQQQIHHHHHH", 0, "ELF header")
    ident, kind, machine, version, entry, phoff, _, _, ehsize, phsize, phnum, *_ = header
    require(ident[:7] == b"\x7fELF\x02\x01\x01", "Expected little-endian ELF64")
    require(kind == 3 and machine == 183 and version == 1,
            "Expected AArch64 position-independent ET_DYN ELF")
    require(ehsize == 64 and phsize == 56 and 0 < phnum <= 4096,
            "Invalid ELF/program-header sizes")
    region(data, phoff, phsize * phnum, "program-header table")
    loads, dynamic = [], []
    for index in range(phnum):
        typ, flags, offset, address, _, filesz, memsz, alignment = unpack(
            data, "<IIQQQQQQ", phoff + index * phsize, "program header")
        require(typ != 3, "ELF contains a host INTERP loader")
        if typ not in (1, 2):
            continue
        region(data, offset, filesz, "load/dynamic segment")
        require(filesz <= memsz and address + memsz <= 1 << 64,
                "Invalid segment file/memory extent")
        item = {"flags": flags, "offset": offset, "address": address,
                "file_size": filesz, "memory_size": memsz}
        if typ == 1:
            require(flags & ~7 == 0 and flags & 4, "Invalid/unreadable LOAD segment")
            require(flags & 3 != 3, "Writable executable LOAD segment")
            require(alignment in (0, 1) or
                    (alignment & (alignment - 1) == 0 and offset % alignment == address % alignment),
                    "Invalid LOAD alignment/congruence")
            loads.append(item)
        else:
            dynamic.append(item)
    loads.sort(key=lambda item: item["address"])
    require(len(loads) == 3 and [item["flags"] for item in loads] == [5, 4, 6],
            "Expected exactly RX, R, RW LOAD segments for the NRO")
    for left, right in zip(loads, loads[1:]):
        require(left["address"] + left["memory_size"] <= right["address"],
                "Overlapping LOAD memory ranges")
    require(loads[0]["address"] == 0 and
            loads[0]["address"] <= entry < loads[0]["address"] + loads[0]["memory_size"],
            "ELF entry is outside the executable NRO image")
    require(len(dynamic) == 1, "Expected one dynamic table")
    dyn = dynamic[0]
    require(dyn["file_size"] % 16 == 0, "Malformed dynamic table size")
    tags = {}
    terminated = False
    for offset in range(dyn["offset"], dyn["offset"] + dyn["file_size"], 16):
        tag, value = unpack(data, "<qQ", offset, "dynamic entry")
        if tag == 0:
            terminated = True
            break
        require(tag != 1, "Host dynamic NEEDED dependency")
        require(tag not in tags, f"Duplicate dynamic tag {tag}")
        tags[tag] = value
    require(terminated, "Unterminated dynamic table")
    require(22 not in tags and not tags.get(30, 0) & 4, "ELF declares text relocations")
    require(tags.get(0x6ffffffb, 0) & 0x08000000, "Missing dynamic PIE flag")

    def file_address(address, size):
        candidates = [item for item in loads if item["address"] <= address and
                      size <= item["file_size"] - (address - item["address"])]
        require(len(candidates) == 1, "Dynamic table points outside file-backed LOAD data")
        item = candidates[0]
        return item["offset"] + address - item["address"]

    targets = set()
    require(file_address(dyn["address"], dyn["file_size"]) == dyn["offset"],
            "Dynamic table is not mapped at its declared LOAD address")

    def writable_target(address):
        require(address % 8 == 0, "Unaligned 64-bit relocation write")
        candidates = [item for item in loads if item["address"] <= address and
                      8 <= item["memory_size"] - (address - item["address"])]
        require(len(candidates) == 1 and candidates[0]["flags"] & 2,
                f"Relocation writes outside writable LOAD memory at 0x{address:x}")
        require(address not in targets, f"Duplicate relocation write at 0x{address:x}")
        targets.add(address)

    # Switch static PIE uses RELATIVE RELA or packed RELR. Unknown relocation
    # semantics are rejected rather than assuming their write width/support.
    require(tags.get(18, 0) == 0, "Unsupported DT_REL runtime relocations")
    rela_count = 0

    def rela_table(address, size, entry_size):
        nonlocal rela_count
        require(entry_size == 24 and size % 24 == 0, "Malformed RELA table")
        start = file_address(address, size)
        for offset in range(start, start + size, 24):
            target, info, _ = unpack(data, "<QQq", offset, "RELA entry")
            typ, symbol = info & 0xffffffff, info >> 32
            if typ == 0:  # R_AARCH64_NONE writes nothing.
                continue
            require(typ == 1027 and symbol == 0,
                    f"Unsupported runtime RELA type/symbol {typ}/{symbol}")
            writable_target(target)
            rela_count += 1

    if tags.get(8, 0):
        require(7 in tags, "Missing RELA address")
        rela_table(tags[7], tags[8], tags.get(9, 0))
    if tags.get(2, 0):
        require(23 in tags and tags.get(20) == 7, "Unsupported PLT relocation table")
        rela_table(tags[23], tags[2], 24)
    relr_entries = 0
    relr_targets = 0
    if tags.get(35, 0):
        require(36 in tags and tags.get(37) == 8 and tags[35] % 8 == 0,
                "Malformed RELR table")
        start = file_address(tags[36], tags[35])
        cursor = None
        for offset in range(start, start + tags[35], 8):
            value, = unpack(data, "<Q", offset, "RELR entry")
            relr_entries += 1
            if value & 1 == 0:
                writable_target(value)
                relr_targets += 1
                cursor = value + 8
            else:
                require(cursor is not None, "RELR bitmap precedes its base address")
                for bit in range(1, 64):
                    if value & (1 << bit):
                        writable_target(cursor + (bit - 1) * 8)
                        relr_targets += 1
                cursor += 63 * 8
            require(cursor <= 1 << 64, "RELR cursor overflow")
    return {"machine": "AArch64", "format": "ELF64 little-endian PIE",
            "entry": entry, "segments": loads,
            "relocations": {"rela_writes": rela_count, "relr_entries": relr_entries,
                            "relr_writes": relr_targets, "writable_writes": len(targets),
                            "text_writes": 0}}


def inspect_nro(data, nacp, loads, title=None, elf=None):
    require(len(nacp) == 0x4000, "NACP must be exactly 0x4000 bytes")
    require(data[0x10:0x14] == b"NRO0", "Invalid NRO magic")
    version, image_size, flags = unpack(data, "<III", 0x14, "NRO header")
    require(version == 0 and flags == 0 and image_size >= 0x80,
            "Unsupported/malformed NRO header")
    region(data, 0, image_size, "NRO image")
    nro_segments = []
    for index, load in enumerate(loads):
        offset, size = unpack(data, "<II", 0x20 + index * 8, "NRO segment")
        require(offset == load["address"] and size == page_up(load["file_size"]),
                "NRO segment does not match its ELF LOAD segment")
        require(offset % 0x1000 == 0 and size % 0x1000 == 0,
                "NRO segment is not page aligned")
        region(data, offset, size, "NRO segment data")
        require(offset + size <= image_size, "NRO segment extends into assets")
        nro_segments.append({"offset": offset, "size": size})
    require(nro_segments[0]["offset"] == 0 and nro_segments[0]["size"] >= 0x80,
            "NRO text does not cover its header")
    for left, right in zip(nro_segments, nro_segments[1:]):
        require(left["offset"] + left["size"] <= right["offset"],
                "Overlapping NRO segments")
    require(nro_segments[-1]["offset"] + nro_segments[-1]["size"] == image_size,
            "NRO size differs from the initialized image extent")
    bss_size, = unpack(data, "<I", 0x38, "NRO BSS size")
    require(bss_size == page_up(loads[-1]["memory_size"]) - nro_segments[-1]["size"],
            "NRO BSS differs from ELF writable memory extent")
    magic, asset_version, icon_offset, icon_size, nacp_offset, nacp_size, romfs_offset, romfs_size = unpack(
        data, "<II6Q", image_size, "NRO asset header")
    require(magic == 0x54455341 and asset_version == 0, "Invalid NRO asset header")
    require(romfs_size == 0, "CPU test NRO must not contain a packaged RomFS")
    assets = []
    for name, offset, size in (("icon", icon_offset, icon_size),
                               ("NACP", nacp_offset, nacp_size),
                               ("RomFS", romfs_offset, romfs_size)):
        if not size:
            continue
        require(offset >= 56, f"{name} overlaps the NRO asset header")
        region(data, image_size + offset, size, name)
        assets.append((offset, offset + size, name))
    assets.sort()
    for left, right in zip(assets, assets[1:]):
        require(left[1] <= right[0], "Overlapping NRO asset entries")
    require(nacp_size == len(nacp) and
            data[image_size + nacp_offset:image_size + nacp_offset + nacp_size] == nacp,
            "Embedded/supplied NACP mismatch")
    if title is not None:
        require(nacp[:0x200].split(b"\0", 1)[0] == title.encode("utf-8"),
                "Unexpected NACP title")
    if elf is not None:
        require(data[:0x10] == elf[loads[0]["offset"]:loads[0]["offset"] + 0x10],
                "NRO startup entry differs from ELF")
        for index, load in enumerate(loads):
            # elf2nro fills its header at 0x10..0x7f. Every other initialized
            # byte must come from this ELF, not a similarly sized stale NRO.
            skip = 0x80 if index == 0 else 0
            length = load["file_size"] - skip
            for start in range(0, length, 4 * 1024 * 1024):
                size = min(4 * 1024 * 1024, length - start)
                source = load["offset"] + skip + start
                target = load["address"] + skip + start
                require(elf[source:source + size] == data[target:target + size],
                        "NRO initialized payload differs from ELF")
    return {"image_size": image_size, "file_size": len(data), "bss_size": bss_size,
            "segments": nro_segments, "embedded_nacp_matches": True,
            "elf_payload_matches": elf is not None, "romfs_size": 0, "icon_size": icon_size}


FORBIDDEN_LIBRARY = re.compile(
    r"(?:^|[/\\\s])(?:lib)?(?:SDL[23]|vulkan|volk|X11|wayland\w*|asound|avcodec|"
    r"avformat|avutil|swresample|swscale|d3d11|dxgi|EGL|GL)(?:[.\-_]|$)", re.I)


def inspect_build_inputs(commands_path, map_path, allowed_roots, build_dir):
    roots = [Path("/opt/devkitpro").resolve(), *[Path(item).resolve() for item in allowed_roots]]

    def allowed(value, directory, label):
        path = Path(value)
        if not path.is_absolute():
            path = directory / path
        path = path.resolve()
        require(any(path.is_relative_to(root) for root in roots),
                f"Unapproved {label} path: {path}")

    result = {}
    if commands_path:
        raw = commands_path.read_bytes()
        commands = json.loads(raw)
        require(isinstance(commands, list) and commands, "Empty/malformed compile commands")
        for entry in commands:
            directory = Path(entry["directory"]).resolve()
            allowed(directory, build_dir, "compile directory")
            allowed(entry["file"], directory, "source")
            args = entry.get("arguments") or shlex.split(entry["command"])
            require(args and args[0] in (
                "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++",
                "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc"), "Non-target compiler")
            require(any(arg == "-D__SWITCH__" or arg.startswith("-D__SWITCH__=") for arg in args),
                    "Compilation lacks the Switch target definition")
            index = 1
            while index < len(args):
                arg = args[index]
                if arg in ("-I", "-isystem", "-iquote", "-idirafter", "-include", "--sysroot"):
                    index += 1
                    require(index < len(args), "Missing include/sysroot argument")
                    allowed(args[index], directory, "include/sysroot")
                else:
                    for prefix in ("-I", "-isystem", "-iquote", "-idirafter", "--sysroot="):
                        if arg.startswith(prefix) and len(arg) > len(prefix):
                            allowed(arg[len(prefix):], directory, "include/sysroot")
                            break
                require(not FORBIDDEN_LIBRARY.search(arg), "Forbidden desktop/graphics/media build input")
                if arg.startswith("-l"):
                    require(not FORBIDDEN_LIBRARY.search(arg[2:]), "Forbidden linked backend")
                index += 1
        result["compile_commands"] = {"count": len(commands), "target_compilers_only": True,
                                       "sha256": hashlib.sha256(raw).hexdigest()}
    if map_path:
        raw = map_path.read_bytes()
        contents = raw.decode("utf-8", errors="strict")
        require(not FORBIDDEN_LIBRARY.search(contents) and not re.search(r"\.dll\b|\.so(?:\.|\s|$)", contents, re.I),
                "Forbidden desktop/graphics/media library in link map")
        archives = set(re.findall(r"([^\s()]+\.a)(?=\(|\s|$)", contents))
        require(archives, "No archive inputs found in link map")
        for archive in archives:
            require(not FORBIDDEN_LIBRARY.search(archive), "Forbidden backend archive")
            allowed(archive, build_dir, "archive")
        result["link_map"] = {"archive_count": len(archives), "approved_archive_roots_only": True,
                              "sha256": hashlib.sha256(raw).hexdigest()}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--stem", required=True)
    parser.add_argument("--title")
    parser.add_argument("--compile-commands", type=Path)
    parser.add_argument("--map", dest="link_map", type=Path)
    parser.add_argument("--allow-root", action="append", default=[])
    args = parser.parse_args()
    require(re.fullmatch(r"[A-Za-z0-9_-]+", args.stem), "Unsafe artifact stem")
    require(not (args.compile_commands or args.link_map) or args.allow_root,
            "Provide explicit --allow-root entries for build-input inspection")
    build = args.build_dir.resolve()
    paths = {kind: build / f"{args.stem}.{kind}" for kind in ("elf", "nro", "nacp")}
    nacp = paths["nacp"].read_bytes()
    with paths["elf"].open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as elf:
        elf_report = inspect_elf(elf)
        elf_hash = hashlib.sha256(elf).hexdigest()
        with paths["nro"].open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as nro:
            nro_report = inspect_nro(nro, nacp, elf_report["segments"], args.title, elf)
            nro_hash = hashlib.sha256(nro).hexdigest()
    inputs = inspect_build_inputs(args.compile_commands, args.link_map, args.allow_root, build)
    print(json.dumps({"result": "PASS", "stem": args.stem, "elf": elf_report, "nro": nro_report,
                      "sha256": {"elf": elf_hash, "nro": nro_hash,
                                 "nacp": hashlib.sha256(nacp).hexdigest()},
                      "build_inputs": inputs}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (InspectionError, OSError, ValueError, KeyError, struct.error) as error:
        print(f"FAIL CPU artifact inspection: {error}", file=sys.stderr)
        raise SystemExit(1)
