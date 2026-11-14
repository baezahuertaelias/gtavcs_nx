"""Build and run the RHI conformance scenes on this host: upstream's Vulkan backend (the desktop reference) and the
port's OpenGL backend, then compare them (tools/rhi_compare.py).

    python tools/rhi_host_tests.py [--deps DIR] [--out build/m05/host] [--churn N] [--no-run]

--deps: upstream's FetchContent folder of a desktop reference build (volk, Vulkan-Headers, VulkanMemoryAllocator at
upstream's pinned tags; default build/m02/reference/build/release/_deps). Needs host clang/gcc, CMake, Ninja,
glslangValidator, EGL (Mesa) and a Vulkan driver (lavapipe on a headless host). No game data, no network.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from glsl_shaders import variants  # noqa: E402
from upstream_overlay import prepare  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


def run(args, **kw):
    print("+ " + " ".join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kw)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--deps", type=Path, default=ROOT / "build/m02/reference/build/release/_deps")
    ap.add_argument("--out", type=Path, default=ROOT / "build/m05/host")
    ap.add_argument("--churn", type=int, default=200)
    ap.add_argument("--no-run", action="store_true")
    a = ap.parse_args()
    out = a.out.resolve()
    source = ROOT / "build/m05/host-source"
    prepare(source)
    glslang = shutil.which("glslangValidator")
    if not glslang:
        print("FAIL glslangValidator not found", file=sys.stderr)
        return 1
    shaders = out / "shaders"
    (shaders / "spirv").mkdir(parents=True, exist_ok=True)
    cmake_text = (source / "src/render/CMakeLists.txt").read_text()
    for stem, entry, profile, name, header, defines in variants(cmake_text):
        target = shaders / "spirv" / header
        src = source / "src/render/shaders" / (stem + ".hlsl")
        if target.exists() and target.stat().st_mtime > src.stat().st_mtime:
            continue
        args = [sys.executable, "-I", source / "tools/hlsl_spirv.py", "--glslang", glslang,
                "--stage", "vert" if profile.startswith("vs") else "frag", "--entry", entry,
                "--name", name + "_spv", "--out", target]
        for d in defines:
            args += ["-D", d]
        run(args + [src], stdout=subprocess.DEVNULL)
    run([sys.executable, ROOT / "tools/glsl_shaders.py", "--upstream", source, "--out", shaders / "glsl/gs_shaders_glsl.h",
         "--manifest", out / "shader-manifest.json"])
    build = out / "build"
    deps = a.deps.resolve()
    run(["cmake", "-S", ROOT / "tests/rhi", "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         f"-DVCS_UPSTREAM_DIR={source}", f"-DVULKAN_HEADERS_DIR={deps / 'vulkanheaders-src/include'}",
         f"-DVOLK_DIR={deps / 'volk-src'}", f"-DVMA_DIR={deps / 'vulkanmemoryallocator-src/include'}",
         f"-DSHADER_HEADERS_DIR={shaders}", f"-DIMGUI_DIR={deps / 'imgui-src'}"], stdout=subprocess.DEVNULL)
    run(["cmake", "--build", build])
    if a.no_run:
        return 0
    env = dict(os.environ, VK_ICD_FILENAMES=os.environ.get("VK_ICD_FILENAMES", "/usr/share/vulkan/icd.d/lvp_icd.json"))
    status = 0
    for backend in ("vulkan", "opengl"):
        result = out / backend
        shutil.rmtree(result, ignore_errors=True)
        with open(out / f"{backend}.stderr.log", "w") as log:
            code = subprocess.run([build / "rhi_conformance", "--backend", backend, "--out", result, "--churn", str(a.churn)],
                                  env=env, stderr=log).returncode
        print(f"{backend}: exit {code}; checks {result / 'checks.tsv'}; log {out / (backend + '.stderr.log')}")
        status |= code != 0
    code = subprocess.run([sys.executable, ROOT / "tools/rhi_compare.py", out / "vulkan", out / "opengl",
                           "--report", out / "compare-host.json"]).returncode
    return 1 if status or code else 0


if __name__ == "__main__":
    raise SystemExit(main())
