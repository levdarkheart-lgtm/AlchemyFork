#!/usr/bin/env python3
"""Summarize labeled Metal command-buffer submissions from an xctrace capture.

The input may be a Metal System Trace ``.trace`` bundle or XML exported from
the ``metal-application-command-buffer-submissions`` table.  Durations are
reported exactly as recorded by Instruments; they are not GPU frame time.
"""
import argparse
import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


TABLE_XPATH = (
    '/trace-toc/run[@number="1"]/data/'
    'table[@schema="metal-application-command-buffer-submissions"]'
)


def _export_trace(path: Path) -> bytes:
    if sys.platform != "darwin":
        raise ValueError("xctrace bundle export requires macOS")
    command = [
        "xcrun", "xctrace", "export", "--quiet", "--input", str(path),
        "--xpath", TABLE_XPATH,
    ]
    try:
        result = subprocess.run(
            command, check=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
    except FileNotFoundError as error:
        raise ValueError("xcrun/xctrace is unavailable") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.decode("utf-8", errors="replace").strip()
        raise ValueError(
            f"xctrace export failed{': ' + detail if detail else ''}") from error
    if not result.stdout.strip():
        raise ValueError("xctrace export returned no submission table")
    return result.stdout


def _xml_bytes(path: Path) -> bytes:
    if path.suffix == ".trace" or path.is_dir():
        return _export_trace(path)
    try:
        return path.read_bytes()
    except OSError as error:
        raise ValueError(str(error)) from error


def _references(root):
    values = {}
    for element in root.iter():
        identifier = element.get("id")
        if identifier:
            values[identifier] = {
                "fmt": element.get("fmt"),
                "text": (element.text or "").strip(),
            }
    return values


def _value(element, references, field):
    value = element.get(field)
    if value is not None:
        return value
    reference = element.get("ref")
    return references.get(reference, {}).get(field) if reference else None


def _duration_ns(element, references):
    raw = (element.text or "").strip()
    if not raw and element.get("ref"):
        raw = references.get(element.get("ref"), {}).get("text", "")
    try:
        value = int(raw)
    except (TypeError, ValueError) as error:
        raise ValueError("submission row has an invalid duration") from error
    if value < 0:
        raise ValueError("submission row has a negative duration")
    return value


def _submission_label(row, references):
    for narrative in row.findall("narrative"):
        formatted = _value(narrative, references, "fmt")
        if not formatted or not formatted.startswith("Committed "):
            continue
        parts = formatted.split('"')
        if len(parts) >= 3 and parts[1].strip():
            return parts[1].strip()
    return None


def summarize_xml(data: bytes, source="export.xml"):
    try:
        root = ET.fromstring(data)
    except ET.ParseError as error:
        raise ValueError(f"invalid xctrace XML: {error}") from error
    references = _references(root)
    labels = defaultdict(lambda: {
        "count": 0, "event_duration_ns": 0, "encoder_duration_ns": 0,
    })
    submission_count = 0
    for row in root.iter("row"):
        label = _submission_label(row, references)
        if label is None:
            continue
        durations = row.findall("duration")
        if not durations:
            raise ValueError("submission row has no duration")
        event_duration = _duration_ns(durations[0], references)
        encoder_duration = _duration_ns(
            durations[1] if len(durations) > 1 else durations[0], references)
        item = labels[label]
        item["count"] += 1
        item["event_duration_ns"] += event_duration
        item["encoder_duration_ns"] += encoder_duration
        submission_count += 1
    if not submission_count:
        raise ValueError("no committed Metal command-buffer submissions found")

    entries = []
    for label, values in labels.items():
        count = values["count"]
        entries.append({
            "label": label,
            "count": count,
            "event_ms": round(values["event_duration_ns"] / 1_000_000, 3),
            "encoder_ms": round(
                values["encoder_duration_ns"] / 1_000_000, 3),
            "mean_encoder_ms": round(
                values["encoder_duration_ns"] / count / 1_000_000, 6),
        })
    entries.sort(key=lambda item: (-item["encoder_ms"], -item["count"],
                                  item["label"]))
    return {
        "source": source,
        "table": "metal-application-command-buffer-submissions",
        "submission_count": submission_count,
        "event_ms": round(sum(
            item["event_duration_ns"] for item in labels.values()) /
            1_000_000, 3),
        "encoder_ms": round(sum(
            item["encoder_duration_ns"] for item in labels.values()) /
            1_000_000, 3),
        "labels": entries,
        "limitations": (
            "Instruments application-submission durations only; not GPU frame "
            "time, not a performance pass gate, and not proof of scene parity."
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path,
                        help="Metal System Trace .trace bundle or exported XML")
    parser.add_argument("--output", type=Path, help="optional JSON output path")
    parser.add_argument("--top", type=int, default=0,
                        help="retain only the N labels with most encoder time")
    args = parser.parse_args()
    if args.top < 0:
        parser.error("--top must be nonnegative")
    try:
        report = summarize_xml(_xml_bytes(args.input), args.input.name)
    except ValueError as error:
        parser.error(str(error))
    if args.top:
        report["labels"] = report["labels"][:args.top]
    result = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(result, encoding="utf-8")
    print(result, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
