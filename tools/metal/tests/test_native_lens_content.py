import contextlib
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[3]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(
        name, ROOT / "scripts/content_tools" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class LensDirtModelTests(unittest.TestCase):
    def test_density_equal_to_hash_keeps_the_centered_disc(self):
        dirt = load_tool("check_lens_dirt")
        coordinate = dirt.np.zeros((1, 1))
        with mock.patch.object(dirt, "hash21", side_effect=lambda x, y, seed: dirt.np.full_like(x, 0.5)), \
             mock.patch.object(dirt, "hash22", side_effect=lambda x, y, seed: (dirt.np.zeros_like(x), dirt.np.zeros_like(y))):
            result = dirt.motes(coordinate, coordinate, 1.0, 0.1, 0.1,
                                0.5, 7.0, 1.0, 0.5, 1.0)
        # GLSL/MSL step(hash, density) keeps equality. Only the central
        # grid disc reaches this coordinate; its smoothstep value is one.
        self.assertEqual(float(result[0, 0]), 1.0)

    def test_mote_scale_keeps_radius_in_the_same_cell_units(self):
        dirt = load_tool("check_lens_dirt")
        captured = {}
        for scale in (0.5, 1.0, 2.0):
            calls = []

            def capture(u, v, *parameters):
                calls.append(parameters)
                return dirt.np.zeros_like(u)

            with mock.patch.object(dirt, "motes", side_effect=capture):
                dirt.build(16, 16, 7.0, 1.0, scale, 0.5, 0, 1.6, 1.0)
            captured[scale] = calls

        # Changing mote size moves spacing and radius together in the original
        # shader and MSL. Compare the actual distance fields at corresponding
        # coordinates; this property applies to each mote layer, not the whole
        # plate's independent noise, fibres and vignette.
        v, u = dirt.np.meshgrid(
            dirt.np.linspace(0.01, 0.99, 24),
            dirt.np.linspace(0.01, 0.99, 24), indexing="ij")
        for layer in range(3):
            reference = dirt.motes(u, v, *captured[1.0][layer])
            self.assertGreater(float(reference.max()), 0.0)
            for scale in (0.5, 2.0):
                with self.subTest(layer=layer, scale=scale):
                    scaled = dirt.motes(
                        u * scale, v * scale, *captured[scale][layer])
                    # The shader's additive 1e-9 in each disc denominator
                    # remains in UV units. Bound its contribution through
                    # smoothstep's maximum slope (1.5) and nine discs.
                    parameters = captured[1.0][layer]
                    epsilon_bound = (9 * 1.5 * 1e-9 * abs(1 / scale - 1) /
                                     (parameters[1] * parameters[3]))
                    dirt.np.testing.assert_allclose(
                        scaled, reference, rtol=0.0, atol=epsilon_bound)


class NativeLensSourceTests(unittest.TestCase):
    def setUp(self):
        self.flare = load_tool("check_lens_flare_state")
        self.dirt = load_tool("check_lens_dirt")
        self.source = self.flare.SHADER.read_text()

    def verify(self, module, source):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "native shader source.metal"
            path.write_text(source)
            with contextlib.redirect_stdout(io.StringIO()):
                return module.verify_port(path)

    def mutate(self, name, before, after):
        begin = self.source.index(name + "(")
        self.assertIn(before, self.source[begin:])
        return self.source[:begin] + self.source[begin:].replace(before, after, 1)

    def test_actual_native_source_and_comments(self):
        for module in (self.flare, self.dirt):
            with self.subTest(module=module.__name__):
                self.assertTrue(self.verify(module, self.source))
                self.assertTrue(self.verify(module, "// 999 is not a shader constant\n" + self.source))

    def test_default_source_path_survives_another_working_directory(self):
        previous = Path.cwd()
        with tempfile.TemporaryDirectory() as directory:
            try:
                os.chdir(directory)
                for module in (self.flare, self.dirt):
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertTrue(module.verify_port(module.SHADER))
            finally:
                os.chdir(previous)

    def test_missing_path_fails_honestly_in_api_and_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "absent.metal"
            for module in (self.flare, self.dirt):
                with self.subTest(module=module.__name__):
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertFalse(module.verify_port(missing))
                    result = subprocess.run(
                        [sys.executable, module.__file__, "--shader", str(missing)],
                        cwd=directory, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("port check FAILED", result.stdout)

    def test_missing_and_unterminated_native_functions_fail(self):
        functions = [(self.flare, "almetal_world_lens_flare_state_fragment"),
                     (self.dirt, "almetal_world_lens_dirt_fragment")]
        functions += [(self.dirt, "almetal_lens_dirt_" + name) for name in (
            "hash21", "hash22", "screen", "motes", "noise", "fbm3",
            "segment_distance", "lines")]
        for module, name in functions:
            with self.subTest(name=name):
                self.assertFalse(self.verify(module, self.source.replace(name, "missing_" + name)))
        begin = self.source.index("almetal_world_lens_flare_state_fragment(")
        self.assertFalse(self.verify(self.flare, self.source[begin:begin + 2000]))

    def test_native_dirt_helper_and_generator_constant_drift_fails(self):
        cases = (
            ("almetal_world_lens_dirt_fragment", "0.62", "0.63"),
            ("almetal_lens_dirt_hash21", "127.1", "127.2"),
            ("almetal_lens_dirt_hash22", "269.5", "269.6"),
            ("almetal_lens_dirt_screen", "1.0", "0.9"),
            ("almetal_lens_dirt_motes", "57.0", "58.0"),
            ("almetal_lens_dirt_noise", "3.0", "3.1"),
            ("almetal_lens_dirt_fbm3", "13.0", "14.0"),
            ("almetal_lens_dirt_segment_distance", "1.0e-9", "1.0e-8"),
            ("almetal_lens_dirt_lines", "6.2831853", "6.28"),
        )
        for name, before, after in cases:
            with self.subTest(name=name):
                self.assertFalse(self.verify(self.dirt, self.mutate(name, before, after)))

    def test_radius_operator_drift_is_not_hidden_by_equal_literals(self):
        source = self.mutate("almetal_world_lens_dirt_fragment",
                             "0.050 * mote_scale", "0.050")
        self.assertFalse(self.verify(self.dirt, source))

    def test_density_operator_drift_is_not_hidden_by_equal_literals(self):
        source = self.mutate(
            "almetal_lens_dirt_motes",
            "step(\n                almetal_lens_dirt_hash21(grid, seed + 57.0), density)",
            "(almetal_lens_dirt_hash21(grid, seed + 57.0) < density ? 1.0 : 0.0)")
        self.assertFalse(self.verify(self.dirt, source))

    def test_native_flare_spatial_temporal_and_sky_drift_fails(self):
        name = "almetal_world_lens_flare_state_fragment"
        for before, after in (("256u", "128u"), ("0.35", "0.36"),
                              ("depth <= 0.0", "depth < 0.0"),
                              ("slew * dt * reference", "slew * dt / reference")):
            with self.subTest(before=before):
                self.assertFalse(self.verify(self.flare, self.mutate(name, before, after)))


if __name__ == "__main__":
    unittest.main()
