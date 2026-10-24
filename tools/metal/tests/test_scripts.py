import csv
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class PerfTests(unittest.TestCase):
    def setUp(self):
        self.perf = load("perf_compare")
        self.spec = json.loads((ROOT / "docs/metal/benchmarks/scenarios.json").read_text())

    def test_good_result(self):
        # Synthetic tool test only; MUST NOT be presented as actual Metal performance.
        names = [s["id"] for s in self.spec["scenarios"]]
        ref = {name: [20.0] * 300 for name in names}
        new = {name: [15.0] * 300 for name in names}
        result = self.perf.compare(ref, new, self.spec)
        self.assertTrue(result["passes_target"])
        self.assertEqual(4, result["qualifying_scenes"])

    def test_regression_fails(self):
        names = [s["id"] for s in self.spec["scenarios"]]
        ref = {name: [20.0] * 300 for name in names}
        new = {name: [15.0] * 300 for name in names}
        new[names[0]] = [23.0] * 300
        self.assertFalse(self.perf.compare(ref, new, self.spec)["passes_target"])

    def test_missing_or_short_samples_fail(self):
        names = [s["id"] for s in self.spec["scenarios"]]
        ref = {name: [20.0] * 299 for name in names}
        new = {name: [15.0] * 300 for name in names}
        result = self.perf.compare(ref, new, self.spec)
        self.assertFalse(result["passes_target"])
        self.assertEqual(4, len(result["insufficient_sample_scenes"]))

    def test_invalid_csv_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "input.csv"
            with path.open("w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["scenario", "frame_ms"])
                writer.writerow(["quiet-region", "nan"])
            with self.assertRaisesRegex(ValueError, "nonpositive/nonfinite"):
                self.perf.load_frames(path)


class AuditTests(unittest.TestCase):
    def test_gl_detected_and_absent_after_rewrite(self):
        module = load("audit_gl")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "indra/llrender"
            src.mkdir(parents=True)
            p = src / "renderer.cpp"
            p.write_text("void render(){ glBindFramebuffer(GL_FRAMEBUFFER, 0); }\n")
            self.assertGreater(module.audit(root)["finding_count"], 0)
            p.write_text("void render(){ encodeMetalCommands(); }\n")
            self.assertEqual(module.audit(root)["finding_count"], 0)


class HarnessTests(unittest.TestCase):
    def test_initial_harness_contract(self):
        module = load("validate_harness")
        self.assertEqual([], module.validate(ROOT))
        # Release must not pass when nothing is validated.
        self.assertTrue(module.validate(ROOT, release=True))


if __name__ == "__main__":
    unittest.main()
