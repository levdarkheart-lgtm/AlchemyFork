"""Pure sampled-memory window reduction; no collection or run qualification.

The caller retains source/process identity and native/external phase gates.
Brackets are closed intervals: both endpoints must lie inside the given window.
Global missed schedule intervals do not invalidate otherwise valid observations.
Reported sample extrema are not continuous/lifetime peaks, GPU residency or
per-policy marginal cost. Actual gaps remain explicit; uniform cadence is not
inferred. This module never queries a process or changes its state.
"""
from collections.abc import Mapping
import math


MAX_U64 = (1 << 64) - 1


def _uint(value):
    if type(value) is not int or not 0 <= value <= MAX_U64:
        raise ValueError("Expected an unsigned 64-bit integer")
    return value


def _stats(values):
    if not values:
        return None
    values = sorted(values)

    def quantile(fraction):
        position = (len(values) - 1) * fraction
        lower = math.floor(position)
        upper = min(lower + 1, len(values) - 1)
        return values[lower] + (values[upper] - values[lower]) * (position - lower)

    return {"count": len(values), "mean": sum(values) / len(values),
            "min": values[0], "max": values[-1], "p50": quantile(.5),
            "p95": quantile(.95), "p99": quantile(.99)}


def analyze_memory_window(samples, window_mach_ns, *, missed_intervals):
    """Summarize fully contained numeric sample brackets without a global-zero gate.

    Each row needs mach_before_ns, mach_after_ns and physical_footprint_bytes.
    Other row fields, if supplied, must also be unsigned integers. Optional
    sampler kind/sample_call_ns fields are checked. Samples must be ordered,
    with strictly increasing starts and nonoverlapping brackets. Quantiles use
    linear interpolation over sampled values, not a continuous-time population.
    Means average observations without time weighting across unobserved gaps.
    No phase-comparison qualification is manufactured from these statistics.
    """
    if not isinstance(window_mach_ns, (list, tuple)) or len(window_mach_ns) != 2:
        raise ValueError("Expected two Mach window endpoints")
    lo, hi = map(_uint, window_mach_ns)
    if lo >= hi:
        raise ValueError("Expected an increasing Mach window")
    missed_intervals = _uint(missed_intervals)
    selected, selected_indices, starts = [], [], []
    previous = None
    straddling = 0
    total = 0
    for index, row in enumerate(samples):
        if not isinstance(row, Mapping):
            raise ValueError("Expected a numeric sample mapping")
        if any(key not in row for key in (
                "mach_before_ns", "mach_after_ns", "physical_footprint_bytes")):
            raise ValueError("Missing numeric sample field")
        for value in row.values():
            _uint(value)
        before, after = row["mach_before_ns"], row["mach_after_ns"]
        if before > after or (previous is not None and
                              (before <= previous[0] or before < previous[1])):
            raise ValueError("Sample brackets must be ordered and nonoverlapping")
        if "kind" in row and row["kind"] != 2:
            raise ValueError("Expected a sample row")
        if "sample_call_ns" in row and row["sample_call_ns"] != after - before:
            raise ValueError("Sample call duration disagrees with its bracket")
        previous = (before, after)
        starts.append(before)
        total += 1
        if before >= lo and after <= hi:
            selected.append(row)
            selected_indices.append(index)
        elif before <= hi and after >= lo:
            straddling += 1
    selected_gaps = [b["mach_before_ns"] - a["mach_before_ns"]
                     for a, b in zip(selected, selected[1:])]
    return {"schema": 1, "window_mach_ns": [lo, hi],
            "sampled_observation_available": bool(selected),
            "total_samples": total, "selected_samples": len(selected),
            "selected_indices": selected_indices, "boundary_straddling_samples": straddling,
            "missed_intervals_global": missed_intervals,
            "physical_footprint_bytes": _stats([r["physical_footprint_bytes"] for r in selected]),
            "API_bracket_wall_ns": _stats([r["mach_after_ns"] - r["mach_before_ns"] for r in selected]),
            "first_selected_bracket_ns": [selected[0]["mach_before_ns"], selected[0]["mach_after_ns"]] if selected else None,
            "last_selected_bracket_ns": [selected[-1]["mach_before_ns"], selected[-1]["mach_after_ns"]] if selected else None,
            "before_gap_ns": selected[0]["mach_before_ns"] - lo if selected else None,
            "after_gap_ns": hi - selected[-1]["mach_after_ns"] if selected else None,
            "selected_sample_start_gaps_ns": selected_gaps,
            "max_global_sample_start_gap_ns": max((b-a for a, b in zip(starts, starts[1:])), default=None),
            "max_selected_sample_start_gap_ns": max(selected_gaps, default=None),
            "scope": "Sample observations only; caller retains identity/phase gates. "
                     "No uniform cadence, continuous peak, GPU residency or marginal cost inferred."}
