"""Fail closed on wrong architecture, host dependencies or malformed NRO."""
from pathlib import Path
import json
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    build = ROOT / "build/switch"
    prefix = "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-"
    report = subprocess.check_output([prefix + "readelf", "-h", "-l", "-d", "-r", str(build / "gtavcs.elf")], text=True)
    assert "AArch64" in report and "ELF64" in report, "Not an AArch64 ELF64"
    assert "DYN (" in report, "Expected Switch position-independent ELF"
    assert "(NEEDED)" not in report and "INTERP" not in report, "Host dynamic dependency"
    nro = (build / "gtavcs.nro").read_bytes()
    nacp = (build / "gtavcs.nacp").read_bytes()
    assert nro[16:20] == b"NRO0" and len(nacp) == 0x4000, "Invalid NRO/NACP"
    nro_size = struct.unpack_from("<I", nro, 24)[0]
    assert nro[nro_size:nro_size + 4] == b"ASET", "Missing NRO assets"
    offset, size = struct.unpack_from("<QQ", nro, nro_size + 24)
    assert size == len(nacp) and nro[nro_size + offset:nro_size + offset + size] == nacp, "NACP mismatch"
    assert nacp.startswith(b"GTA VCS NX - platform smoke\0"), "Wrong metadata"
    commands = json.loads((build / "compile_commands.json").read_text())
    for entry in commands:
        command = entry["command"]
        assert "/opt/devkitpro/devkitA64/bin/" in command, "Host compiler"
        for include in re.findall(r"(?:-I|-isystem\s*)(/[^ ]+)", command):
            assert include.startswith(("/opt/devkitpro/", str(ROOT) + "/")), f"Host include {include}"
    linkmap = (build / "gtavcs.map").read_text()
    assert "core_stub" in linkmap and "libSDL2.a" in linkmap and "libEGL.a" in linkmap
    assert not re.search(r"lib(?:vulkan|X11|wayland|asound)\b", linkmap), "Forbidden backend"
    for archive in re.findall(r"(/[^\s()]+\.a)", linkmap):
        assert archive.startswith(("/opt/devkitpro/", str(ROOT) + "/")), f"Host archive {archive}"
    out = ROOT / "build/evidence/M01"
    out.mkdir(parents=True, exist_ok=True)
    (out / "readelf.txt").write_text(report)
    print("PASS AArch64 PIE ELF, no host loader/NEEDED, NRO/NACP, target compiler/includes/archives; stub only")


if __name__ == "__main__":
    main()
