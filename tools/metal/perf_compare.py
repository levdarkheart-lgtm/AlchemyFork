#!/usr/bin/env python3
"""Analyze per-frame CSV evidence; never fabricate telemetry.

CSV schema: scenario,frame_ms[,cpu_ms,gpu_ms,draw_calls,metal_submissions].
Samples must be warm-up-free and measured at comparable settings. --enforce
applies docs/metal/benchmarks/scenarios.json. --require-provenance validates the
development viewer's source-exact schema.
"""
import argparse
import csv
import json
import math
import re
import statistics
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SPEC = ROOT / "docs/metal/benchmarks/scenarios.json"
PROVENANCE_HEADERS = {
    "variant", "frame_index", "source_sha", "build_type",
    "drawable_width", "drawable_height", "quality_profile", "native_exact",
}
SOURCE_SHA = re.compile(r"^[0-9a-f]{40}$")
SECONDARY_METRICS = ("cpu_ms", "gpu_ms", "draw_calls", "metal_submissions")


def _unsigned(row, field, path, line, allow_zero=True):
    try:
        value = int(row[field])
    except (ValueError, TypeError, KeyError) as e:
        raise ValueError(f"{path}:{line}: invalid {field}") from e
    if value < 0 or (not allow_zero and value == 0):
        raise ValueError(f"{path}:{line}: invalid {field}")
    return value


def load_capture(path: Path, require_provenance=False, candidate=False):
    result = defaultdict(list)
    secondary = {}
    metadata = {}
    previous_index = {}
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        headers = set(reader.fieldnames or [])
        if not {"scenario", "frame_ms"}.issubset(headers):
            raise ValueError(f"{path}: requires scenario,frame_ms CSV headers")
        has_provenance = PROVENANCE_HEADERS.issubset(headers)
        secondary = {
            field: defaultdict(list)
            for field in SECONDARY_METRICS if field in headers
        }
        if require_provenance and not has_provenance:
            missing = ",".join(sorted(PROVENANCE_HEADERS - headers))
            raise ValueError(f"{path}: missing provenance headers: {missing}")
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

            for field, scenes in secondary.items():
                if field in ("draw_calls", "metal_submissions"):
                    value = _unsigned(row, field, path, n)
                else:
                    try:
                        value = float(row[field])
                    except (ValueError, TypeError, KeyError) as e:
                        raise ValueError(f"{path}:{n}: invalid {field}") from e
                    if not math.isfinite(value) or value < 0:
                        raise ValueError(f"{path}:{n}: invalid {field}")
                scenes[scene].append(value)

            if not has_provenance:
                continue
            variant = (row.get("variant") or "").strip()
            source_sha = (row.get("source_sha") or "").strip().lower()
            build_type = (row.get("build_type") or "").strip()
            if not variant:
                raise ValueError(f"{path}:{n}: missing variant")
            if not SOURCE_SHA.fullmatch(source_sha):
                raise ValueError(f"{path}:{n}: source_sha must be a full Git commit")
            if not build_type:
                raise ValueError(f"{path}:{n}: missing build_type")
            frame_index = _unsigned(row, "frame_index", path, n)
            width = _unsigned(row, "drawable_width", path, n, allow_zero=False)
            height = _unsigned(row, "drawable_height", path, n, allow_zero=False)
            quality = _unsigned(row, "quality_profile", path, n)
            exact = _unsigned(row, "native_exact", path, n)
            if exact not in (0, 1):
                raise ValueError(f"{path}:{n}: native_exact must be 0 or 1")
            if candidate and exact != 1:
                raise ValueError(
                    f"{path}:{n}: candidate row is not an exact native presentation")
            if scene in previous_index and frame_index != previous_index[scene] + 1:
                raise ValueError(f"{path}:{n}: nonconsecutive frame_index for {scene}")
            previous_index[scene] = frame_index
            row_metadata = {
                "variant": variant,
                "source_sha": source_sha,
                "build_type": build_type,
                "drawable_width": width,
                "drawable_height": height,
                "quality_profile": quality,
            }
            if scene in metadata and metadata[scene] != row_metadata:
                raise ValueError(f"{path}:{n}: provenance changed within {scene}")
            metadata[scene] = row_metadata
    return {
        "frames": result,
        "secondary_metrics": secondary,
        "metadata": metadata,
        "provenance_verified": has_provenance,
    }


def load_frames(path: Path):
    return load_capture(path)["frames"]


def compare_provenance(baseline, candidate, required_scenes):
    if not baseline["provenance_verified"] and not candidate["provenance_verified"]:
        return {"verified": False, "reason": "provenance columns were not supplied"}
    if baseline["provenance_verified"] != candidate["provenance_verified"]:
        raise ValueError("baseline and candidate must use the same provenance schema")

    scenes = {}
    for scene in required_scenes:
        if scene not in baseline["metadata"] or scene not in candidate["metadata"]:
            continue
        base = baseline["metadata"][scene]
        new = candidate["metadata"][scene]
        for field in ("build_type", "drawable_width", "drawable_height",
                      "quality_profile"):
            if base[field] != new[field]:
                raise ValueError(
                    f"{scene}: baseline/candidate {field} does not match")
        scenes[scene] = {"baseline": base, "candidate": new}
    return {"verified": True, "scenes": scenes}


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


def summarize_secondary(values):
    ordered = sorted(values)
    return {
        "count": len(ordered),
        "p50": round(statistics.median(ordered), 4),
        "p95": round(percentile(ordered, 0.95), 4),
        "mean": round(statistics.fmean(ordered), 4),
    }


def compare_secondary_metrics(baseline, candidate, required_scenes):
    """Report common secondary columns without treating them as pass gates."""
    report = {}
    for field in sorted(set(baseline) & set(candidate)):
        scenes = []
        for scene in required_scenes:
            if scene not in baseline[field] or scene not in candidate[field]:
                continue
            scenes.append({
                "scenario": scene,
                "baseline": summarize_secondary(baseline[field][scene]),
                "candidate": summarize_secondary(candidate[field][scene]),
            })
        report[field] = {
            "unit": "ms" if field.endswith("_ms") else "count_per_frame",
            "scenes": scenes,
        }
    return report


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
    ap.add_argument("--require-provenance", action="store_true",
                    help="require source/build/display metadata and exact native candidate rows")
    args = ap.parse_args()
    try:
        spec = json.loads(args.spec.read_text(encoding="utf-8"))
        baseline = load_capture(
            args.baseline, require_provenance=args.require_provenance)
        candidate = load_capture(
            args.candidate, require_provenance=args.require_provenance,
            candidate=True)
        report = compare(baseline["frames"], candidate["frames"], spec)
        report["provenance"] = compare_provenance(
            baseline, candidate, [s["id"] for s in spec["scenarios"]])
        report["secondary_metrics"] = compare_secondary_metrics(
            baseline["secondary_metrics"], candidate["secondary_metrics"],
            [s["id"] for s in spec["scenarios"]])
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
