"""Inventory source metadata and a user-owned ISO; never copy executable/assets.

Run with a host venv containing discovery-requirements.txt. No binary decompilation
or game execution. The parser reports syntactic candidates, not semantic reachability.
"""
import argparse
import collections
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tomllib

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "upstream"
OUT = ROOT / "docs/discovery"


def git(*args):
    return subprocess.check_output(["git", "-C", str(UPSTREAM), *args], text=True).strip()


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def source_inventory():
    import tree_sitter
    import tree_sitter_c
    import tree_sitter_cpp
    parsers = {"c": tree_sitter.Parser(tree_sitter.Language(tree_sitter_c.language())),
               "cpp": tree_sitter.Parser(tree_sitter.Language(tree_sitter_cpp.language()))}
    paths = git("ls-files").splitlines()
    roots = ("src/", "third_party/PS2Recomp/ps2xRuntime/", "third_party/PS2Recomp/ps2xIOP/",
             "third_party/PS2Recomp/ps2xRecomp/")
    files, symbols, declarations, errors, risks, includes = [], [], [], [], [], []
    patterns = {
        "x86_simd": r"\b(?:__m(?:128|256|512)\w*|_mm\w*|_mm256\w*|immintrin|xmmintrin|emmintrin)\b",
        "desktop_os": r"\b(?:_WIN32|__linux__|windows\.h|VirtualAlloc|VirtualProtect|mmap|mprotect|dlopen|fork|XDG_\w+)\b",
        "sdl3": r"\b(?:SDL_[A-Za-z0-9_]+|ImGui_ImplSDL3\w*)\b",
        "thread_time": r"\b(?:std::thread|std::jthread|thread_local|pthread_\w+|steady_clock|condition_variable|fesetround)\b",
        "file_io": r"\b(?:fopen|fseek|pread|ifstream|filesystem|cdrom0|sceCd\w+|sceMc\w+)\b",
        "incomplete_candidate": r"\b(?:TODO|FIXME|UNIMPLEMENTED|unimplemented)\b",
    }
    for path in paths:
        p = UPSTREAM / path
        if not path.startswith(roots) or p.suffix.lower() not in (".c", ".cc", ".cpp", ".h", ".hpp"):
            continue
        data = p.read_bytes().replace(b"\r\n", b"\n")
        text = data.decode("utf-8", "replace")
        tree = parsers["c" if p.suffix == ".c" else "cpp"].parse(data)
        files.append({"path": path, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                      "parse_has_error": tree.root_node.has_error})
        stack = [tree.root_node]
        while stack:
            node = stack.pop()
            if node.type in ("ERROR",) or node.is_missing:
                errors.append({"file": path, "line": node.start_point.row + 1, "kind": node.type})
            if node.type in ("function_definition", "function_declarator"):
                scopes = []
                ancestor = node.parent
                while ancestor:
                    if ancestor.type in ("namespace_definition", "class_specifier", "struct_specifier"):
                        name = ancestor.child_by_field_name("name")
                        if name: scopes.append(name.text.decode("utf-8", "replace"))
                    ancestor = ancestor.parent
                if node.type == "function_definition":
                    decl = node.child_by_field_name("declarator")
                    body = node.child_by_field_name("body")
                    signature = data[node.start_byte:body.start_byte if body else node.end_byte].decode("utf-8", "replace").strip()
                    symbols.append({"file": path, "line": node.start_point.row + 1,
                                    "end_line": node.end_point.row + 1, "scope": "::".join(reversed(scopes)),
                                    "declarator": decl.text.decode("utf-8", "replace") if decl else "",
                                    "signature": signature})
                else:
                    declarations.append({"file": path, "line": node.start_point.row + 1,
                                         "scope": "::".join(reversed(scopes)),
                                         "declarator": node.text.decode("utf-8", "replace")})
            stack.extend(reversed(node.children))
        for line_no, line in enumerate(text.splitlines(), 1):
            match = re.match(r'\s*#\s*include\s*([<\"])([^>\"]+)', line)
            if match: includes.append({"file": path, "line": line_no, "include": match[2]})
            for kind, pattern in patterns.items():
                hits = sorted(set(re.findall(pattern, line)))
                if hits: risks.append({"file": path, "line": line_no, "category": kind, "tokens": hits})
    save("source_files.json", files)
    save("source_functions.json", symbols)
    save("source_declarators.json", declarations)
    save("parser_gaps.json", errors)
    save("portability_candidates.json", risks)
    save("includes.json", includes)
    cmake = []
    for path in paths:
        if (path.endswith("CMakeLists.txt") or path.endswith(".cmake")) and (UPSTREAM / path).is_file():
            for n, line in enumerate((UPSTREAM / path).read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if re.search(r"(?i)\b(?:find_package|find_library|FetchContent_Declare|add_subdirectory|target_link_libraries|add_library|add_executable|target_compile_options|add_compile_options|set)\s*\(", line):
                    cmake.append({"file": path, "line": n, "statement_start": line.strip()})
    save("build_declarations.json", cmake)
    return {"source_files": len(files), "source_function_definitions": len(symbols),
            "function_declarators_including_definition_nodes": len(declarations), "parser_gap_nodes": len(errors),
            "portability_candidates": dict(collections.Counter(x["category"] for x in risks)),
            "include_sites": len(includes), "build_declaration_starts": len(cmake)}


def guest_inventory():
    functions = {}
    rows = 0
    for name in ("functions.csv", "extra_functions.csv"):
        with (UPSTREAM / "game/vcs" / name).open(newline="", encoding="utf-8-sig") as f:
            for row in csv.DictReader(f):
                rows += 1
                key = row["Start"].lower()
                entry = functions.setdefault(key, {"address": key, "names": [], "records": []})
                if row["Name"] not in entry["names"]: entry["names"].append(row["Name"])
                entry["records"].append({"map": name, "end": row["End"], "size": int(row["Size"])})
    save("guest_functions.json", sorted(functions.values(), key=lambda x: int(x["address"], 16)))
    config = tomllib.loads((UPSTREAM / "game/vcs/vcs.toml").read_text(encoding="utf-8"))
    overrides = tomllib.loads((UPSTREAM / "game/vcs/overrides.toml").read_text(encoding="utf-8"))
    save("recompiler_config.json", {"general": config.get("general", {}),
                                    "overrides": overrides,
                                    "limits": "Configuration metadata, not validated reachable functions or binary analysis"})
    vu = []
    for line in (UPSTREAM / "game/vcs/vu1_uploads.txt").read_text().splitlines():
        if not line.strip() or line.startswith("#"): continue
        src, dest, count = line.split()
        vu.append({"elf_address": "0x" + src, "vu_byte_address": "0x" + dest, "instruction_pairs": int(count)})
    save("vu_uploads.json", vu)
    macro_entries = []
    macro = None
    for n, line in enumerate((UPSTREAM / "third_party/PS2Recomp/ps2xRuntime/include/ps2_call_list.h").read_text().splitlines(), 1):
        match = re.match(r"#define\s+(\w+LIST)\(X\)", line)
        if match: macro = match[1]
        if macro:
            for name in re.findall(r"\bX\((\w+)\)", line):
                macro_entries.append({"macro": macro, "name": name, "line": n})
        if macro and not line.rstrip().endswith("\\") and not match: macro = None
    save("runtime_call_macros.json", macro_entries)
    return {"guest_map_rows": rows, "guest_unique_entry_addresses": len(functions),
            "vu_upload_records": len(vu), "runtime_macro_call_entries": len(macro_entries)}


def disc_inventory(iso):
    spec = importlib.util.spec_from_file_location("vcs_disc", UPSTREAM / "tools/vcs_disc.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    identity = module.check(str(iso))
    if not identity.get("ok"): raise ValueError(identity)
    image = module.IsoImage(iso)
    entries = []
    def walk(rec, prefix=""):
        for child in image.list_dir(rec):
            path = prefix + child["name"]
            entries.append({"path": path, "directory": child["dir"], "lba": child["lba"], "size": child["size"]})
            if child["dir"]: walk(child, path + "/")
    try:
        walk(image.root)
        save("disc_manifest.json", {"identity": identity, "volume_id": image.volume_id,
            "sector_size": module.SECTOR, "iso_bytes": iso.stat().st_size, "entries": entries,
            "limits": "ISO9660 tree metadata only; internal archive members, content hashes, raw-sector consumers and padding semantics are NOT inventoried"})
    finally:
        image.f.close()
    files = [x for x in entries if not x["directory"]]
    return {"disc_files": len(files), "disc_directories": len(entries) - len(files),
            "asset_extensions": dict(collections.Counter(Path(x["path"]).suffix.upper() or "<none>" for x in files)),
            "disc_identity": identity}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    summary = {"schema": 1, "upstream_commit": git("rev-parse", "HEAD"),
               "scope": "Tracked VCS source + PS2Recomp runtime/IOP/recompiler, supplied function maps, ISO9660 directory metadata",
               "limitations": ["Syntactic parsing is not a preprocessed compiler index or call graph",
                               "Macro-generated, assembly, indirect and dynamically discovered functions may be absent",
                               "Parser gap sites require targeted inspection in the owning implementation milestone",
                               "Archive members and runtime behavior require later asset/runtime validation",
                               "No game assets, executable bytes or generated game C++ are emitted"]}
    summary.update(source_inventory())
    summary.update(guest_inventory())
    summary.update(disc_inventory(args.iso.resolve()))
    save("summary.json", summary)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
