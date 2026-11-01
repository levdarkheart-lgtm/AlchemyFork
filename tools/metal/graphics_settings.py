#!/usr/bin/env python3
"""Capture and compare effective viewer graphics settings for paired evidence."""

import argparse
import hashlib
import json
import math
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


SCHEMA_VERSION = 1

# These settings identify caches, implementation choices or diagnostics rather
# than the visual-quality contract of a paired capture.
IGNORED_RENDER_SETTINGS = {
    "RenderAppleUseMultGL",
    "RenderDebugGL",
    "RenderDebugPipeline",
    "RenderGLMultiThreadedMedia",
    "RenderInitError",
    "RenderMetalNativeOnly",
    "RenderShaderCacheVersion",
}

# Missing any of these makes a profile unsuitable for a controlled comparison.
REQUIRED_SETTINGS = {
    "RenderAnisotropicLevel",
    "RenderAvatarLODFactor",
    "RenderAvatarMaxComplexity",
    "RenderAvatarMaxNonImpostors",
    "RenderDeferredSSAO",
    "RenderFarClip",
    "RenderFSAASamples",
    "RenderFSAAType",
    "RenderHeroProbeDistance",
    "RenderHeroProbeResolution",
    "RenderLocalLightCount",
    "RenderMaxPartCount",
    "RenderMaxTextureResolution",
    "RenderMirrors",
    "RenderQualityPerformance",
    "RenderReflectionProbeCount",
    "RenderReflectionProbeDetail",
    "RenderShadowDetail",
    "RenderTerrainLODFactor",
    "RenderTerrainPBREnabled",
    "RenderTransparentWater",
    "RenderVolumeLODFactor",
}


def parse_value(element):
    """Convert the LLSD XML value types used by viewer settings."""
    tag = element.tag
    text = element.text or ""
    if tag in {"string", "uri", "uuid", "date"}:
        return text
    if tag == "boolean":
        return text.strip().lower() in {"1", "true"}
    if tag == "integer":
        return int(text.strip() or "0")
    if tag == "real":
        value = float(text.strip() or "0")
        if not math.isfinite(value):
            raise ValueError("non-finite real in settings LLSD")
        return value
    if tag == "array":
        return [parse_value(child) for child in element]
    if tag == "map":
        return parse_map(element)
    if tag == "undef":
        return None
    raise ValueError(f"unsupported LLSD value type: {tag}")


def parse_map(element):
    children = list(element)
    if len(children) % 2:
        raise ValueError("LLSD map has an unpaired key")
    result = {}
    for index in range(0, len(children), 2):
        key_element = children[index]
        if key_element.tag != "key":
            raise ValueError("LLSD map entry does not start with a key")
        result[key_element.text or ""] = parse_value(children[index + 1])
    return result


def load_settings(path):
    path = Path(path)
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as exc:
        raise ValueError(f"cannot parse settings {path}: {exc}") from exc
    if root.tag != "llsd" or len(root) != 1 or root[0].tag != "map":
        raise ValueError(f"settings root is not an LLSD map: {path}")
    settings = parse_map(root[0])
    values = {}
    for name, record in settings.items():
        if isinstance(record, dict) and "Value" in record:
            values[name] = record["Value"]
    return values


def source_record(path):
    path = Path(path).resolve()
    return {
        "path": str(path),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }


def is_visual_setting(name):
    return name.startswith("Render") and name not in IGNORED_RENDER_SETTINGS


def build_manifest(defaults, profile, label):
    paths = [Path(path) for path in defaults] + [Path(profile)]
    effective = {}
    for path in paths:
        effective.update(load_settings(path))
    visual = {
        name: value
        for name, value in sorted(effective.items())
        if is_visual_setting(name)
    }
    missing = sorted(REQUIRED_SETTINGS - set(visual))
    if missing:
        raise ValueError(
            "effective settings are missing required graphics keys: "
            + ", ".join(missing)
        )
    return {
        "schema_version": SCHEMA_VERSION,
        "label": label,
        "sources": [source_record(path) for path in paths],
        "settings": visual,
    }


def load_manifest(path):
    path = Path(path)
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read manifest {path}: {exc}") from exc
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"unsupported graphics manifest schema in {path}")
    if not isinstance(manifest.get("settings"), dict):
        raise ValueError(f"graphics manifest has no settings map: {path}")
    return manifest


def compare_manifests(reference, candidate):
    reference_settings = reference["settings"]
    candidate_settings = candidate["settings"]
    common = sorted(set(reference_settings) & set(candidate_settings))
    mismatches = [
        {
            "setting": name,
            "reference": reference_settings[name],
            "candidate": candidate_settings[name],
        }
        for name in common
        if reference_settings[name] != candidate_settings[name]
    ]
    required_missing = sorted(
        name
        for name in REQUIRED_SETTINGS
        if name not in reference_settings or name not in candidate_settings
    )
    reference_only = sorted(set(reference_settings) - set(candidate_settings))
    candidate_only = sorted(set(candidate_settings) - set(reference_settings))
    return {
        "comparable": not (
            mismatches or required_missing or reference_only or candidate_only
        ),
        "reference_label": reference.get("label", "reference"),
        "candidate_label": candidate.get("label", "candidate"),
        "compared_setting_count": len(common),
        "mismatches": mismatches,
        "required_missing": required_missing,
        "reference_only": reference_only,
        "candidate_only": candidate_only,
    }


def write_json(payload, output):
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if output:
        Path(output).write_text(text, encoding="utf-8")
    else:
        print(text, end="")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    capture = subparsers.add_parser(
        "capture", help="merge shipped defaults and a profile into a manifest"
    )
    capture.add_argument(
        "--defaults", action="append", required=True,
        help="shipped settings file; repeat in viewer load order",
    )
    capture.add_argument("--profile", required=True, help="isolated user settings.xml")
    capture.add_argument("--label", required=True)
    capture.add_argument("--output")

    compare = subparsers.add_parser(
        "compare", help="fail unless paired manifests have identical visual settings"
    )
    compare.add_argument("reference")
    compare.add_argument("candidate")
    compare.add_argument("--output")

    args = parser.parse_args(argv)
    try:
        if args.command == "capture":
            manifest = build_manifest(args.defaults, args.profile, args.label)
            write_json(manifest, args.output)
            return 0
        report = compare_manifests(
            load_manifest(args.reference), load_manifest(args.candidate)
        )
        write_json(report, args.output)
        return 0 if report["comparable"] else 1
    except (ValueError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
