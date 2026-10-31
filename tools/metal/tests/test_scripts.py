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

    def write_capture(self, path, variant, exact=1, width=2048,
                      metal_submissions=None):
        headers = ["scenario", "variant", "frame_ms", "frame_index",
                   "source_sha", "build_type", "drawable_width",
                   "drawable_height", "quality_profile", "native_exact"]
        if metal_submissions is not None:
            headers.append("metal_submissions")
        with path.open("w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            frame_index = 100
            for scene in (s["id"] for s in self.spec["scenarios"]):
                for _ in range(300):
                    row = [
                        scene, variant, 16.0, frame_index,
                        "0123456789abcdef0123456789abcdef01234567",
                        "RelWithDebInfo", width, 1280, 5, exact]
                    if metal_submissions is not None:
                        row.append(metal_submissions)
                    writer.writerow(row)
                    frame_index += 1

    def test_source_exact_capture_provenance(self):
        with tempfile.TemporaryDirectory() as d:
            baseline_path = Path(d) / "baseline.csv"
            candidate_path = Path(d) / "candidate.csv"
            self.write_capture(baseline_path, "baseline", exact=0)
            self.write_capture(candidate_path, "candidate", exact=1)
            baseline = self.perf.load_capture(
                baseline_path, require_provenance=True)
            candidate = self.perf.load_capture(
                candidate_path, require_provenance=True, candidate=True)
            result = self.perf.compare_provenance(
                baseline, candidate,
                [s["id"] for s in self.spec["scenarios"]])
            self.assertTrue(result["verified"])
            self.assertEqual(4, len(result["scenes"]))

    def test_nonexact_candidate_and_mismatched_display_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            baseline_path = Path(d) / "baseline.csv"
            candidate_path = Path(d) / "candidate.csv"
            self.write_capture(baseline_path, "baseline", exact=0)
            self.write_capture(candidate_path, "candidate", exact=0)
            with self.assertRaisesRegex(ValueError, "not an exact native"):
                self.perf.load_capture(
                    candidate_path, require_provenance=True, candidate=True)

            self.write_capture(candidate_path, "candidate", exact=1, width=1920)
            baseline = self.perf.load_capture(
                baseline_path, require_provenance=True)
            candidate = self.perf.load_capture(
                candidate_path, require_provenance=True, candidate=True)
            with self.assertRaisesRegex(ValueError, "drawable_width"):
                self.perf.compare_provenance(
                    baseline, candidate,
                    [s["id"] for s in self.spec["scenarios"]])

    def test_secondary_metal_submission_counts_are_reported_not_gated(self):
        with tempfile.TemporaryDirectory() as d:
            baseline_path = Path(d) / "baseline.csv"
            candidate_path = Path(d) / "candidate.csv"
            self.write_capture(
                baseline_path, "baseline", exact=0, metal_submissions=12)
            self.write_capture(
                candidate_path, "candidate", exact=1, metal_submissions=5)
            baseline = self.perf.load_capture(baseline_path)
            candidate = self.perf.load_capture(candidate_path, candidate=True)
            report = self.perf.compare_secondary_metrics(
                baseline["secondary_metrics"], candidate["secondary_metrics"],
                [s["id"] for s in self.spec["scenarios"]])
            self.assertEqual("count_per_frame",
                             report["metal_submissions"]["unit"])
            self.assertEqual(12,
                             report["metal_submissions"]["scenes"][0]
                             ["baseline"]["p50"])
            self.assertEqual(5,
                             report["metal_submissions"]["scenes"][0]
                             ["candidate"]["p50"])


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


class TraceSubmissionTests(unittest.TestCase):
    def test_resolves_compact_xctrace_references_and_sums_durations(self):
        module = load("trace_submissions")
        xml = b'''<?xml version="1.0"?>
<trace-query-result><node>
  <row><duration id="1" fmt="1 us">1000</duration>
    <duration id="2" fmt="0.75 us">750</duration>
    <narrative id="3" fmt='Committed &quot; Alpha &quot; with 1 encoders'/></row>
  <row><duration ref="1"/><duration ref="2"/><narrative ref="3"/></row>
  <row><duration id="4" fmt="2 us">2000</duration>
    <duration id="5" fmt="1.5 us">1500</duration>
    <narrative id="6" fmt='Committed &quot; Beta &quot; with 2 encoders'/></row>
  <row><duration id="7" fmt="1 us">1000</duration>
    <narrative id="8" fmt='Committed &quot; No Encoder &quot; with 0 encoders'/></row>
</node></trace-query-result>'''
        report = module.summarize_xml(xml, "fixture.xml")
        self.assertEqual(4, report["submission_count"])
        self.assertEqual(0.005, report["event_ms"])
        self.assertEqual(0.003, report["encoder_ms"])
        self.assertEqual("Alpha", report["labels"][0]["label"])
        self.assertEqual(2, report["labels"][0]["count"])
        self.assertEqual("Beta", report["labels"][1]["label"])
        self.assertEqual("No Encoder", report["labels"][2]["label"])
        self.assertEqual(1, report["labels"][2]["encoder_unreported"])
        self.assertIsNone(report["labels"][2]["mean_encoder_ms"])

    def test_rejects_xml_without_submission_rows(self):
        module = load("trace_submissions")
        with self.assertRaisesRegex(ValueError, "no committed"):
            module.summarize_xml(b"<trace-query-result/>")


class HarnessTests(unittest.TestCase):
    def test_initial_harness_contract(self):
        module = load("validate_harness")
        self.assertEqual([], module.validate(ROOT))
        # Release must not pass when nothing is validated.
        self.assertTrue(module.validate(ROOT, release=True))


if __name__ == "__main__":
    unittest.main()
