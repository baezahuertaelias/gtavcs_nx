"""Metadata-only M02 ownership, reserved-encoding and SDK declaration audit.

Private ELF/generated sources/preprocessor output never leave build/. The report
contains names, addresses, classifications and hashes only, never game bodies or
instruction words. Reserved encoding is NOT proof of data or unreachable code.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PCSX2_COMMIT = "4223d03c58f8fd1787da7af28fe80c2140e1a22a"
PCSX2_HASH = "80562704233e7d7f68d54878f4cf00cad2f63d1ba8362b76ea09634c5093ec1f"
TABLES = {"SPECIAL instruction": "tbl_Special", "COP2 format": "Int_COP2PrintTable",
          "COP0 CO-OP": "tbl_COP0_C0", "COP0 instruction format": "tbl_COP0"}


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def family(token, original):
    if token == "_mm_pause":
        return "processor_wait_hint"
    if token == "_mm_bsrli_si128":
        return "byte_lane_shift"
    if token.startswith("_mm_custom_"):
        return "per_lane_variable_shift"
    if token.startswith("_mm256_"):
        name = token[len("_mm256_"):]
        if name.startswith(("cmp", "movemask")):
            return "bitwise_or_comparison"
        if name.startswith(("load", "store")):
            return "memory_alignment_and_access"
        if name.startswith(("set", "blend")):
            return "lane_order_and_packing"
        if name.startswith(("cvt", "cast")):
            return "bit_cast_or_numeric_conversion"
        if name.endswith(("_ps", "_ss", "_pd", "_sd")):
            return "floating_point_arithmetic"
        if name.startswith(("add", "sub", "mul", "min", "max")):
            return "integer_arithmetic_and_saturation"
        raise RuntimeError(f"Unclassified AVX token: {token}")
    if original == "requires_manual_classification":
        raise RuntimeError(f"Unclassified token: {token}")
    return original


def ownership(path):
    if "/ps2xRecomp/" in path:
        return "host_generator", "M02-02/M02-03", "Native host code emits target expressions; generated output is compiled separately"
    if path.endswith("gs_gpu_parallel.cpp"):
        return "optional_backend", "outside SDL2/OpenGL baseline", "Optional paraLLEl-GS/Vulkan backend is excluded from Switch CPU target"
    if path.endswith("ps2_debug_panel.cpp"):
        return "later_milestone", "M09-01/M09-02", "Desktop debug UI is excluded from the CPU harness"
    if path.endswith(("vcs_hle_world.cpp", "vcs_rsl_world.cpp")):
        return "later_milestone", "M06-01/M06-03", "Native world/RSL integration and visual conformance are later work"
    if path.endswith("vcs_vu0_native.cpp"):
        return "later_milestone", "M04-12/M06-01", "Game-specific native VU0 hook is not registered by M02 CPU workloads; runtime interpreter remains in M02"
    if "/ps2xRuntime/" in path:
        return "target_core", "M02-05/M02-06/M02-07/M02-10", "Runtime/generated-code dependency or intrinsic header boundary"
    raise RuntimeError(f"Missing ownership: {path}")


def owner_resolution(record, lines):
    if record["owner"] is not None:
        return "syntactic_function", record["owner"]
    at = record["line"] - 1
    text = lines[at].strip()
    if text.startswith("#include"):
        return "include_boundary", "translation-unit intrinsic selection"
    if text.startswith("//"):
        return "comment_only", "lexical comment hit; no emitted operation"
    start = at
    while start > 0 and lines[start-1].rstrip().endswith("\\"):
        start -= 1
    macro = re.match(r"\s*#\s*define\s+(\w+)", lines[start])
    if macro:
        return "macro_definition", macro[1]
    if text.startswith("using "):
        return "type_alias", text.split()[1]
    decl = re.search(r"\b(\w+)\s*\(", text)
    if decl and text.endswith(";") and "=" not in text:
        return "function_declaration", decl[1]
    field = re.search(r"\b(\w+)\s*(?:\[[^]]*\])?\s*(?:=|;)", text)
    if field:
        return "register_or_member_declaration", field[1]
    raise RuntimeError(f"Unresolved null owner: {record['file']}:{record['line']}")


def elf_sections(data):
    if data[:6] != b"\x7fELF\x01\x01":
        raise RuntimeError("Expected private ELF32 little endian")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + i*header[11])
                for i in range(header[12])]
    names_header = sections[header[13]]
    names = data[names_header[4]:names_header[4]+names_header[5]]
    return [{"name": names[s[0]:].split(b"\0", 1)[0].decode(),
             "address": s[3], "size": s[5], "flags": s[2]} for s in sections]


def reference_tables(path):
    data = path.read_bytes()
    if sha(data) != PCSX2_HASH:
        raise RuntimeError("Pinned PCSX2 opcode table content mismatch")
    text = data.decode()
    result = {}
    for name in TABLES.values():
        match = re.search(r"\b"+name+r"\[\d+\][^=]*=\s*\{(.*?)\};", text, re.S)
        if not match:
            raise RuntimeError(f"Missing reference table {name}")
        result[name] = re.findall(r"\b[A-Za-z_]\w*\b", re.sub(r"//[^\n]*", "", match[1]))
    if [len(result[n]) for n in TABLES.values()] != [64, 32, 64, 32]:
        raise RuntimeError("Reference table lengths changed")
    return result


def declaration_probe():
    return '''// Public declaration contract only; no generated game source.
#include "ps2_syscalls.h"
#include "ps2_stubs.h"
#include "ps2_runtime_macros.h"
#include "ps2_runtime_calls.h"
#include "ThreadNaming.h"
#include "runtime/ee_scheduler.h"
#include "runtime/gs/gs_hw.h"
#include "runtime/ps2_vu1_compiled.h"
#include <type_traits>
using AuditHostCall = void (*)(uint8_t *, R5900Context *, PS2Runtime *);
#define AUDIT_SYSCALL(name) static_assert(std::is_same_v<decltype(&ps2_syscalls::name),AuditHostCall>); extern const bool audit_syscall_##name;
PS2_SYSCALL_LIST(AUDIT_SYSCALL)
#undef AUDIT_SYSCALL
#define AUDIT_STUB(name) static_assert(std::is_same_v<decltype(&ps2_stubs::name),AuditHostCall>); extern const bool audit_stub_##name;
PS2_STUB_LIST(AUDIT_STUB)
#undef AUDIT_STUB
static_assert(sizeof(__m128i)==16 && alignof(__m128i)==16);
'''


def compile_probes(reference, private):
    probe = private / "sdk-declarations.cpp"
    probe.write_text(declaration_probe())
    compiler = reference / "build/build-tools/llvm-mingw-20261020-ucrt-x86_64/bin/clang++.exe"
    runtime = ROOT / "upstream/third_party/PS2Recomp/ps2xRuntime"
    includes = ["-I"+str(runtime/"include"), "-I"+str(runtime/"src/lib/Kernel"),
                "-I"+str(ROOT/"upstream/third_party/PS2Recomp/ps2xIOP/include")]
    flags = ["-std=c++20", "-O2", "-ffp-contract=off", "-frounding-math", "-fno-strict-aliasing", "-fsigned-char"]
    with (private / "sdk-native-build.log").open("wb") as log:
        subprocess.run([str(compiler), *flags, "-march=x86-64-v3", *includes, "-c", str(probe),
                        "-o", str(private/"sdk-native.o")], check=True, stdout=log, stderr=subprocess.STDOUT)
        subprocess.run([str(compiler), *flags, "-march=x86-64-v3", *includes, "-E", "-dD", str(probe),
                        "-o", str(private/"sdk-native.ii")], check=True, stdout=log, stderr=subprocess.STDOUT)
    rel = private.relative_to(ROOT).as_posix()
    base = ["/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++", *flags,
            "-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-ftls-model=local-exec", "-fPIE",
            "-D__SWITCH__", "-DUSE_SSE2NEON", "-DSSE2NEON_PRECISE_MINMAX=1", "-DSSE2NEON_PRECISE_DIV=1",
            "-DSSE2NEON_PRECISE_SQRT=1", "-DSSE2NEON_PRECISE_DP=1", "-include", "/work/src/compat/switch_sse2neon_alloc.h",
            "-I/opt/devkitpro/libnx/include", "-I/work/build/m02/deps/sse2neon",
            "-I/work/upstream/third_party/PS2Recomp/ps2xRuntime/include",
            "-I/work/upstream/third_party/PS2Recomp/ps2xRuntime/src/lib/Kernel",
            "-I/work/upstream/third_party/PS2Recomp/ps2xIOP/include"]
    docker = ["docker", "run", "--rm", "--network=none", "--mount", f"type=bind,source={ROOT},target=/work",
              "-w", "/work", "gtavcs-nx-toolchain:m01"]
    with (private/"sdk-arm-build.log").open("wb") as log:
        for mode, extension in ((["-c"], "o"), (["-E", "-dD"], "ii")):
            subprocess.run([*docker, *base, *mode, f"/work/{rel}/sdk-declarations.cpp", "-o",
                            f"/work/{rel}/sdk-arm.{extension}"], check=True, stdout=log, stderr=subprocess.STDOUT)


def sdk_metadata(private):
    result = {}
    for arch in ("native", "arm"):
        obj = (private/f"sdk-{arch}.o").read_bytes()
        if arch == "arm":
            if obj[:4] != b"\x7fELF" or struct.unpack_from("<H", obj, 18)[0] != 183:
                raise RuntimeError("SDK target probe is not AArch64 ELF")
        elif struct.unpack_from("<H", obj)[0] != 0x8664:
            raise RuntimeError("SDK native probe is not AMD64 COFF")
        pp = (private/f"sdk-{arch}.ii").read_text(errors="strict")
        syscalls = re.findall(r"audit_syscall_(\w+)\s*;", pp)
        stubs = re.findall(r"audit_stub_(\w+)\s*;", pp)
        result[arch] = {"syscalls": sorted(set(syscalls)), "stubs": sorted(set(stubs)),
                        "expanded_declaration_counts":{"syscalls":len(syscalls),"stubs":len(stubs)},
                        "repeated_declarations":{name:count for name,count in Counter(stubs).items() if count>1},
                        "object_sha256": sha(obj), "preprocessed_sha256": sha(pp.encode()),
                        "avx2_enabled_in_probe":"__AVX2__" in re.findall(r"^#define\s+(\w+)",pp,re.M),
                        "avx256_intrinsic_tokens":len(re.findall(r"\b_mm256_\w+",pp)),
                        "architecture": "AArch64 ELF" if arch == "arm" else "AMD64 COFF",
                        "guest_call_abi": "platform-default" if arch == "arm" else "preserve_none for guest entries; SDK host callbacks default",
                        "prototype": "void(uint8_t*, R5900Context*, PS2Runtime*)"}
    for kind in ("syscalls", "stubs"):
        if not result["native"][kind] or result["native"][kind] != result["arm"][kind]:
            raise RuntimeError(f"Native/ARM preprocessed {kind} declaration mismatch")
    return result


def audit(reference, private, output):
    simd = load(ROOT/"docs/milestones/M02/simd_candidates.json")
    generation = load(ROOT/"docs/milestones/M02/generation_metadata.json")
    pin = subprocess.check_output(["git", "-C", str(ROOT/"upstream"), "rev-parse", "HEAD"], text=True).strip()
    if pin != simd["source"] or pin != generation["source"]:
        raise RuntimeError("Source/metadata pin mismatch")
    reference_pin = subprocess.check_output(["git", "-C", str(reference), "rev-parse", "HEAD"], text=True).strip()
    if reference_pin != pin or load(reference/"generated/recompile_stamp.json") != generation["stamp"]:
        raise RuntimeError("Reference checkout/generation stamp mismatch")
    sources = {x["path"]:x for x in load(ROOT/"docs/discovery/source_files.json")}
    records = []
    for r in simd["records"]:
        data = (ROOT/"upstream"/r["file"]).read_bytes().replace(b"\r\n", b"\n")
        if sha(data) != sources[r["file"]]["sha256"]:
            raise RuntimeError(f"Source inventory changed: {r['file']}")
        kind, task, reason = ownership(r["file"])
        owner_kind, owner = owner_resolution(r, data.decode().splitlines())
        records.append({**r, "scope":kind, "task":task, "reason":reason,
                        "resolved_owner_kind":owner_kind, "resolved_owner":owner,
                        "resolved_operations":{k:family(k,v) for k,v in r["operations"].items()}})
    if len(records) != 661:
        raise RuntimeError("Expected every 661 original lexical site")
    save(output/"core_simd_scope.json", {"source":pin,"count":len(records),
        "counts":dict(Counter(r["scope"] for r in records)),
        "owner_kinds":dict(Counter(r["resolved_owner_kind"] for r in records)),
        "limitation":"Ownership and declaration/macro resolution are not numeric proof; AVX2 guarded code is not enabled by ARM flags",
        "records":records})

    elf = (reference/"build/elf/SLUS_215.90").read_bytes()
    if hashlib.sha1(elf).hexdigest() != generation["elf_sha1"]:
        raise RuntimeError("ELF identity mismatch")
    sections = elf_sections(elf)
    tables = reference_tables(private/"coverage-pcsx2-opcodes.cpp")
    registration = (reference/"generated/ee/register_functions.cpp").read_text()
    table_base = int(re.search(r"g_ps2RecompiledFunctionTableBase\s*=\s*(0x[0-9a-fA-F]+)",registration)[1],16)
    assignments = {int(i):symbol for i,symbol in re.findall(r"g_ps2RecompiledFunctionTable\[(\d+)\] = (\w+);",registration)}
    events = []
    for line in (reference/"build/recomp.log").read_text(errors="replace").splitlines():
        m = re.search(r"\[(error|warning)\] ([\w-]+) function=(\w+) addr=(0x[0-9a-fA-F]+) - (.*)",line)
        if not m:
            continue
        level,category,owner,addr,message=m.groups(); address=int(addr,16)
        section=next((s for s in sections if s["address"]<=address<s["address"]+s["size"]),None)
        e={"level":level,"category":category,"owner":owner,"address":addr,
           "section":section["name"] if section else None,
           "section_executable":bool(section and section["flags"]&4),
           "registered_owner":assignments.get((address-table_base)//4) if address>=table_base else None,
           "execution_reachability":"not established; section/registration is not execution evidence"}
        if level=="error":
            match=re.search(r"Unhandled (SPECIAL instruction|COP2 format|COP0 CO-OP|COP0 instruction format): (0x[0-9A-Fa-f]+)",message)
            if not match:
                raise RuntimeError("Unhandled diagnostic family needs explicit classification")
            label,selector=match[1],int(match[2],16); dispatch=tables[TABLES[label]][selector]
            if "Unknown" not in dispatch:
                raise RuntimeError(f"Recognized missing opcode: {label} selector={selector} {dispatch}")
            e.update(family=label,selector_index=selector,reference_dispatch=dispatch,
                     classification="reserved_or_unknown_R5900_encoding",
                     emitted_behavior="explicit std::runtime_error; no invented guest semantics")
        else:
            sites=re.findall(r"0x[0-9a-fA-F]+",message.split(";",1)[0])
            promoted=re.search(r"promoted (\d+) fallback entries",message)
            e.update(classification="indirect_control_flow_with_registered_fallback_entries",
                     indirect_sites=sites,promoted_entries=int(promoted[1]) if promoted else None,
                     actual_runtime_targets="not observed by this static audit")
        events.append(e)
    if Counter(e["level"] for e in events) != Counter(error=561,warning=1196):
        raise RuntimeError("Diagnostic event count mismatch")
    original_events=[(e["level"],e["category"],e["function"],e["address"]) for e in generation["diagnostic_events"]]
    actual_events=[(e["level"],e["category"],e["owner"],e["address"]) for e in events]
    if original_events != actual_events:
        raise RuntimeError("Diagnostic identities differ from the original metadata")
    save(output/"core_diagnostic_scope.json", {"source":pin,"elf_sha1":generation["elf_sha1"],
        "reference":{"repository":"https://github.com/PCSX2/pcsx2", "commit":PCSX2_COMMIT,
            "file":"pcsx2/R5900OpcodeTables.cpp","sha256":PCSX2_HASH},
        "counts":dict(Counter(e["classification"] for e in events)),
        "distinct_diagnostics_by_owner_and_first_address":{
            level:len({(e["owner"],e["address"]) for e in events if e["level"]==level})
            for level in ("error","warning")},
        "unhandled_family_counts":dict(Counter(e["family"] for e in events if e["level"]=="error")),
        "limitations":["All first diagnostic addresses are inside ELF .text; no data/unreachable classification is claimed",
            "Reserved selectors are not missing supported EE instructions in the pinned comparison",
            "Retain illegal-input failure; actual game reachability and indirect targets need integration traces",
            "M02 synthetic tests install their own public callbacks; they do not call these game owner bodies"],
        "events":events})
    sdk=sdk_metadata(private)
    parser_records=[]
    for gap in load(ROOT/"docs/discovery/parser_gaps.json"):
        path=gap["file"]
        if "/ps2xRecomp/" in path:
            scope,proof="host_generator","M02-02 native recompiler/analyzer build"
        elif path.endswith("ps2xRuntime/src/main.cpp"):
            scope,proof="excluded_desktop_runner","M02 supplies its own entry; revisit M04-02"
        elif "/ps2xRuntime/include/" in path:
            scope,proof="target_core_header","SDK/native and ARM compiler probes include all relevant gap-bearing runtime headers"
        elif "/ps2xRuntime/src/lib/" in path or "/ps2xIOP/" in path:
            scope,proof="target_core_source","M02 runtime-arm64-portability-v2.log full runtime/IOP compilation; not execution proof"
        elif path.startswith("src/debug/"):
            scope,proof="later_milestone","M04-07 desktop facilities; M09 UI where applicable"
        elif path.startswith("src/game/"):
            scope,proof="later_milestone","M04-02/M06-01/M09-01 game integration, world and UI"
        elif path.startswith("src/platform/"):
            scope,proof="later_milestone","M04-01 SDL2 platform adapter; desktop SDL3 is excluded"
        elif path.startswith("src/rhi/"):
            scope,proof="optional_backend","M05 preserves desktop D3D/Vulkan references; Switch uses a separate OpenGL adapter"
        elif path.startswith("src/render/"):
            scope,proof="later_milestone","M05 renderer/presentation integration"
        else:
            raise RuntimeError(f"Unclassified parser gap: {path}")
        parser_records.append({**gap,"scope":scope,"proof":proof})
    save(output/"core_parser_scope.json",{"source":pin,"count":len(parser_records),
        "counts":dict(Counter(x["scope"] for x in parser_records)),
        "limitation":"A successful compiler handles these constructs; the original syntactic index still is not a semantic call graph",
        "records":parser_records})
    source_calls=load(ROOT/"docs/discovery/runtime_call_macros.json")
    for macro,kind in (("PS2_SYSCALL_LIST","syscalls"),("PS2_STUB_LIST","stubs")):
        original=sorted(set(x["name"] for x in source_calls if x["macro"]==macro))
        if original != sdk["arm"][kind]:
            raise RuntimeError(f"Preprocessed declarations disagree with macro inventory: {macro}")
    configured=[]
    for entry in load(ROOT/"docs/discovery/recompiler_config.json")["general"]["stubs"]:
        name,addr=entry.rsplit("@",1)
        symbol=assignments.get((int(addr,16)-table_base)//4)
        if symbol is None:
            raise RuntimeError(f"Configured stub lacks registered wrapper: {entry}")
        configured.append({"name":name,"address":addr,"registered_wrapper":symbol})
    save(output/"core_sdk_catalog.json",{"source":pin,"architectures":sdk,
        "source_macro_counts":dict(Counter(x["macro"] for x in source_calls)),
        "configured_guest_stubs":configured,"configured_guest_stub_count":len(configured),
        "limitations":["Static assertions prove all expanded SDK declaration types on native/ARM, not every SDK's behavior",
            "Configured game wrappers use PS2_GUEST_CC; SDK host callbacks use the host convention",
            "Full registration/runtime linking and target execution remain separately required"]})
    print(f"PASS metadata audit: {len(records)} SIMD sites, {len(events)} diagnostics, "
          f"{len(sdk['arm']['syscalls'])} syscalls/{len(sdk['arm']['stubs'])} SDK stubs; {len(configured)} configured guest wrappers")


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference",type=Path,required=True)
    parser.add_argument("--private",type=Path,default=ROOT/"build/evidence/M02/coverage")
    parser.add_argument("--output",type=Path,default=ROOT/"docs/milestones/M02")
    parser.add_argument("--compile-probes",action="store_true")
    args=parser.parse_args()
    private=args.private.resolve()
    if not private.is_relative_to((ROOT/"build").resolve()):
        raise SystemExit("Private proof must stay below build/")
    private.mkdir(parents=True,exist_ok=True)
    if args.compile_probes:
        compile_probes(args.reference.resolve(),private)
    audit(args.reference.resolve(),private,args.output.resolve())
