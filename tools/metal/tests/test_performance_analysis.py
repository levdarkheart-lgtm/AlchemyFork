"""Sanitized PPD-575/578/579/581/583 contracts; no viewer or recordings."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / "performance_analysis.py"
SPEC = importlib.util.spec_from_file_location("performance_analysis", SCRIPT)
analysis = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analysis)
FIELDS = tuple(f"value{index}" for index in range(8))


def metric(at, frame, value):
    return {"mach_ns": at, "frame_id": frame, "values": [value] + [0] * 7}


def report(name, qualified=True, reasons=None):
    return {"run": name, "qualified_for_comparison": qualified,
            "qualification_reasons": [] if reasons is None else reasons}


def bucket(start=0, value=4, count=10, variant=0, layer="layer-a", name="metric-a", duration=10):
    return {"start": start, "duration": duration, "value-in-ms": value,
            "number-of-frames": count, "metric-variant": variant, "name": name, "layer-id": layer}


def reply_histogram(samples=0, minimum=0, maximum=0, counts=None):
    bins = [0] * 32
    for slot, count in (counts or {}).items():
        bins[slot] = count
    return {"samples": samples, "min_ns": minimum, "max_ns": maximum, "bins_log2_us": bins}


class QualificationTests(unittest.TestCase):
    def test_analysis_exit_zero_does_not_qualify_false_measurement(self):
        failed = report("candidate", False, ["native_FRAME_gap_in_window"])
        failed["analysis_exit_code"] = 0
        with self.assertRaisesRegex(ValueError, "Unqualified measurement: candidate"):
            analysis.require_qualified([report("control"), failed], ["control", "candidate"])

    def test_literal_true_and_explicit_empty_reasons_required(self):
        cases = [report("control", 1), report("control", "true"),
                 report("control", None), {"run": "control", "qualified_for_comparison": True},
                 report("control", True, ["uncertain coverage"])]
        for value in cases:
            with self.subTest(value=value), self.assertRaises(ValueError):
                analysis.require_qualified([value], ["control"])

    def test_exact_distinct_run_set_and_original_order(self):
        reports = [report("candidate"), report("control")]
        self.assertEqual(analysis.require_qualified(reports, ["control", "candidate"]),
                         ["candidate", "control"])
        for values, expected in (([], ["control"]), ([report("other")], ["control"]),
                ([report("control"), report("control")], ["control"]),
                ([report("control")], ["control", "control"]), ([], [])):
            with self.subTest(values=values), self.assertRaises(ValueError):
                analysis.require_qualified(values, expected)


class MetricTests(unittest.TestCase):
    def test_zero_frame_tags_remain_three_observations(self):
        rows = [metric(1, 0, 10), metric(2, 0, 30), metric(3, 0, 50)]
        result = analysis.metric_distributions(rows, FIELDS)
        self.assertEqual(result["fields"]["value0"]["count"], 3)
        self.assertEqual(result["fields"]["value0"]["mean"], 30)
        self.assertEqual(result["zero_or_unqualified_frame_tag_rows"], 3)
        self.assertEqual(result["optional_first_positive_frame"]["distinct_positive_frame_tags"], 0)

    def test_repeated_conflicting_and_exact_duplicate_observations_are_primary(self):
        rows = [metric(1, 0, 10), metric(2, 0, 30), metric(3, 7, 100),
                metric(4, 7, 200), metric(4, 7, 200), metric(5, 8, 50)]
        before = copy.deepcopy(rows)
        result = analysis.metric_distributions(rows, FIELDS)
        primary, selected = result["fields"]["value0"], result["optional_first_positive_frame"]
        self.assertEqual(primary["count"], 6)
        self.assertAlmostEqual(primary["mean"], 590 / 6)
        self.assertEqual(primary["p50"], 75)
        self.assertEqual(result["exact_duplicate_payload_rows_retained"], 1)
        self.assertEqual(selected["positive_tagged_rows"], 4)
        self.assertEqual(selected["distinct_positive_frame_tags"], 2)
        self.assertEqual(selected["repeated_positive_tag_rows"], 2)
        self.assertEqual(selected["conflicting_positive_frame_observations"], 2)
        self.assertEqual(selected["conflicting_positive_frame_tags"], 1)
        self.assertEqual(selected["fields"]["value0"]["mean"], 75)
        self.assertNotIn("sum", primary)
        self.assertNotIn("sum", selected["fields"]["value0"])
        self.assertEqual(result["first"], rows[0])
        self.assertEqual(result["last"], rows[-1])
        self.assertEqual(rows, before)

    def test_absent_negative_boolean_and_missing_tags_are_not_identities(self):
        rows = [metric(1, None, 1), metric(2, -1, 2), metric(3, True, 3), metric(4, "7", 4)]
        rows.append({"mach_ns": 5, "values": [5] * 8})
        result = analysis.metric_distributions(rows, FIELDS)
        self.assertEqual(result["zero_or_unqualified_frame_tag_rows"], 5)
        self.assertEqual(result["fields"]["value0"]["count"], 5)
        self.assertEqual(result["optional_first_positive_frame"]["distinct_positive_frame_tags"], 0)

    def test_invalid_payloads_and_nonfinite_cells_are_not_numeric_samples(self):
        rows = [metric(1, 0, True), metric(2, 0, "5"), metric(3, 0, float("nan")),
                metric(4, 0, float("inf")), metric(5, 0, 9), {"values": [3]}, {"values": None}]
        result = analysis.metric_distributions(rows, FIELDS)
        self.assertEqual(result["all_observed_rows"], 7)
        self.assertEqual(result["invalid_value_payload_rows"], 2)
        self.assertEqual(result["fields"]["value0"]["count"], 1)
        self.assertEqual(result["fields"]["value0"]["mean"], 9)

    def test_empty_distribution_and_field_schema_validation(self):
        result = analysis.metric_distributions([], FIELDS)
        self.assertEqual(result["fields"]["value0"], dict(count=0, mean=None, min=None,
                                                        max=None, p50=None, p95=None, p99=None))
        for fields in (FIELDS[:7], ["same"] * 8, FIELDS[:7] + ("",)):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                analysis.metric_distributions([], fields)


class ContextTests(unittest.TestCase):
    def test_anonymous_physics_lambda_is_broad_not_exact_binder(self):
        names = ["_ZZN27ALMetalWorldGBufferRenderer24renderOpaquePostDeferredENK3$_2cl"
                 "I29ALMetalWorldPhysicsShapeBatch28ALMetalWorldBoundingBoxBatchEE"]
        contexts = analysis.cpu_contexts(names)
        self.assertTrue(contexts["broad_opaque_post_lambda_inclusive"])
        self.assertFalse(contexts["explicit_named_forward_binder_inclusive_under_opaque_post"])

    def test_only_explicit_name_sets_named_binder_context(self):
        contexts = analysis.cpu_contexts(["bind_forward_probe_constants()",
            "ALMetalWorldGBufferRenderer::renderOpaquePostDeferred()"])
        self.assertTrue(contexts["explicit_named_forward_binder_inclusive_under_opaque_post"])
        self.assertFalse(contexts["broad_opaque_post_lambda_inclusive"])
        self.assertFalse(analysis.cpu_contexts(["bind_forward_probe_constants()"])
                         ["explicit_named_forward_binder_inclusive_under_opaque_post"])

    def test_palette_hash_is_broad_and_repeated_inline_names_do_not_multiply_flag(self):
        names = ["std::__hash_memory", "std::__hash_memory",
                 "ALMetalWorldFrameData::publishPaletteRecord()",
                 "ALMetalWorldGBufferRenderer::renderOpaquePostDeferred()::$_1::operator()()"]
        contexts = analysis.cpu_contexts(names)
        self.assertIs(contexts["broad_hash_subtree_under_opaque_post"], True)
        self.assertTrue(contexts["broad_hash_leaf_without_publish_symbol"])
        self.assertFalse(contexts["frame_publish_inclusive_under_opaque_post"])
        self.assertFalse(contexts["old_strict_hash_publish_setLightingData_post_leaf"])

    def test_publish_ancestry_survives_removed_setter_symbol(self):
        contexts = analysis.cpu_contexts(["std::__hash_memory", "ALMetalWorldFrameData::publish()",
            "ALMetalWorldGBufferRenderer::renderOpaquePostDeferred()"])
        self.assertTrue(contexts["broad_hash_leaf_with_publish_ancestor"])
        self.assertFalse(contexts["old_strict_hash_publish_setLightingData_post_leaf"])
        self.assertFalse(any(analysis.cpu_contexts([]).values()))
        with self.assertRaises(ValueError):
            analysis.cpu_contexts([None])


class OverviewTests(unittest.TestCase):
    def test_actual_metric_variant_and_integer_association_weighting(self):
        rows = [bucket(value=2, count=10), bucket(start=10, value=8, count=30)]
        before = copy.deepcopy(rows)
        result = analysis.overview_distributions(rows, 0, 20)
        item = result["groups"][0]
        self.assertEqual(item["associated_frames_complete_buckets"], 40)
        self.assertEqual(item["frame_weighted_value_ms"], 6.5)
        self.assertEqual(result["invalid_rows"], 0)
        self.assertEqual(rows, before)
        wrong_column = bucket()
        wrong_column["variant"] = wrong_column.pop("metric-variant")
        with self.assertRaisesRegex(ValueError, "metric-variant"):
            analysis.overview_distributions([wrong_column], 0, 20)

    def test_families_variants_and_layers_never_pool(self):
        rows = [bucket(), bucket(variant=1), bucket(layer="layer-b"), bucket(name="metric-b")]
        groups = analysis.overview_distributions(rows, 0, 20)["groups"]
        self.assertEqual(len(groups), 4)
        self.assertEqual({(row["name"], row["variant_raw"], row["layer"]) for row in groups},
                         {("metric-a", 0, "layer-a"), ("metric-a", 1, "layer-a"),
                          ("metric-a", 0, "layer-b"), ("metric-b", 0, "layer-a")})

    def test_boundary_buckets_not_clipped_into_weighted_mean(self):
        rows = [bucket(start=0, value=100), bucket(start=10, value=4), bucket(start=20, value=200)]
        item = analysis.overview_distributions(rows, 5, 25)["groups"][0]
        self.assertEqual(item["overlap_buckets"], 3)
        self.assertEqual(item["complete_buckets"], 1)
        self.assertEqual(item["boundary_buckets_excluded_from_weighted_mean"], 2)
        self.assertEqual(item["frame_weighted_value_ms"], 4)
        self.assertEqual(item["complete_bucket_bounds_ns"], [10, 20])

    def test_zero_associations_remain_observations_and_invalid_rows_explicit(self):
        rows = [bucket(value=0, count=0), bucket(value=2, count=0), bucket(count=True),
                bucket(value=float("inf")), bucket(duration=0), bucket(variant="unknown")]
        result = analysis.overview_distributions(rows, 0, 20)
        self.assertEqual(result["invalid_rows"], 5)
        self.assertEqual(result["groups"][0]["complete_buckets"], 1)
        self.assertEqual(result["groups"][0]["positive_overlap_buckets"], 0)
        self.assertIsNone(result["groups"][0]["frame_weighted_value_ms"])

    def test_outer_bounds_are_not_gapless_coverage_or_variant_semantics(self):
        rows = [bucket(start=0, variant="9"), bucket(start=90, variant="9")]
        item = analysis.overview_distributions(rows, 0, 100)["groups"][0]
        self.assertTrue(item["outer_bounds_contain_window"])
        self.assertEqual(item["variant_raw"], 9)
        self.assertNotIn("coverage_contains_fixed_window", item)
        self.assertNotIn("occupancy", item)
        for window in ((0, 0), (10, 0), (-1, 1), (True, 10)):
            with self.subTest(window=window), self.assertRaises(ValueError):
                analysis.overview_distributions([], *window)


class ReplyHistogramTests(unittest.TestCase):
    def test_empty_is_no_observation_not_zero_latency(self):
        value = reply_histogram()
        before = copy.deepcopy(value)
        result = analysis.http_reply_histogram(value)
        self.assertFalse(result["actual_callback_age_observed"])
        self.assertIsNone(result["min_ns"])
        self.assertIsNone(result["max_ns"])
        self.assertEqual(result["quantile_intervals_ns"], dict(p50=None, p95=None, p99=None))
        self.assertEqual(value, before)

    def test_quantiles_are_intervals_not_bin_midpoints_or_means(self):
        value = reply_histogram(100, 100, 40_000_000, {0: 50, 11: 45, 16: 5})
        before = copy.deepcopy(value)
        result = analysis.http_reply_histogram(value)
        self.assertEqual(result["quantile_intervals_ns"], {
            "p50": {"lower_ns": 100, "upper_ns": 999},
            "p95": {"lower_ns": 1_024_000, "upper_ns": 2_047_999},
            "p99": {"lower_ns": 32_768_000, "upper_ns": 40_000_000}})
        self.assertNotIn("mean", result)
        self.assertNotIn("mean_ns", result)
        self.assertNotIn("p95_ns", result)
        self.assertTrue(result["actual_callback_age_observed"])
        self.assertEqual(value, before)

    def test_nanosecond_edges_and_nearest_rank_are_explicit(self):
        result = analysis.http_reply_histogram(reply_histogram(3, 999, 2000, {0: 1, 1: 1, 2: 1}))
        self.assertEqual(result["quantile_intervals_ns"]["p50"], {"lower_ns": 1000, "upper_ns": 1999})
        self.assertEqual(result["quantile_intervals_ns"]["p95"], {"lower_ns": 2000, "upper_ns": 2000})

    def test_top_bin_is_open_ended_then_constrained_by_actual_extrema(self):
        lower = 1_073_741_824_000
        result = analysis.http_reply_histogram(reply_histogram(20, lower + 123, lower * 3, {31: 20}))
        for interval in result["quantile_intervals_ns"].values():
            self.assertEqual(interval, {"lower_ns": lower + 123, "upper_ns": lower * 3})
        self.assertNotEqual(result["quantile_intervals_ns"]["p95"]["lower_ns"],
                            result["quantile_intervals_ns"]["p95"]["upper_ns"])

    def test_one_known_observation_has_a_justified_degenerate_interval(self):
        result = analysis.http_reply_histogram(reply_histogram(1, 1234, 1234, {1: 1}))
        self.assertTrue(all(interval == {"lower_ns": 1234, "upper_ns": 1234}
                            for interval in result["quantile_intervals_ns"].values()))

    def test_intervals_contain_known_exact_order_statistics(self):
        values = [0, 10, 999, 1000, 1999, 2000, 3999, 8000, 7_000_000, 1_073_741_824_010]
        result = analysis.http_reply_histogram(reply_histogram(10, values[0], values[-1],
            {0: 3, 1: 2, 2: 2, 4: 1, 13: 1, 31: 1}))
        for percent in (50, 95, 99):
            exact = values[(len(values) * percent + 99) // 100 - 1]
            interval = result["quantile_intervals_ns"][f"p{percent}"]
            self.assertLessEqual(interval["lower_ns"], exact)
            self.assertGreaterEqual(interval["upper_ns"], exact)

    def test_exact_schema_rejects_unrelated_fields_and_missing_values(self):
        valid = reply_histogram()
        for value in (None, [], {}, {**valid, "unlisted": "synthetic-extra"},
                      {key: value for key, value in valid.items() if key != "samples"}):
            with self.subTest(value=value), self.assertRaises(ValueError):
                analysis.http_reply_histogram(value)

    def test_bin_width_and_container_type_are_validated(self):
        for bins in ([0] * 31, [0] * 33, "0" * 32, {0: 0}):
            with self.subTest(bins=bins), self.assertRaises(ValueError):
                analysis.http_reply_histogram({**reply_histogram(), "bins_log2_us": bins})

    def test_scalar_types_negative_values_and_unsigned_overflow_are_rejected(self):
        invalid = (True, False, 0.0, "0", None, -1, analysis.MAX_U64 + 1, float("nan"), float("inf"))
        for field in ("samples", "min_ns", "max_ns"):
            for value in invalid:
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    analysis.http_reply_histogram({**reply_histogram(), field: value})
        for value in invalid:
            aggregate = reply_histogram()
            aggregate["bins_log2_us"][0] = value
            with self.subTest(bin=value), self.assertRaises(ValueError):
                analysis.http_reply_histogram(aggregate)

    def test_count_mismatch_sum_overflow_and_saturated_counters_are_rejected(self):
        cases = [reply_histogram(2, 0, 0, {0: 1}),
                 reply_histogram(analysis.MAX_U64 - 1, 0, 1000,
                                 {0: analysis.MAX_U64 - 1, 1: analysis.MAX_U64 - 1}),
                 reply_histogram(analysis.MAX_U64, 0, 0, {0: analysis.MAX_U64})]
        for value in cases:
            with self.subTest(value=value), self.assertRaises(ValueError):
                analysis.http_reply_histogram(value)

    def test_extrema_must_agree_with_count_and_nonempty_bin_range(self):
        cases = [reply_histogram(0, 1, 1), reply_histogram(1, 1001, 1000, {1: 1}),
                 reply_histogram(1, 1000, 1001, {1: 1}),
                 reply_histogram(2, 1000, 2000, {0: 1, 2: 1}),
                 reply_histogram(2, 1000, 2000, {1: 1, 3: 1}),
                 reply_histogram(3, 1000, 2000, {0: 1, 1: 1, 2: 1}),
                 reply_histogram(3, 1000, 2000, {1: 1, 2: 1, 3: 1})]
        for value in cases:
            with self.subTest(value=value), self.assertRaises(ValueError):
                analysis.http_reply_histogram(value)

    def test_large_valid_counts_do_not_use_floating_rank_arithmetic(self):
        count = analysis.MAX_U64 - 1
        result = analysis.http_reply_histogram(reply_histogram(count, 0, 0, {0: count}))
        self.assertEqual(result["samples"], count)
        self.assertEqual(result["quantile_intervals_ns"]["p99"], {"lower_ns": 0, "upper_ns": 0})


class CommandTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def save(self, name, value):
        path = self.directory / name
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def invoke(self, *args):
        return subprocess.run([sys.executable, str(SCRIPT), *map(str, args)],
                              text=True, capture_output=True, check=False)

    def test_actual_cli_gate_rejects_exit_zero_but_unqualified_and_preserves_inputs(self):
        value = report("control", False, ["native_FRAME_gap_in_window"])
        value["analysis_exit_code"] = 0
        path = self.save("report.json", value)
        before = path.read_bytes()
        result = self.invoke("qualify", path, "--expected", "control")
        self.assertEqual(result.returncode, 2)
        self.assertIs(json.loads(result.stderr)["qualified_for_comparison"], False)
        self.assertEqual(path.read_bytes(), before)
        path = self.save("qualified.json", report("control"))
        result = self.invoke("qualify", path, "--expected", "control")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["run_order"], ["control"])

    def test_reduction_exit_zero_leaves_comparison_qualification_unevaluated(self):
        path = self.save("rows.json", [metric(1, 0, 1), metric(2, 0, 3)])
        result = self.invoke("metrics", path, "--fields", *FIELDS)
        self.assertEqual(result.returncode, 0, result.stderr)
        output = json.loads(result.stdout)
        self.assertEqual(output["comparison_qualification"], "not_evaluated")
        self.assertEqual(output["result"]["fields"]["value0"]["count"], 2)
        with self.assertRaises(ValueError):
            analysis.require_qualified([output], ["control"])

    def test_cli_cpu_and_overview_have_no_private_runtime_dependencies(self):
        names = self.save("names.json", ["renderOpaquePostDeferred()::$_1::operator()()"])
        result = self.invoke("cpu-context", names)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(json.loads(result.stdout)["result"]
                         ["explicit_named_forward_binder_inclusive_under_opaque_post"])
        rows = self.save("buckets.json", [bucket()])
        result = self.invoke("overview", rows, "--start-ns", 0, "--end-ns", 20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["result"]["groups"][0]["variant_raw"], 0)

    def test_ambiguous_nonfinite_and_wrong_json_shapes_reject(self):
        path = self.directory / "invalid.json"
        for text in ('{"run":"control","run":"other"}', '[NaN]', '{}'):
            path.write_text(text)
            result = self.invoke("metrics", path, "--fields", *FIELDS)
            self.assertEqual(result.returncode, 2, result.stdout)

    def test_input_and_observation_budgets(self):
        path = self.save("small.json", [metric(1, 0, 1)])
        with mock.patch.object(analysis, "MAX_INPUT_BYTES", 1), self.assertRaises(ValueError):
            analysis._read_json(path)
        with mock.patch.object(analysis, "MAX_ROWS", 1), self.assertRaises(ValueError):
            analysis.metric_distributions([metric(1, 0, 1), metric(2, 0, 2)], FIELDS)

    def test_histogram_cli_keeps_uncertainty_and_comparison_gate_separate(self):
        path = self.save("reply-age.json", reply_histogram(100, 100, 40_000_000, {0: 50, 11: 45, 16: 5}))
        before = path.read_bytes()
        result = self.invoke("http-reply-histogram", path)
        self.assertEqual(result.returncode, 0, result.stderr)
        output = json.loads(result.stdout)
        self.assertEqual(output["comparison_qualification"], "not_evaluated")
        self.assertEqual(output["result"]["quantile_intervals_ns"]["p95"],
                         {"lower_ns": 1_024_000, "upper_ns": 2_047_999})
        self.assertEqual(path.read_bytes(), before)
        zero = self.invoke("http-reply-histogram", self.save("zero-age.json", reply_histogram()))
        self.assertEqual(zero.returncode, 0, zero.stderr)
        self.assertFalse(json.loads(zero.stdout)["result"]["actual_callback_age_observed"])

    def test_histogram_cli_rejects_invalid_counters_and_does_not_emit_unlisted_values(self):
        for value in (reply_histogram(1), {**reply_histogram(), "unlisted": "synthetic-extra"}):
            path = self.save("invalid-age.json", value)
            result = self.invoke("http-reply-histogram", path)
            self.assertEqual(result.returncode, 2)
            self.assertIs(json.loads(result.stderr)["qualified_for_comparison"], False)
            self.assertNotIn("synthetic-extra", result.stderr + result.stdout)


if __name__ == "__main__":
    unittest.main()
