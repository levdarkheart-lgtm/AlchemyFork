#!/usr/bin/env python3
"""Small offline performance-evidence contracts (PPD-575/578/579/581/583/587).

This module neither records nor exports traces. Inputs must be closed, already
selected observations from one process and one clock domain. It does not replace
quiet_capture.py's collection checks or quiet_diagnostic_summary.py's native
FRAME/window analysis. Successful reduction is not comparison qualification.

Fresh-checkout examples (JSON arrays for observations, JSON objects for reports):
  python3 tools/metal/performance_analysis.py qualify a.json b.json \
      --expected control --expected candidate
  python3 tools/metal/performance_analysis.py metrics rows.json \
      --fields v0 v1 v2 v3 v4 v5 v6 v7
  python3 tools/metal/performance_analysis.py cpu-context stack-names.json
  python3 tools/metal/performance_analysis.py overview buckets.json \
      --start-ns 1000000000 --end-ns 3000000000
  python3 tools/metal/performance_analysis.py http-reply-histogram counters.json

Overview mappings use the actual export mnemonics: start, duration, value-in-ms,
number-of-frames, metric-variant, name, and optionally layer-id. They are decoded
rows, not raw xctrace XML. Variant values remain uninterpreted without independent
installed-schema proof. CPU names are leaf first; context flags overlap.
Only qualify exits zero as an affirmative gate. Other commands leave comparison
qualification explicitly unevaluated. No input files are modified.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from collections.abc import Mapping
import json
import math
from pathlib import Path
import re
import statistics
import sys

MAX_INPUT_BYTES = 16 * 1024 * 1024
MAX_ROWS = 100_000
MAX_STACK_DEPTH = 512
MAX_REPORTS = 1024
MAX_U64 = (1 << 64) - 1
HASH_NAMES = ("__murmur2_or_cityhash", "__weak_hash_len_32_with_seeds",
              "__hash_memory", "__cityhash", "__hash_len_")


def require_qualified(reports, expected):
    """Require explicit qualification and exactly the declared distinct runs.

    Reducer/process exit status is deliberately not a substitute for the report's
    literal True and empty reasons list. This checks declarations, not the raw
    measurements, controls, clocks, identities or freshness of those reports.
    Returns names in supplied order; never silently reorders or pools runs.
    """
    reports, expected = list(reports), list(expected)
    if not expected or len(expected) > MAX_REPORTS or any(
            not isinstance(name, str) or not name for name in expected):
        raise ValueError("Expected run names must be a nonempty bounded list")
    if len(expected) != len(set(expected)):
        raise ValueError("Duplicate expected run names")
    if len(reports) > MAX_REPORTS:
        raise ValueError("Report count exceeds budget")
    names = []
    for report in reports:
        if not isinstance(report, Mapping):
            raise ValueError("A comparison report must be an object")
        name = report.get("run")
        if not isinstance(name, str) or not name:
            raise ValueError("A comparison report needs a run name")
        if (report.get("qualified_for_comparison") is not True
                or report.get("qualification_reasons") != []):
            raise ValueError(f"Unqualified measurement: {name}")
        names.append(name)
    if len(names) != len(set(names)) or set(names) != set(expected):
        raise ValueError("Missing, duplicate or unexpected comparison runs")
    return names


def _finite_number(value):
    if type(value) not in (int, float):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def _statistics(values):
    values = sorted(values)

    def quantile(fraction):
        if not values:
            return None
        position = (len(values) - 1) * fraction
        lower = int(position)
        upper = min(lower + 1, len(values) - 1)
        part = position - lower
        return values[lower] * (1 - part) + values[upper] * part

    return {"count": len(values), "mean": statistics.mean(values) if values else None,
            "min": values[0] if values else None, "max": values[-1] if values else None,
            "p50": quantile(.5), "p95": quantile(.95), "p99": quantile(.99)}


def _rows(records):
    records = list(records)
    if len(records) > MAX_ROWS:
        raise ValueError("Observation count exceeds budget")
    if any(not isinstance(record, Mapping) for record in records):
        raise ValueError("Observations must be objects")
    return records


def metric_distributions(records, fields):
    """Retain every observed eight-value metric row, including exact duplicates.

    Zero, absent, boolean and negative frame tags are not frame identities.
    First-positive-frame distributions are an optional, explicitly selected
    subset. Neither distribution sums counters into work or weights by time.
    """
    records, fields = _rows(records), tuple(fields)
    if len(fields) != 8 or any(not isinstance(field, str) or not field for field in fields):
        raise ValueError("Exactly eight nonempty metric field names are required")
    if len(set(fields)) != len(fields):
        raise ValueError("Metric field names must be distinct")

    def columns(rows):
        return {name: _statistics([record["values"][index] for record in rows
                if isinstance(record.get("values"), list) and len(record["values"]) == 8
                and _finite_number(record["values"][index])])
                for index, name in enumerate(fields)}

    first_positive = {}
    positive_rows = unqualified = conflicts = duplicates = 0
    conflicting_tags, payloads = set(), set()
    for record in records:
        payload = json.dumps(record, sort_keys=True, separators=(",", ":"))
        duplicates += payload in payloads
        payloads.add(payload)
        frame = record.get("frame_id")
        if type(frame) is not int or frame <= 0:
            unqualified += 1
            continue
        positive_rows += 1
        if frame not in first_positive:
            first_positive[frame] = record
        elif record.get("values") != first_positive[frame].get("values"):
            conflicts += 1
            conflicting_tags.add(frame)
    return {"all_observed_rows": len(records),
            "zero_or_unqualified_frame_tag_rows": unqualified,
            "exact_duplicate_payload_rows_retained": duplicates,
            "invalid_value_payload_rows": sum(not isinstance(row.get("values"), list)
                or len(row["values"]) != 8 for row in records),
            "fields": columns(records), "first": records[0] if records else None,
            "last": records[-1] if records else None,
            "optional_first_positive_frame": {
                "positive_tagged_rows": positive_rows,
                "distinct_positive_frame_tags": len(first_positive),
                "repeated_positive_tag_rows": positive_rows - len(first_positive),
                "conflicting_positive_frame_observations": conflicts,
                "conflicting_positive_frame_tags": len(conflicting_tags),
                "fields": columns(first_positive.values())},
            "meaning": "Primary statistics retain all observed rows, not complete operations, "
                       "time-weighted work or cumulative totals; positive-frame statistics are optional."}


def cpu_contexts(names):
    """Name-based inclusive flags, not exclusive CPU time or exact binder cost.

    Anonymous opaque-post lambda/operator frames include unrelated physics and
    other constants. Only a literal named binder frame receives the named flag;
    an absent optimized/inlined name does not establish absent work.
    """
    names = list(names)
    if len(names) > MAX_STACK_DEPTH or any(not isinstance(name, str) for name in names):
        raise ValueError("CPU stack must contain at most 512 string names, leaf first")
    leaf = names[0] if names else ""
    post = any("renderOpaquePostDeferred" in name for name in names)
    publish = any("ALMetalWorldFrameData::publish(" in name for name in names)
    old_setter = any("setLightingData(" in name for name in names)
    hash_leaf = any(token in leaf for token in HASH_NAMES)
    any_hash = any(token in name for name in names for token in HASH_NAMES)
    broad_lambda = any("renderOpaquePostDeferred" in name
        and any(token in name for token in ("operator()", "lambda", "$_")) for name in names)
    return {"broad_hash_subtree_under_opaque_post": post and any_hash,
            "broad_hash_leaf_under_opaque_post": post and hash_leaf,
            "broad_hash_leaf_with_publish_ancestor": post and hash_leaf and publish,
            "broad_hash_leaf_without_publish_symbol": post and hash_leaf and not publish,
            "old_strict_hash_publish_setLightingData_post_leaf": post and hash_leaf and publish and old_setter,
            "frame_publish_inclusive_under_opaque_post": post and publish,
            "broad_opaque_post_lambda_inclusive": post and broad_lambda,
            "explicit_named_forward_binder_inclusive_under_opaque_post": post and any(
                "bind_forward_probe_constants" in name for name in names),
            "all_opaque_post_inclusive": post}


def _integer(value):
    if type(value) is int:
        return value
    if isinstance(value, str) and re.fullmatch(r"[+-]?[0-9]+", value):
        return int(value)
    raise ValueError("Expected an integer cell")


def overview_distributions(records, start_ns, end_ns):
    """Reduce decoded, process-selected Overview buckets by name/variant/layer.

    Require metric-variant, never the invented variant alias. Weighted means use
    only fully contained buckets and integer drawable associations. Families and
    layers stay separate; bounds alone do not prove gapless coverage. Results are
    not GPU occupancy, whole-frame quantiles or additive CPU/GPU costs.
    """
    if type(start_ns) is not int or type(end_ns) is not int or not 0 <= start_ns < end_ns:
        raise ValueError("Expected an increasing nonnegative integer-nanosecond window")
    groups, invalid = defaultdict(list), 0
    for record in _rows(records):
        if "metric-variant" not in record:
            raise ValueError("Overview requires the metric-variant column")
        try:
            start, duration = _integer(record["start"]), _integer(record["duration"])
            count, variant = _integer(record["number-of-frames"]), _integer(record["metric-variant"])
            raw_value = record["value-in-ms"]
            if isinstance(raw_value, bool):
                raise ValueError("Boolean metric value")
            value = float(raw_value)
            name, layer = record["name"], record.get("layer-id", "<missing>")
            if (start < 0 or duration <= 0 or count < 0 or not math.isfinite(value) or value < 0
                    or (count == 0 and value != 0) or not isinstance(name, str) or not name
                    or not isinstance(layer, str) or not layer):
                raise ValueError("Invalid Overview row")
        except (KeyError, TypeError, ValueError, OverflowError):
            invalid += 1
            continue
        groups[name, variant, layer].append({"start_ns": start, "end_ns": start + duration,
            "value_ms": value, "associated_frames": count})
    output = []
    for (name, variant, layer), rows in sorted(groups.items()):
        rows.sort(key=lambda row: row["start_ns"])
        complete = [row for row in rows if start_ns <= row["start_ns"] and row["end_ns"] <= end_ns]
        overlap = [row for row in rows if row["start_ns"] < end_ns and row["end_ns"] > start_ns]
        associations = sum(row["associated_frames"] for row in complete)
        output.append({"name": name, "variant_raw": variant, "layer": layer,
            "first_ns": rows[0]["start_ns"], "last_end_ns": max(row["end_ns"] for row in rows),
            "outer_bounds_contain_window": rows[0]["start_ns"] <= start_ns
                and max(row["end_ns"] for row in rows) >= end_ns,
            "complete_buckets": len(complete), "overlap_buckets": len(overlap),
            "positive_overlap_buckets": sum(row["associated_frames"] > 0 for row in overlap),
            "associated_frames_complete_buckets": associations,
            "frame_weighted_value_ms": sum(row["value_ms"] * (row["associated_frames"] / associations)
                for row in complete) if associations else None,
            "complete_bucket_bounds_ns": [complete[0]["start_ns"], max(row["end_ns"] for row in complete)]
                if complete else None,
            "boundary_buckets_excluded_from_weighted_mean": len(overlap) - len(complete),
            "overlap_rows": overlap})
    return {"window_ns": [start_ns, end_ns], "groups": output, "invalid_rows": invalid,
            "meaning": "Per-family/variant/layer observed drawable associations; complete buckets only. "
                       "Not GPU occupancy, frame quantiles, gapless coverage or additive costs."}


def http_reply_histogram(aggregate):
    """Validate PPD587 counters and return conservative percentile intervals.

    Input is exactly samples/min_ns/max_ns and bins_log2_us (32 integer counts).
    Bin0 is [0,999]ns; bin n=1..30 is [2^(n-1)*1000,2^n*1000-1]ns;
    bin31 combines all >=2^30us. Its upper bound is the observed exact max.
    Extrema constrain intervals but do not reveal the within-bin distribution.
    Quantiles use nearest rank, not interpolation or fabricated bin midpoints.
    """
    keys = {"samples", "min_ns", "max_ns", "bins_log2_us"}
    if not isinstance(aggregate, Mapping) or set(aggregate) != keys:
        raise ValueError("HTTP reply histogram requires only samples, min_ns, max_ns, bins_log2_us")
    count, minimum, maximum = (aggregate[key] for key in ("samples", "min_ns", "max_ns"))
    bins = aggregate["bins_log2_us"]
    if not isinstance(bins, (list, tuple)) or len(bins) != 32:
        raise ValueError("HTTP reply histogram requires 32 bins")
    if any(type(value) is not int or not 0 <= value <= MAX_U64
           for value in (count, minimum, maximum, *bins)):
        raise ValueError("HTTP reply counters and extrema must be unsigned 64-bit integers")
    total = sum(bins)
    if total > MAX_U64:
        raise ValueError("HTTP reply histogram count sum overflows unsigned 64-bit range")
    if total != count or minimum > maximum:
        raise ValueError("HTTP reply histogram count/extrema mismatch")
    if count == MAX_U64 or MAX_U64 in bins:
        raise ValueError("Saturated HTTP reply counters cannot establish exact observation counts")
    if count == 0 and (minimum != 0 or maximum != 0):
        raise ValueError("Empty HTTP reply histogram must have zero stored extrema")
    if count == 1 and minimum != maximum:
        raise ValueError("A single HTTP reply observation must have equal extrema")

    def index(ns):
        return min((ns // 1000).bit_length(), 31)

    if count:
        first, last = index(minimum), index(maximum)
        if not bins[first] or not bins[last] or any(bins[:first]) or any(bins[last + 1:]):
            raise ValueError("HTTP reply histogram disagrees with its observed extrema")

    def quantile(percent):
        if not count:
            return None
        rank, cumulative = (count * percent + 99) // 100, 0
        for slot, observations in enumerate(bins):
            cumulative += observations
            if cumulative >= rank:
                lower = 0 if slot == 0 else (1 << (slot - 1)) * 1000
                upper = maximum if slot == 31 else (1 << slot) * 1000 - 1
                return {"lower_ns": max(lower, minimum), "upper_ns": min(upper, maximum)}

    return {"samples": count, "min_ns": minimum if count else None,
            "max_ns": maximum if count else None, "bins_log2_us": list(bins),
            "actual_callback_age_observed": count > 0,
            "quantile_intervals_ns": {f"p{percent}": quantile(percent) for percent in (50, 95, 99)},
            "quantile_method": "Nearest rank; inclusive interval bounds constrained by observed min/max. "
                               "No exact percentile or mean is inferred within a bin.",
            "meaning": "Observed reply-publication to callback-entry ages. Empty is not zero latency; "
                       "not network transfer, callback work, loading time, FPS or comparison qualification."}


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _read_json(path):
    before = path.stat()
    with path.open("rb") as stream:
        data = stream.read(MAX_INPUT_BYTES + 1)
    after = path.stat()
    if len(data) > MAX_INPUT_BYTES:
        raise ValueError("JSON input exceeds 16 MiB")
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError("Input changed during read")

    def invalid_constant(value):
        raise ValueError(f"Nonfinite JSON constant: {value}")

    return json.loads(data, object_pairs_hook=_unique_object, parse_constant=invalid_constant)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    gate = commands.add_parser("qualify", help="reject any unqualified/missing/duplicate run")
    gate.add_argument("reports", type=Path, nargs="+")
    gate.add_argument("--expected", action="append", required=True, help="repeat for each declared run")
    metrics = commands.add_parser("metrics", help="retain all eight-value metric observations")
    metrics.add_argument("input", type=Path)
    metrics.add_argument("--fields", nargs=8, required=True)
    cpu = commands.add_parser("cpu-context", help="classify one leaf-first array of stack names")
    cpu.add_argument("input", type=Path)
    overview = commands.add_parser("overview", help="reduce process-selected, decoded Overview rows")
    overview.add_argument("input", type=Path)
    overview.add_argument("--start-ns", required=True, type=int)
    overview.add_argument("--end-ns", required=True, type=int)
    replies = commands.add_parser("http-reply-histogram", help="validate aggregate reply-age counters and bound quantiles")
    replies.add_argument("input", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "qualify":
            names = require_qualified([_read_json(path) for path in args.reports], args.expected)
            result = {"schema": 1, "qualified_for_comparison": True, "run_order": names}
        else:
            value = _read_json(args.input)
            if args.command == "http-reply-histogram":
                reduced = http_reply_histogram(value)
            elif not isinstance(value, list):
                raise ValueError("Observation input must be a JSON array")
            elif args.command == "metrics":
                reduced = metric_distributions(value, args.fields)
            elif args.command == "cpu-context":
                reduced = cpu_contexts(value)
            else:
                reduced = overview_distributions(value, args.start_ns, args.end_ns)
            result = {"schema": 1, "analysis_completed": True,
                      "comparison_qualification": "not_evaluated", "result": reduced}
        print(json.dumps(result, indent=2, allow_nan=False))
        return 0
    except (OSError, TypeError, ValueError, OverflowError, RecursionError) as error:
        print(json.dumps({"schema": 1, "qualified_for_comparison": False, "error": str(error)}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
