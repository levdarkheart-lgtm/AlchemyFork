#!/usr/bin/env python3
"""Analyze per-frame CSV evidence; never fabricate telemetry.

CSV schema: scenario,frame_ms[,cpu_ms,gpu_ms]. Samples must be warm-up-free and
measured at comparable settings. --enforce applies docs/metal/benchmarks/scenarios.json.
"""
import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SPEC = ROOT / "docs/metal/benchmarks/scenarios.json"


def load_frames(path: Path):
    result = defaultdict(list)
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if not {"scenario", "frame_ms"}.issubset(reader.fieldnames or []):
            raise ValueError(f"{path}: requires scenario,frame_ms CSV headers")
        for n, row in enumerate(reader, 2):
            scene = (row.get("scenario") or "").strip()
            if not scene:
                raise ValueError(f"{path}:{n}: missing scenario")
            try:
                ms = float(row["frame_ms"])
            except (ValueError, TypeError) as e:
                raise ValueError(f"{path}:{n}: invalid frame_ms") from e
            if not math.isfinite(ms) or ms <= 0:
                raise ValueError(f"{path}:{n}: nonpositive/nonfinite frame time")
            result[scene].append(ms)
    return result


def percentile(sorted_values, pct):
    if not sorted_values:
        raise ValueError("empty samples")
    idx = max(0, min(len(sorted_values) - 1, math.ceil(pct * len(sorted_values)) - 1))
    return sorted_values[idx]


def summarize(values):
    ordered = sorted(values)
    return {"count": len(ordered), "p50_ms": round(statistics.median(ordered), 4),
            "p95_ms": round(percentile(ordered, 0.95), 4),
            "mean_ms": round(statistics.fmean(ordered), 4)}


def compare(baseline, candidate, spec):
    required = [s["id"] for s in spec["scenarios"]]
    unknown = sorted((set(baseline) | set(candidate)) - set(required))
    missing = sorted(set(required) - (set(baseline) & set(candidate)))
    results = []
    min_samples = int(spec["minimum_frames_per_variant"])
    target = float(spec["minimum_p50_frame_time_improvement_pct"])
    regression = float(spec["maximum_p95_frame_time_regression_pct"])
    qualifying = 0
    bad_p95 = []
    insufficient = []
    for scene in required:
        if scene not in baseline or scene not in candidate:
            continue
        b = summarize(baseline[scene])
        c = summarize(candidate[scene])
        p50_win = (b["p50_ms"] - c["p50_ms"]) / b["p50_ms"] * 100
        p95_win = (b["p95_ms"] - c["p95_ms"]) / b["p95_ms"] * 100
        enough = min(b["count"], c["count"]) >= min_samples
        qualifies = enough and p50_win >= target
        if qualifies:
            qualifying += 1
        if p95_win < -regression:
            bad_p95.append(scene)
        if not enough:
            insufficient.append(scene)
        results.append({"scenario": scene, "baseline": b, "candidate": c,
                        "p50_frame_time_improvement_pct": round(p50_win, 2),
                        "p95_frame_time_improvement_pct": round(p95_win, 2),
                        "enough_samples": enough, "p50_target_met": qualifies})
    passes = not missing and not unknown and not insufficient and not bad_p95 and qualifying >= int(spec["required_scenarios_for_20pct_p50_win"])
    return {"passes_target": passes, "required_qualifying_scenes": spec["required_scenarios_for_20pct_p50_win"],
            "qualifying_scenes": qualifying, "missing_scenes": missing, "unknown_scenes": unknown,
            "insufficient_sample_scenes": insufficient, "p95_regressions": bad_p95,
            "scenes": results,
            "limitations": "Measures supplied frame times only; scene comparability, image quality and GPU/CPU traces must be separately verified."}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--baseline", type=Path, required=True)
    ap.add_argument("--candidate", type=Path, required=True)
    ap.add_argument("--spec", type=Path, default=DEFAULT_SPEC)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--enforce", action="store_true")
    args = ap.parse_args()
    try:
        spec = json.loads(args.spec.read_text(encoding="utf-8"))
        report = compare(load_frames(args.baseline), load_frames(args.candidate), spec)
    except (ValueError, OSError, json.JSONDecodeError, KeyError) as e:
        ap.error(str(e))
    result = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result, encoding="utf-8")
    print(result, end="")
    return 0 if report["passes_target"] or not args.enforce else 1


if __name__ == "__main__":
    raise SystemExit(main())
