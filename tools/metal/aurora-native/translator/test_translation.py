"""Regression for Naga's renamed entry points and exact embedded source bytes."""
from pathlib import Path
import re
import subprocess
import sys
import unittest
ROOT = Path(__file__).resolve().parents[4]
GENERATED = ROOT / "indra/almetal/shaders/aurora"
VENDOR = ROOT / "tools/metal/aurora-evaluation/vendor/aurora/shaders/fsr"
MODULES = ("prepare_inputs", "pyramids", "reactivity", "accumulate")

class TranslationTests(unittest.TestCase):
    def test_recorded_provenance_identifies_current_inputs_and_native_host(self):
        # Exercise the shipping provenance verifier: the original manifest
        # missed the accepted glass contract and diagnostic host changes.
        result = subprocess.run(
            [sys.executable, str(Path(__file__).with_name("manifest.py"))],
            cwd=ROOT, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_every_reflected_entry_names_a_real_metal_function(self):
        embedded = (GENERATED / "sources.h").read_text()
        common = (VENDOR / "common.wgsl").read_text()
        count = 0
        for module in MODULES:
            wgsl = common + "\n" + (VENDOR / (module + ".wgsl")).read_text()
            msl = (GENERATED / (module + ".metal")).read_text()
            entries = re.findall(r"@(vertex|fragment)\s+fn\s+(\w+)\s*\(", wgsl)
            self.assertGreater(len(entries), 1)
            for stage, original in entries:
                match = re.search(r"inline constexpr const char\* " + module + "_" + original + r' = "([^"]+)";', embedded)
                self.assertIsNotNone(match, (module, original))
                actual = match.group(1)
                self.assertRegex(msl, r"(?m)^" + stage + r"\s+\w+\s+" + re.escape(actual) + r"\(")
                count += 1
        self.assertEqual(count, 15)

    def test_existing_pyramid_collision_is_not_silently_assumed_identity(self):
        # These are actual pinned WGSL names whose Naga MSL names conflict with
        # helper-generated identifiers. The initial native port accidentally
        # created valid vertex-only PSOs for both, clearing the two pyramids.
        # Preserve this counterexample rather than testing only identity names.
        embedded = (GENERATED / "sources.h").read_text()
        for name in ("fs_farthest_depth_mip1", "fs_shading_change_mip0"):
            self.assertIn(f'inline constexpr const char* pyramids_{name} = "{name}_";', embedded)
            self.assertNotRegex((GENERATED / "pyramids.metal").read_text(), r"(?m)^fragment\s+\w+\s+" + name + r"\(")

    def test_embedded_sources_are_exact_generated_modules(self):
        embedded = (GENERATED / "sources.h").read_text()
        for module in MODULES:
            marker = f'inline constexpr const char* {module} = R"AURORA('
            source = embedded.split(marker, 1)[1].split(')AURORA";', 1)[0]
            self.assertEqual(source, (GENERATED / (module + ".metal")).read_text())

if __name__ == "__main__":
    unittest.main()
