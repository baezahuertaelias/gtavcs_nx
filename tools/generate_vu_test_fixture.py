"""Generate public synthetic VU tests with the pinned upstream VU recompiler.

No disc, game ELF or uploaded game microcode is read. A minimal synthetic ELF
and uploads list are constructed from the instruction encodings below.
"""
import argparse
import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PIN = "ecf150e53f87e4656dfe81eacd922839d952b282"
GENERATOR_SHA256 = "9b9f8fa3b0da2b451940ff561373da3c13d630b705e8c9a11c11f191dd9a9725"
EFFECTIVE_GENERATOR_SHA256 = "fea83a1165ce07a00a30de806f1ee80497a0fb6318cd085d44f7cbeac637de31"
POLICY_PATCH_SHA256 = "304d5810dde7960671dae5b2245ce52ae296ed5376b288a1bd2ed4552afe2910"
OUTPUT = ROOT / "tests/core/synthetic_vu_fixture.inc"
NOP = 0x000002ff


def upper(op, fd=0, fs=0, ft=0, dest=15):
    return dest << 21 | ft << 16 | fs << 11 | fd << 6 | op


def special(op, source=0, target=0, dest=0):
    return 0x40 << 25 | dest << 21 | target << 16 | source << 11 | (op & 0x7c) << 4 | (op & 3) | 0x3c


def upper_special(op, source=0, target=0, dest=15):
    return dest << 21 | target << 16 | source << 11 | (op & 0x7c) << 4 | (op & 3) | 0x3c


def iadd(target, source, immediate):
    assert 0 <= immediate <= 0x7ff
    return 0x08 << 25 | target << 16 | source << 11 | immediate


def sq(source, qword):
    return 1 << 25 | 15 << 21 | source << 11 | qword


def ended(pairs):
    return pairs + [(0, NOP | 0x40000000), (0, NOP)]


def programs():
    # Names/addresses are a public test ABI consumed by compiled_vu_tests.cpp.
    return [
        ("arithmetic", 0x000, ended([
            (0, upper(0x29, 3, 1, 2)),       # MADD vf3, vf1, vf2 (ACC=-1)
            (0, upper(0x2a, 4, 1, 2)),       # MUL vf4, vf1, vf2
            (0, upper(0x28, 5, 4, 6)),       # ADD vf5, vf4, vf6 (-1)
            (sq(3, 4), NOP),
            (sq(5, 5), NOP),
            (0, upper(0x2b, 8, 1, 2)),       # MAX
            (0, upper(0x2f, 9, 1, 2)),       # MINI
            (0, upper_special(0x1f, 10, 11, 14)), # CLIP xyz vf10,vf11.w
            (0, upper(0x2d, 14, 1, 2)),      # MSUB vector destination
            (0, upper_special(0x29, 1, 2)),   # MADDA accumulator destination
            (0, upper_special(0x2d, 1, 2)),   # MSUBA accumulator destination
            (0, NOP), (0, NOP), (0, NOP),
        ])),
        ("branch", 0x100, [
            (iadd(1, 0, 3), NOP),
            (0x09 << 25 | 1 << 16 | 1 << 11 | 1, NOP), # ISUBIU vi1,vi1,1
            (0, NOP),
            (0x29 << 25 | 1 << 11 | ((-3) & 0x7ff), NOP), # IBNE vi1,vi0,loop
            (iadd(2, 2, 1), NOP),             # branch delay executes three times
            (iadd(3, 0, (0x100 + 8 * 8) // 8), NOP),
            (0x24 << 25 | 3 << 11, NOP),     # JR uses the just-written VI
            (iadd(4, 4, 1), NOP),             # JR delay
            (0, NOP | 0x40000000),
            (sq(1, 6), NOP),                 # E-bit delay
        ]),
        ("q_p", 0x200, ended([
            (special(0x38, 10, 11), NOP),     # DIV Q,vf10.x,vf11.x
            (special(0x3b), NOP),             # WAITQ
            (0, upper(0x1c, 12, 10, 0)),      # MULq vf12,vf10,Q
            (special(0x70, 10), NOP),         # ESADD P,vf10 (actual generic lower helper)
            (special(0x7b), NOP),             # WAITP
            (special(0x64, 0, 13, 15), NOP),  # MFP vf13,P
            (sq(12, 8), NOP),
            (sq(13, 9), NOP),
        ])),
        ("xgkick", 0x300, ended([
            (sq(10, 13), NOP),
            (special(0x6c, 7), NOP),          # XGKICK at qword12; pending store is payload
            (0, NOP), (0, NOP), (0, NOP),
        ])),
        ("identity", 0x400, [
            (iadd(1, 0, 3), NOP),
            (0, NOP | 0x40000000),
            (iadd(2, 0, 7), NOP),
        ]),
        ("fdiv_flags", 0x500, ended([
            (special(0x38, 10, 11, 4), NOP),  # DIV Q,vf10.x,vf11.y=0
            (special(0x3b), NOP),             # WAITQ commits current/sticky D
        ])),
    ]


def render(generator):
    upstream = ROOT / "upstream"
    revision = subprocess.check_output(["git", "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip()
    if revision != PIN:
        raise RuntimeError("Pinned upstream changed; re-audit the fixture generator")
    pristine = upstream / "tools/vu1_recomp.py"
    if hashlib.sha256(pristine.read_bytes()).hexdigest() != GENERATOR_SHA256:
        raise RuntimeError("Upstream VU generator content changed")
    policy = ROOT / "patches/upstream/0003-vu-compiled-numeric-parity.patch"
    if hashlib.sha256(policy.read_bytes()).hexdigest() != POLICY_PATCH_SHA256:
        raise RuntimeError("Maintained strict VU policy patch identity changed")
    source = generator.resolve()
    if source.name != "vu1_recomp.py" or hashlib.sha256(source.read_bytes()).hexdigest() != EFFECTIVE_GENERATOR_SHA256:
        raise RuntimeError("Explicit generator is not the audited strict overlaid snapshot")
    spec = importlib.util.spec_from_file_location("nx_public_vu_generator", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    public = programs()
    payload = b"".join(struct.pack("<II", lower, high) for _, _, pairs in public for lower, high in pairs)
    header = struct.pack("<16sHHIIIIIHHHHHH", b"\x7fELF\x01\x01\x01" + bytes(9),
                         2, 8, 1, 0x100000, 52, 0, 0, 52, 32, 1, 0, 0, 0)
    segment = struct.pack("<8I", 1, 0x100, 0x100000, 0, len(payload), len(payload), 5, 16)
    elf = (header + segment).ljust(0x100, b"\0") + payload
    build = ROOT / "build/m02"
    build.mkdir(parents=True, exist_ok=True)
    if not build.resolve().is_relative_to((ROOT / "build").resolve()) or (ROOT / "build").is_symlink():
        raise RuntimeError("Synthetic fixture temporary directory must stay inside this workspace build tree")
    with tempfile.TemporaryDirectory(prefix="public-vu-fixture-", dir=build) as temporary:
        directory = Path(temporary)
        elf_path = directory / "public-synthetic-vu.elf"
        elf_path.write_bytes(elf)
        upload_path = directory / "public-uploads.txt"
        cursor, lines = 0x100000, []
        for _, address, pairs in public:
            lines.append(f"{cursor:x} {address:x} {len(pairs)}")
            cursor += len(pairs) * 8
        upload_path.write_text("\n".join(lines) + "\n")
        module.ELF, module.OUT, module.UPLOADS = str(elf_path), str(directory / "fixture.cpp"), str(upload_path)
        with contextlib.redirect_stdout(io.StringIO()):
            module.main()
        text = Path(module.OUT).read_text()
    lines = text.splitlines()
    lines[0] = "// SPDX-License-Identifier: GPL-3.0"
    lines.insert(1, "// Public SYNTHETIC VU instruction tests, generated by tools/generate_vu_test_fixture.py.")
    lines.insert(2, "// Uses the pinned upstream GPL-3.0 VU generator; no disc/ELF/game microcode input.")
    lines.insert(3, f"// Upstream pin {PIN}; generator SHA256 {GENERATOR_SHA256}.")
    lines.insert(4, f"// Effective generator SHA256 {EFFECTIVE_GENERATOR_SHA256}; strict policy patch SHA256 {POLICY_PATCH_SHA256}.")
    text = "\n".join(lines) + "\n"
    # Instrument the real emitted functions. No expected-result implementation is
    # substituted: every instruction/stall/branch below remains generator output.
    instrumentation = '''namespace
{
static uint64_t nxSyntheticVuCompiledCalls = 0;
static uint64_t nxSyntheticVuCompiledPairs = 0;
struct NxSyntheticVuPairCounter {
    uint64_t &pairs; uint64_t start;
    ~NxSyntheticVuPairCounter() { nxSyntheticVuCompiledPairs += pairs-start; }
};
'''
    text = text.replace("namespace\n{\n", instrumentation, 1)
    text, count = re.subn(r"(static Vu1CompiledResult vu1c_p\d+_c\d+(?:_t)?\(VU1Interpreter &vu, uint64_t budgetEnd, uint64_t &pairsRun\)\n\{)",
                         r"\1\n    ++nxSyntheticVuCompiledCalls;\n    NxSyntheticVuPairCounter nxCounter{pairsRun,pairsRun};", text)
    if count != 2 * len(public):
        raise RuntimeError("Unexpected emitted function structure")
    # Keep the effective generator's exact registrar policy, but invoke it
    # explicitly after the test's scoped fast-mode environment is established.
    pattern = r"struct Vu1CompiledRegistrar\n\{\n    Vu1CompiledRegistrar\(\)\n    \{\n(.*?)\n    \}\n\} vu1c_registrar;"
    text, count = re.subn(pattern, lambda match: "static void nxRegisterSyntheticVuPieces()\n{\n" + match.group(1) + "\n}",
                         text, flags=re.S)
    if count != 1:
        raise RuntimeError("Unexpected generated registrar structure")
    for number, (name, _, _) in enumerate(public):
        text = re.sub(rf'VU1 program {number} \(ELF 0x[0-9a-f]+\)', f'NX public synthetic {name}', text)
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generator", required=True, type=Path,
                        help="Explicit audited maintained-overlay tools/vu1_recomp.py snapshot")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = render(args.generator)
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text() != expected:
            raise RuntimeError("Public synthetic VU fixture is stale; regenerate and inspect")
        print("PASS public synthetic VU fixture reproduces exactly")
    else:
        OUTPUT.write_text(expected, encoding="utf-8", newline="\n")
        print(f"Generated {len(programs())} public synthetic VU programs; {len(expected.splitlines())} lines")


if __name__ == "__main__":
    main()
