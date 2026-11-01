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
    "docs/metal/state/archive/INDEX.md",
    "docs/metal/work/active/metal-port.md", "docs/metal/feature-matrix.csv",
    "docs/metal/work/archive/INDEX.md", "docs/metal/release-invariants.json",
    "docs/metal/benchmarks/scenarios.json", ".github/workflows/metal-harness.yml",
    ".agents/skills/metal-port/SKILL.md", ".agents/skills/gpu-validation/SKILL.md",
    ".agents/skills/agent-continuity/SKILL.md", "tools/metal/audit_gl.py",
    "tools/metal/perf_compare.py", "tools/metal/trace_submissions.py",
    "tools/metal/cua_preflight.py", "tools/metal/graphics_settings.py",
    "tools/metal/build.sh", "tools/metal/doctor.sh",
]
VALID_STATES = {"unverified", "verified", "failed", "blocked", "not_applicable", "waived_by_owner"}
LIVE_DOCUMENTS = [
    "AGENTS.md",
    "docs/metal/README.md",
    "docs/metal/PRODUCT.md",
    "docs/metal/VALIDATION.md",
    "docs/metal/PERFORMANCE.md",
    "docs/metal/OPERATING_MODEL.md",
    "docs/metal/START_CODEX.md",
    "docs/metal/state/CURRENT.md",
    "docs/metal/state/archive/INDEX.md",
    "docs/metal/work/active/metal-port.md",
    "docs/metal/work/archive/INDEX.md",
    ".agents/skills/metal-port/SKILL.md",
    ".agents/skills/gpu-validation/SKILL.md",
    ".agents/skills/agent-continuity/SKILL.md",
]
REQUIRED_INVARIANTS = {
    "independent-arm64-macos-build",
    "native-metal-no-translation-or-gl",
    "alchemy-feature-retention",
    "measured-quality-equivalent-performance",
    "stability-and-independent-review",
    "reproducible-distinct-open-source-release",
    "no-premature-completion",
}
REPO_PATH_RE = re.compile(
    r"(?<![A-Za-z0-9_.-])"
    r"((?:tools/metal|docs/metal|\.agents|benchmarks)"
    r"(?:/[A-Za-z0-9_.-]+)+)"
)
SCRIPT_PATH_RE = re.compile(
    r"(?<![A-Za-z0-9_.-])(tools/metal/[A-Za-z0-9_.-]+\.(?:sh|py))"
)
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
WORK_STATES = {"todo", "doing", "blocked", "deferred", "complete"}


def milestone_states(text: str):
    states = {}
    for line in text.splitlines():
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        if not cells:
            continue
        match = re.match(r"(M-\d{3})", cells[0])
        state = next((cell for cell in cells[1:] if cell in WORK_STATES), None)
        if match and state:
            states[match.group(1)] = state
    return states


def validate_live_references(root: Path, documents=LIVE_DOCUMENTS):
    """Validate local links and repository command paths in live instructions."""
    errors = []
    for relative in documents:
        document = root / relative
        if not document.is_file():
            errors.append(f"Live document missing: {relative}")
            continue
        content = document.read_text(encoding="utf-8")
        for match in MARKDOWN_LINK_RE.finditer(content):
            target = match.group(1).strip()
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            if target.startswith("<") and target.endswith(">"):
                target = target[1:-1]
            target = target.split("#", 1)[0]
            if not target:
                continue
            if not (document.parent / target).resolve().exists():
                errors.append(f"Stale Markdown link in {relative}: {target}")
        for match in REPO_PATH_RE.finditer(content):
            target = match.group(1)
            candidate = ((document.parent / target).resolve()
                         if target.startswith("benchmarks/")
                         else root / target)
            if not candidate.exists():
                errors.append(f"Stale repository path in {relative}: {target}")
        for match in SCRIPT_PATH_RE.finditer(content):
            target = match.group(1)
            script = root / target
            if script.exists() and script.stat().st_mode & 0o111 == 0:
                errors.append(f"Referenced command is not executable: {target}")
    return errors


def validate_current_documents(root: Path):
    """Keep recovery/ledger documents compact and mutually consistent."""
    errors = []
    current_path = root / "docs/metal/state/CURRENT.md"
    ledger_path = root / "docs/metal/work/active/metal-port.md"
    if not current_path.is_file() or not ledger_path.is_file():
        return errors
    current = current_path.read_text(encoding="utf-8")
    ledger = ledger_path.read_text(encoding="utf-8")
    if len(current.splitlines()) > 160:
        errors.append("CURRENT.md exceeds the 160-line recovery-snapshot limit")
    if len(ledger.splitlines()) > 180:
        errors.append("Active metal-port ledger exceeds the 180-line limit")
    for heading in [
        "## Complete-product advancement assessment",
        "## Exact next actions",
    ]:
        if heading not in current:
            errors.append(f"CURRENT.md missing checkpoint section: {heading}")
    for heading in [
        "## Complete-product checkpoint gate",
        "## Immediate executable action",
        "## Non-negotiable open gates",
    ]:
        if heading not in ledger:
            errors.append(f"Active ledger missing checkpoint section: {heading}")
    current_states = milestone_states(current)
    ledger_states = milestone_states(ledger)
    for milestone in sorted(set(current_states) & set(ledger_states)):
        if current_states[milestone] != ledger_states[milestone]:
            errors.append(
                f"Milestone state mismatch for {milestone}: "
                f"CURRENT={current_states[milestone]} ledger={ledger_states[milestone]}"
            )
    for milestone in ("M-008", "M-012"):
        if milestone not in current_states or milestone not in ledger_states:
            errors.append(f"Current state/ledger must both declare {milestone}")
    current_sha = re.search(
        r"\*\*Renderer code checkpoint:\*\* `([0-9a-f]{10,40})`", current
    )
    ledger_sha = re.search(r"Commit `([0-9a-f]{10,40})`", ledger)
    if not current_sha or not ledger_sha:
        errors.append("Current state/ledger must identify the renderer checkpoint commit")
    elif current_sha.group(1) != ledger_sha.group(1):
        errors.append("Current state/ledger renderer checkpoint commits differ")
    return errors


def validate_release_invariants(root: Path, required_ids=REQUIRED_INVARIANTS):
    """Validate declarative coverage of the immutable product/release contract."""
    contract_path = root / "docs/metal/release-invariants.json"
    if not contract_path.is_file():
        return []
    errors = []
    try:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        if contract.get("version") != 1:
            errors.append("release-invariants.json version must be 1")
        source_path = root / contract["source"]
        source = source_path.read_text(encoding="utf-8")
        invariants = contract["invariants"]
    except (KeyError, TypeError, ValueError, OSError, json.JSONDecodeError) as exc:
        return [f"Invalid release-invariants.json: {exc}"]
    ids = [item.get("id") for item in invariants if isinstance(item, dict)]
    missing = set(required_ids) - set(ids)
    if missing:
        errors.append(
            "Missing required release invariants: " + ", ".join(sorted(missing))
        )
    if len(ids) != len(set(ids)):
        errors.append("Duplicate release invariant ID")
    for item in invariants:
        invariant_id = item.get("id", "<missing>")
        for pattern in item.get("source_patterns", []):
            if not re.search(pattern, source, re.IGNORECASE | re.DOTALL):
                errors.append(
                    f"Product source no longer covers {invariant_id}: {pattern}"
                )
        coverage = item.get("coverage", {})
        if not coverage:
            errors.append(f"Release invariant has no coverage documents: {invariant_id}")
        for relative, patterns in coverage.items():
            path = root / relative
            if not path.is_file():
                errors.append(
                    f"Release invariant {invariant_id} references missing {relative}"
                )
                continue
            text = path.read_text(encoding="utf-8")
            for pattern in patterns:
                if not re.search(pattern, text, re.IGNORECASE | re.DOTALL):
                    errors.append(
                        f"Invariant coverage missing in {relative} for "
                        f"{invariant_id}: {pattern}"
                    )
    return errors


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
    errors.extend(validate_live_references(root))
    errors.extend(validate_current_documents(root))
    errors.extend(validate_release_invariants(root))
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
