import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "memory_sample_analysis", Path(__file__).resolve().parents[1] / "memory_sample_analysis.py")
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)


def sample(before, after, footprint):
    return {"kind": 2, "mach_before_ns": before, "mach_after_ns": after,
            "sample_call_ns": after-before, "physical_footprint_bytes": footprint}


class MemoryWindowTests(unittest.TestCase):
    def test_contained_brackets_exclude_both_boundary_straddlers(self):
        rows = [sample(0, 10, 0), sample(15, 25, 100), sample(30, 40, 10),
                sample(50, 60, 30), sample(75, 85, 200)]
        out = M.analyze_memory_window(rows, [20, 80], missed_intervals=0)
        self.assertEqual(out["selected_indices"], [2, 3])
        self.assertEqual(out["boundary_straddling_samples"], 2)
        self.assertEqual(out["physical_footprint_bytes"]["mean"], 20)
        self.assertEqual((out["before_gap_ns"], out["after_gap_ns"]), (10, 20))

    def test_global_missed_one_does_not_veto_window_and_real_gaps_remain(self):
        rows = [sample(0, 1, 1), sample(20, 21, 2), sample(80, 81, 3), sample(100, 101, 4)]
        out = M.analyze_memory_window(rows, [0, 101], missed_intervals=1)
        self.assertTrue(out["sampled_observation_available"])
        self.assertEqual(out["missed_intervals_global"], 1)
        self.assertEqual(out["selected_sample_start_gaps_ns"], [20, 60, 20])
        self.assertEqual(out["max_global_sample_start_gap_ns"], 60)

    def test_zero_footprint_and_exact_closed_endpoints_are_retained(self):
        out = M.analyze_memory_window([sample(10, 20, 0)], [10, 20], missed_intervals=0)
        self.assertEqual(out["selected_samples"], 1)
        self.assertEqual(out["physical_footprint_bytes"]["min"], 0)
        self.assertIsNone(out["max_selected_sample_start_gap_ns"])

    def test_empty_window_has_no_invented_measurement_or_phase_gate(self):
        out = M.analyze_memory_window([sample(0, 10, 50)], [20, 30], missed_intervals=9)
        self.assertFalse(out["sampled_observation_available"])
        self.assertIsNone(out["physical_footprint_bytes"])
        self.assertIsNone(out["first_selected_bracket_ns"])
        self.assertNotIn("qualified_for_policy_comparison", out)

    def test_uints_reject_boolean_negative_overflow_and_float(self):
        for value in (True, -1, 1 << 64, 1.0):
            with self.subTest(value=value), self.assertRaises(ValueError):
                M.analyze_memory_window([sample(0, 1, value)], [0, 2], missed_intervals=0)
            with self.subTest(missed=value), self.assertRaises(ValueError):
                M.analyze_memory_window([], [0, 2], missed_intervals=value)
        self.assertEqual(M.analyze_memory_window([sample(0, 1, M.MAX_U64)],
                         [0, 2], missed_intervals=0)["physical_footprint_bytes"]["max"], M.MAX_U64)

    def test_order_overlap_reversed_and_call_duration_are_rejected(self):
        for rows in ([sample(20, 30, 1), sample(0, 10, 2)],
                     [sample(0, 20, 1), sample(10, 30, 2)],
                     [{"mach_before_ns": 2, "mach_after_ns": 1, "physical_footprint_bytes": 1}],
                     [{**sample(0, 1, 1), "sample_call_ns": 2}]):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                M.analyze_memory_window(rows, [0, 40], missed_intervals=0)

    def test_invalid_window_and_missing_or_nonnumeric_rows_are_rejected(self):
        for window in ([0], [1, 1], [2, 1], [False, 1], [-1, 2], [0, 1 << 64]):
            with self.subTest(window=window), self.assertRaises(ValueError):
                M.analyze_memory_window([], window, missed_intervals=0)
        for row in ({}, "row", {**sample(0, 1, 1), "extra": "unlisted"},
                    {**sample(0, 1, 1), "kind": 3}):
            with self.subTest(row=row), self.assertRaises(ValueError):
                M.analyze_memory_window([row], [0, 2], missed_intervals=0)

    def test_sample_quantiles_are_linear_interpolation_and_input_is_unchanged(self):
        rows = [sample(0, 1, 10), sample(2, 3, 30)]
        before = [dict(row) for row in rows]
        out = M.analyze_memory_window(rows, [0, 3], missed_intervals=0)
        self.assertEqual(out["physical_footprint_bytes"]["p50"], 20)
        self.assertEqual(out["physical_footprint_bytes"]["p95"], 29)
        self.assertEqual(rows, before)


if __name__ == "__main__":
    unittest.main()
