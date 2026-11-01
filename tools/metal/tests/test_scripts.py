import csv
import importlib.util
import json
import plistlib
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
                      metal_submissions=None, metal_completed=None,
                      metal_timed=None, metal_gpu_ms=None):
        headers = ["scenario", "variant", "frame_ms", "frame_index",
                   "source_sha", "build_type", "drawable_width",
                   "drawable_height", "quality_profile", "native_exact"]
        if metal_submissions is not None:
            headers.append("metal_submissions")
        if metal_completed is not None:
            headers.append("metal_completed")
        if metal_timed is not None:
            headers.append("metal_timed")
        if metal_gpu_ms is not None:
            headers.append("metal_gpu_ms")
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
                    if metal_completed is not None:
                        row.append(metal_completed)
                    if metal_timed is not None:
                        row.append(metal_timed)
                    if metal_gpu_ms is not None:
                        row.append(metal_gpu_ms)
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
                baseline_path, "baseline", exact=0,
                metal_submissions=12, metal_completed=11,
                metal_timed=10, metal_gpu_ms=5.5)
            self.write_capture(
                candidate_path, "candidate", exact=1,
                metal_submissions=5, metal_completed=4,
                metal_timed=4, metal_gpu_ms=2.25)
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
            self.assertEqual("count_per_frame",
                             report["metal_completed"]["unit"])
            self.assertEqual(4,
                             report["metal_completed"]["scenes"][0]
                             ["candidate"]["p50"])
            self.assertEqual(10,
                             report["metal_timed"]["scenes"][0]
                             ["baseline"]["p50"])
            self.assertEqual("ms", report["metal_gpu_ms"]["unit"])
            self.assertEqual(2.25,
                             report["metal_gpu_ms"]["scenes"][0]
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


class CUAPreflightTests(unittest.TestCase):
    def make_app(self, root):
        app = root / "Alchemy Metal Development.app"
        macos = app / "Contents/MacOS"
        macos.mkdir(parents=True)
        executable = macos / "Alchemy Metal Development"
        executable.write_bytes(b"test viewer\n")
        executable.chmod(0o755)
        with (app / "Contents/Info.plist").open("wb") as stream:
            plistlib.dump(
                {
                    "CFBundleExecutable": "Alchemy Metal Development",
                    "CFBundleShortVersionString": "26.4.0.99999",
                },
                stream,
            )
        return app, executable

    def valid_snapshot(self, module, root):
        app, executable = self.make_app(root)
        identity, identity_errors = module.app_identity(app)
        self.assertEqual([], identity_errors)
        profile = root / "isolated-profile"
        log_path = profile / "logs/Alchemy.log"
        process = module.Process(
            42, 1, str(identity["executable"]) + " --multiple --noaudio"
        )
        environment = f"{process.command} ALCHEMYNEXT_USER_DIR={profile} TERM=xterm"
        open_files = [str(log_path)]
        log_text = (
            f"Started logging to {log_path}\n"
            "Version: Alchemy Metal Development 26.4.0.99999\n"
        )
        return identity, profile, process, environment, open_files, log_text

    def test_exact_isolated_snapshot_passes(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            self.assertEqual(
                [],
                module.validate_runtime_snapshot(
                    identity, profile, [process], environment, open_files, log_text
                ),
            )

    def test_stale_sibling_viewer_is_rejected(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            sibling = module.Process(
                43, 1,
                "/Applications/Alchemy Release.app/Contents/MacOS/Alchemy Release",
            )
            errors = module.validate_runtime_snapshot(
                identity,
                profile,
                [process, sibling],
                environment,
                open_files,
                log_text,
            )
            self.assertTrue(any("census" in error for error in errors))

    def test_ordinary_profile_or_missing_environment_is_rejected(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, _, open_files, log_text = values
            normal = Path.home() / "Library/Application Support/AlchemyNext/logs/Alchemy.log"
            errors = module.validate_runtime_snapshot(
                identity,
                profile,
                [process],
                process.command,
                open_files + [str(normal)],
                log_text,
            )
            self.assertTrue(any("isolated-profile root" in error for error in errors))
            self.assertTrue(any("ordinary Alchemy profile" in error for error in errors))

    def test_profile_root_inside_ordinary_tree_is_rejected(self):
        module = load("cua_preflight")
        identity = {
            "executable": Path("/tmp/Alchemy Metal Development.app/Contents/MacOS/Alchemy Metal Development"),
            "channel": "Alchemy Metal Development",
            "version": "26.4.0.99999",
        }
        profile = Path.home() / "Library/Application Support/AlchemyNext/validation"
        process = module.Process(42, 1, str(identity["executable"]))
        errors = module.validate_runtime_snapshot(
            identity,
            profile,
            [process],
            f"{process.command} ALCHEMYNEXT_USER_DIR={profile}",
            [str(profile / "logs/Alchemy.log")],
            (
                f"Started logging to {profile / 'logs/Alchemy.log'}\n"
                "Version: Alchemy Metal Development 26.4.0.99999\n"
            ),
        )
        self.assertTrue(any("profile root" in error for error in errors))


class GraphicsSettingsTests(unittest.TestCase):
    def setUp(self):
        self.module = load("graphics_settings")

    @staticmethod
    def write_settings(path, values):
        records = []
        for name, value in values.items():
            if isinstance(value, bool):
                encoded = f"<boolean>{str(value).lower()}</boolean>"
                value_type = "Boolean"
            elif isinstance(value, int):
                encoded = f"<integer>{value}</integer>"
                value_type = "S32"
            elif isinstance(value, float):
                encoded = f"<real>{value}</real>"
                value_type = "F32"
            else:
                encoded = f"<string>{value}</string>"
                value_type = "String"
            records.append(
                f"<key>{name}</key><map>"
                f"<key>Type</key><string>{value_type}</string>"
                f"<key>Value</key>{encoded}</map>"
            )
        path.write_text("<llsd><map>" + "".join(records) + "</map></llsd>")

    def complete_values(self):
        values = {name: 1 for name in self.module.REQUIRED_SETTINGS}
        values["RenderMirrors"] = True
        values["RenderTransparentWater"] = False
        return values

    def test_profile_overrides_defaults_and_ignores_backend_setting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            defaults = root / "defaults.xml"
            profile = root / "profile.xml"
            values = self.complete_values()
            values["RenderHeroProbeResolution"] = 256
            values["AlchemyExposureCompensation"] = -1.0
            values["RenderShaderCacheVersion"] = "reference-cache"
            values["RenderMetalNativeOnly"] = True
            self.write_settings(defaults, values)
            self.write_settings(
                profile,
                {
                    "RenderHeroProbeResolution": 512,
                    "AlchemyExposureCompensation": 0.5,
                    "RenderShaderCacheVersion": "candidate-cache",
                },
            )
            manifest = self.module.build_manifest([defaults], profile, "fixture")
            self.assertEqual(512, manifest["settings"]["RenderHeroProbeResolution"])
            self.assertEqual(0.5, manifest["settings"]["AlchemyExposureCompensation"])
            self.assertNotIn("RenderShaderCacheVersion", manifest["settings"])
            self.assertNotIn("RenderMetalNativeOnly", manifest["settings"])

    def test_visual_mismatch_fails_comparison(self):
        settings = self.complete_values()
        reference = {
            "schema_version": 1,
            "label": "reference",
            "settings": dict(settings),
        }
        candidate = {
            "schema_version": 1,
            "label": "candidate",
            "settings": dict(settings),
        }
        candidate["settings"]["RenderQualityPerformance"] = 0
        report = self.module.compare_manifests(reference, candidate)
        self.assertFalse(report["comparable"])
        self.assertEqual("RenderQualityPerformance", report["mismatches"][0]["setting"])

    def test_identical_visual_settings_pass(self):
        settings = self.complete_values()
        reference = {"label": "reference", "settings": dict(settings)}
        candidate = {"label": "candidate", "settings": dict(settings)}
        report = self.module.compare_manifests(reference, candidate)
        self.assertTrue(report["comparable"])
        candidate["settings"]["RenderFutureQualitySwitch"] = 7
        report = self.module.compare_manifests(reference, candidate)
        self.assertFalse(report["comparable"])
        self.assertEqual(
            ["RenderFutureQualitySwitch"], report["candidate_only"]
        )


class HarnessTests(unittest.TestCase):
    def test_initial_harness_contract(self):
        module = load("validate_harness")
        self.assertEqual([], module.validate(ROOT))
        # Release must not pass when nothing is validated.
        self.assertTrue(module.validate(ROOT, release=True))

    def test_stale_repository_command_path_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "AGENTS.md").write_text(
                "Run `tools/metal/verify-app.sh app`.\n", encoding="utf-8"
            )
            errors = module.validate_live_references(root, ["AGENTS.md"])
            self.assertTrue(any("verify-app.sh" in error for error in errors))

    def test_stale_document_relative_repository_path_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            skill = root / ".agents/skills/example/SKILL.md"
            skill.parent.mkdir(parents=True)
            skill.write_text(
                "Read `benchmarks/scenarios.json`.\n", encoding="utf-8"
            )
            errors = module.validate_live_references(
                root, [".agents/skills/example/SKILL.md"]
            )
            self.assertTrue(any(
                "benchmarks/scenarios.json" in error for error in errors
            ))

    def test_referenced_command_must_be_executable(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            script = root / "tools/metal/check.sh"
            script.parent.mkdir(parents=True)
            script.write_text("#!/bin/sh\n", encoding="utf-8")
            script.chmod(0o644)
            (root / "AGENTS.md").write_text(
                "Run `tools/metal/check.sh`.\n", encoding="utf-8"
            )
            errors = module.validate_live_references(root, ["AGENTS.md"])
            self.assertIn(
                "Referenced command is not executable: tools/metal/check.sh",
                errors,
            )

    def test_stale_markdown_link_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            guide = root / "docs/guide.md"
            guide.parent.mkdir(parents=True)
            guide.write_text("[missing](absent.md)\n", encoding="utf-8")
            errors = module.validate_live_references(root, ["docs/guide.md"])
            self.assertEqual(
                ["Stale Markdown link in docs/guide.md: absent.md"], errors
            )

    def test_release_invariant_pattern_loss_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            metal = root / "docs/metal"
            metal.mkdir(parents=True)
            (metal / "PRODUCT.md").write_text("immutable alpha\n", encoding="utf-8")
            (metal / "VALIDATION.md").write_text("coverage beta\n", encoding="utf-8")
            (metal / "release-invariants.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "source": "docs/metal/PRODUCT.md",
                        "invariants": [
                            {
                                "id": "fixture",
                                "source_patterns": ["alpha"],
                                "coverage": {
                                    "docs/metal/VALIDATION.md": ["gamma"]
                                },
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            errors = module.validate_release_invariants(root, required_ids=set())
            self.assertTrue(any("coverage missing" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
