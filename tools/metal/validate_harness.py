#!/usr/bin/env python3
"""Validate repo-local Codex harness structure and critical metadata, not the renderer."""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANDATORY = [
    "AGENTS.md", "docs/metal/README.md", "docs/metal/PRODUCT.md",
    "docs/metal/ARCHITECTURE.md", "docs/metal/OPERATING_MODEL.md",
    "docs/metal/VALIDATION.md", "docs/metal/PERFORMANCE.md", "docs/metal/INSTRUMENTATION.md",
    "docs/metal/APPLE_SILICON.md", "docs/metal/DEPENDENCIES.md",
    "docs/metal/REPO_FACTS.md", "docs/metal/START_CODEX.md",
    "docs/metal/state/CURRENT.md", "docs/metal/state/BLOCKERS.md",
    "docs/metal/work/active/metal-port.md", "docs/metal/feature-matrix.csv",
    "docs/metal/benchmarks/scenarios.json", ".github/workflows/metal-harness.yml",
    ".agents/skills/metal-port/SKILL.md", ".agents/skills/gpu-validation/SKILL.md",
    ".agents/skills/agent-continuity/SKILL.md", "tools/metal/audit_gl.py",
    "tools/metal/perf_compare.py", "tools/metal/build.sh", "tools/metal/doctor.sh",
]
VALID_STATES = {"unverified", "verified", "failed", "blocked", "not_applicable", "waived_by_owner"}


def validate(root: Path, release: bool = False):
    errors = []
    for path in MANDATORY:
        p = root / path
        if not p.is_file():
            errors.append(f"Missing: {path}")
        elif not p.read_text(encoding="utf-8").strip():
            errors.append(f"Empty: {path}")
    for name in ["metal-port", "gpu-validation", "agent-continuity"]:
        p = root / ".agents/skills" / name / "SKILL.md"
        if not p.exists():
            continue
        content = p.read_text(encoding="utf-8")
        if not re.match(r"\A---\nname: [a-z0-9-]+\ndescription: .+\n---\n", content):
            errors.append(f"Invalid skill frontmatter: {p.relative_to(root)}")
    matrix = root / "docs/metal/feature-matrix.csv"
    if matrix.exists():
        with matrix.open(newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            required = {"id", "area", "scenario", "critical", "reference", "metal", "evidence", "notes"}
            if not required.issubset(reader.fieldnames or []):
                errors.append("feature-matrix.csv schema is incomplete")
            else:
                seen = set()
                for n, row in enumerate(reader, 2):
                    if not row["id"] or row["id"] in seen:
                        errors.append(f"Invalid/duplicate feature-matrix ID at row {n}")
                    seen.add(row["id"])
                    if row["reference"] not in VALID_STATES or row["metal"] not in VALID_STATES:
                        errors.append(f"Invalid feature-matrix state at row {n}")
                    if row["critical"] not in {"yes", "no"}:
                        errors.append(f"Invalid critical flag at row {n}")
                    if (row["metal"] == "verified" or row["reference"] == "verified") and not row["evidence"]:
                        errors.append(f"Missing evidence for verified feature at row {n}")
                    if release and row["critical"] == "yes" and (row["metal"] != "verified" or row["reference"] != "verified"):
                        errors.append(f"Release feature unverified at row {n}: {row['id']}")
    spec = root / "docs/metal/benchmarks/scenarios.json"
    if spec.exists():
        try:
            obj = json.loads(spec.read_text(encoding="utf-8"))
            scenes = [s["id"] for s in obj["scenarios"]]
            if len(scenes) < 4 or len(scenes) != len(set(scenes)):
                errors.append("Benchmark scene contract requires >=4 unique scenario IDs")
            if obj["minimum_frames_per_variant"] < 100:
                errors.append("Benchmark frame count too low")
        except (KeyError, TypeError, ValueError, json.JSONDecodeError) as e:
            errors.append(f"Invalid benchmark specification: {e}")
    return errors


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", action="store_true", help="fail if critical feature parity is not verified")
    args = parser.parse_args()
    failures = validate(ROOT, release=args.release)
    for item in failures:
        print("ERROR:", item, file=sys.stderr)
    if failures:
        sys.exit(1)
    print(f"Harness structure and metadata validated ({len(MANDATORY)} required files).")
