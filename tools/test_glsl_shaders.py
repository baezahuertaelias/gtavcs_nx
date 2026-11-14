"""Tests of tools/glsl_shaders.py: every upstream shader variant has a GLSL twin that compiles (GLSL 330 core)."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import glsl_shaders  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "upstream"


@unittest.skipUnless((UPSTREAM / "src/render/CMakeLists.txt").is_file(), "upstream submodule not checked out")
class GlslShadersTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def generate(self):
        out = self.tmp / "glsl/gs_shaders_glsl.h"
        manifest = self.tmp / "manifest.json"
        subprocess.run([sys.executable, ROOT / "tools/glsl_shaders.py", "--upstream", UPSTREAM, "--out", out,
                        "--manifest", manifest], check=True, capture_output=True)
        return out.read_text(), json.loads(manifest.read_text())["variants"]

    def test_every_upstream_variant(self):
        found = glsl_shaders.variants((UPSTREAM / "src/render/CMakeLists.txt").read_text())
        names = [v[3] for v in found]
        self.assertEqual(len(names), 39)
        self.assertEqual(len(set(names)), 39)
        # what upstream's gs_shaders.cpp uses for the D3D11 / Vulkan backends is what the generator made
        used = set(re.findall(r"\{(k\w+?)(?:_spv)?, sizeof", (UPSTREAM / "src/render/gs_shaders.cpp").read_text()))
        self.assertEqual(used, set(names))
        header, manifest = self.generate()
        for name in names:
            self.assertIn(f"static const char {name}_glsl[]", header)
        self.assertEqual({v["name"] for v in manifest}, set(names))

    def test_unknown_entry_fails(self):
        cmake = "vcs_shader(shaders/present.hlsl vs_nothing vs_5_0 kX x.h)\n"
        with self.assertRaises(RuntimeError):
            for stem, entry, profile, name, header, defines in glsl_shaders.variants(cmake):
                glsl_shaders.source(stem, entry, profile, defines)

    @unittest.skipUnless(shutil.which("glslangValidator"), "glslangValidator not installed")
    def test_variants_compile(self):
        _, manifest = self.generate()
        header = (self.tmp / "glsl/gs_shaders_glsl.h").read_text()
        blocks = dict(re.findall(r"static const char (\w+)_glsl\[\] =\n(.*?);\n", header, re.S))
        for variant in manifest:
            parts = re.findall(r'"((?:[^"\\]|\\.)*)"', blocks[variant["name"]])
            text = "".join(bytes(p, "utf-8").decode("unicode_escape") for p in parts)
            for zero_to_one in (0, 1):
                path = self.tmp / f"{variant['name']}_{zero_to_one}.{'vert' if variant['stage'] == 'vertex' else 'frag'}"
                path.write_text(f"#version 330 core\n#define RHI_ZERO_TO_ONE {zero_to_one}\n" + text)
                result = subprocess.run(["glslangValidator", str(path)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, f"{variant['name']}: {result.stdout[-600:]}")


if __name__ == "__main__":
    unittest.main()
