#!/usr/bin/env python3
"""Verify a packaged native viewer and preserve honest static release evidence."""

import argparse
import datetime
import hashlib
import json
import mmap
import os
from pathlib import Path
import platform
import plistlib
import re
import subprocess
import sys

OPENHT_REVISION = "72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2"
TAG_PATTERN = re.compile(r"^([0-9]+\.[0-9]+\.[0-9]+)\.([0-9]+)-alpha([1-9][0-9]*)$")
FORBIDDEN_LIBRARY = re.compile(r"OpenGL|OpenGLES|Vulkan|MoltenVK|libGL\.|libvulkan\.", re.IGNORECASE)
FORBIDDEN_SYMBOL = re.compile(r"\b_(?:gl[A-Z]\w*|CGL\w*|agl[A-Z]\w*|SDL_GL\w*|vk[A-Z]\w*)\b")


class ValidationError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ValidationError(message)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def contains_bytes(path, value):
    with Path(path).open("rb") as stream:
        if not Path(path).stat().st_size:
            return False
        with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapping:
            return mapping.find(value) >= 0


def command(argv, report, *, keep_output=False, optional=False):
    argv = [str(arg) for arg in argv]
    try:
        result = subprocess.run(argv, capture_output=True, text=True, encoding="utf-8", errors="replace")
    except OSError as error:
        report["commands"].append(dict(argv=argv, exit=None, stderr=str(error)))
        if optional:
            return ""
        raise ValidationError(f"Cannot run {argv[0]}: {error}") from error
    row = dict(argv=argv, exit=result.returncode, stderr=result.stderr[:4096],
               stdout_bytes=len(result.stdout.encode()),
               stdout_sha256=hashlib.sha256(result.stdout.encode()).hexdigest())
    if keep_output:
        row["stdout"] = result.stdout
    report["commands"].append(row)
    if result.returncode and not optional:
        raise ValidationError(f"Command failed ({result.returncode}): {' '.join(argv)}")
    return result.stdout


def bundle_executable(bundle, app):
    with (bundle / "Contents/Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    require(isinstance(info, dict), f"Invalid Info.plist: {bundle}")
    name = info.get("CFBundleExecutable")
    require(isinstance(name, str) and name not in ("", ".", "..") and
            Path(name).name == name and "\x00" not in name,
            f"Invalid CFBundleExecutable: {bundle}")
    executable = (bundle / "Contents/MacOS" / name).resolve()
    require(executable.is_relative_to(app), f"Bundle executable escapes application: {bundle}")
    require(executable.is_file(), f"Missing executable: {executable}")
    return executable, info


def validate_openht_receipt(path, report):
    receipt = json.loads(path.read_text())
    require(isinstance(receipt, dict), "OpenHTJ2K receipt must be an object")
    expected = dict(source_revision=OPENHT_REVISION, architecture="arm64", configuration="Release",
                    deployment_target="26.0", portable_tuning="apple-m1")
    require(type(receipt.get("schema_version")) is int and receipt["schema_version"] == 1,
            "Unsupported OpenHTJ2K receipt schema")
    for name, value in expected.items():
        require(receipt.get(name) == value, f"OpenHTJ2K receipt {name} does not match {value}")
    require(receipt.get("source_clean") is True and receipt.get("success") is True,
            "OpenHTJ2K receipt must describe a successful clean build")
    for name in ("source_dir", "library_path"):
        require(isinstance(receipt.get(name), str) and Path(receipt[name]).is_absolute(),
                f"OpenHTJ2K receipt {name} must be absolute")
    require(isinstance(receipt.get("library_sha256"), str) and
            re.fullmatch(r"[0-9a-f]{64}", receipt["library_sha256"]),
            "OpenHTJ2K receipt has an invalid archive digest")
    record = dict(receipt_path=str(path), receipt_sha256=sha256(path), receipt=receipt,
                  archive_present=False, archive_hash_verified=False,
                  linked_archive_identity="Not recoverable from final Mach-O; build-time CMake receipt gate supplies link provenance",
                  runtime_decoder_verified=False)
    report["openhtj2k"] = record
    library = Path(receipt["library_path"])
    if library.is_file():
        record["archive_present"] = True
        require(sha256(library) == receipt["library_sha256"], "OpenHTJ2K archive digest does not match receipt")
        with library.open("rb") as stream:
            require(stream.read(8) == b"!<arch>\n", "OpenHTJ2K library is not a static archive")
        architectures = command(["xcrun", "lipo", "-archs", library], report).strip()
        require(architectures == "arm64", "OpenHTJ2K archive is not ARM64-only")
        record["archive_hash_verified"] = True
    else:
        record["archive_verification_limit"] = "Original build-time archive is unavailable on this machine"
    return receipt


def validate_license(app, receipt, report):
    notice = app / "Contents/Resources/licenses/OpenHTJ2K.txt"
    require(not notice.is_symlink() and notice.is_file(), "Missing packaged OpenHTJ2K license: Resources/licenses/OpenHTJ2K.txt")
    expected = receipt.get("license_sha256")
    source_license = Path(receipt["source_dir"]) / "LICENSE"
    if source_license.is_file():
        source_digest = sha256(source_license)
        require(expected is None or expected == source_digest, "OpenHTJ2K receipt license digest differs from source LICENSE")
        expected = source_digest
    require(isinstance(expected, str) and re.fullmatch(r"[0-9a-f]{64}", expected),
            "Cannot establish pinned OpenHTJ2K source license digest")
    actual = sha256(notice)
    require(actual == expected, "Packaged OpenHTJ2K license differs from pinned source LICENSE")
    report["openhtj2k"]["packaged_license"] = dict(path=str(notice), sha256=actual, source_match=True)


def verify_release(app, source_sha, tag, receipt_path, *, report=None):
    report = report if report is not None else {}
    report.update(schema_version=1, success=False, commands=[], source_sha=source_sha, tag=tag,
                  checked_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  host=dict(system=platform.system(), machine=platform.machine(), release=platform.release()),
                  validation_scope="Static packaged-app identity/import/compiled-feature and dependency-receipt evidence; no app launch or runtime parity claim")
    require(isinstance(source_sha, str) and re.fullmatch(r"[0-9a-f]{40}", source_sha), "Source SHA must be 40 lowercase hexadecimal characters")
    report["external_revisions"] = {}
    template_revision = os.environ.get("MASTER_MESSAGE_TEMPLATE_SHA")
    if template_revision is not None:
        require(re.fullmatch(r"[0-9a-f]{40}", template_revision), "MASTER_MESSAGE_TEMPLATE_SHA must be a full commit SHA")
        report["external_revisions"]["master_message_template"] = template_revision
    match = TAG_PATTERN.fullmatch(tag)
    require(match is not None, "Release tag must be VERSION.REVISION-alphaN (for example 26.4.0.68898-alpha5)")
    app, receipt_path = Path(app).resolve(), Path(receipt_path).resolve()
    require(app.is_dir(), f"Missing application bundle: {app}")
    main, info = bundle_executable(app, app)
    report.update(app=str(app), executable=str(main), executable_sha256=sha256(main), info=info)
    version = f"{match[1]}.{match[2]}"
    require(info.get("CFBundleShortVersionString") == version,
            f"Packaged version {info.get('CFBundleShortVersionString')!r} does not match tag version {version}")
    bundle_version = info.get("CFBundleVersion")
    require(isinstance(bundle_version, str) and re.fullmatch(re.escape(match[1]) + r"(?:[dabf]|\.)" + re.escape(match[2]), bundle_version),
            "CFBundleVersion does not match the requested base version and revision")
    require(contains_bytes(main, source_sha.encode("ascii")), "Main executable does not embed requested source SHA")
    report["embedded_source_sha_verified"] = True
    receipt = validate_openht_receipt(receipt_path, report)
    bundles = [app]
    for name in ("media_plugin_cef.app", "media_plugin_libvlc.app"):
        matches = [path for path in app.rglob(name) if path.is_dir() and not path.is_symlink()]
        require(len(matches) == 1, f"Expected exactly one owned media host {name}; found {len(matches)}")
        bundles.extend(matches)
    report["owned_bundles"] = []
    for bundle in bundles:
        binary, bundle_info = bundle_executable(bundle, app)
        libraries = command(["otool", "-L", binary], report, keep_output=True)
        undefined = command(["nm", "-u", binary], report)
        architectures = command(["xcrun", "lipo", "-archs", binary], report).strip()
        require(architectures == "arm64", f"Owned executable is not ARM64-only: {binary}")
        require(not FORBIDDEN_LIBRARY.search(libraries), f"Forbidden direct graphics dependency: {binary}")
        require(not FORBIDDEN_SYMBOL.search(undefined), f"Forbidden owned graphics import: {binary}")
        if bundle == app:
            require("/Metal.framework/" in libraries, "Viewer must link native Metal")
        report["owned_bundles"].append(dict(bundle=str(bundle), executable=str(binary), sha256=sha256(binary),
                                            architectures=architectures, imports_verified=True,
                                            identifier=bundle_info.get("CFBundleIdentifier")))
    symbols = command(["nm", "-m", main], report)
    defined = "\n".join(line for line in symbols.splitlines() if "(undefined)" not in line)
    markers = {"native_music": "LLStreamingAudio_CoreAudio", "webrtc_voice": "LLWebRTCVoiceClient",
               "openht_decoder": "openhtj2k_decoder", "openjpeg_encoder": "_opj_encode"}
    report["compiled_features"] = {}
    for feature, marker in markers.items():
        lines = [line for line in defined.splitlines() if marker in line]
        if feature == "openjpeg_encoder":
            lines = [line for line in lines if re.search(r"\b_opj_encode\b", line)]
        report["compiled_features"][feature] = dict(defined_symbol_count=len(lines), examples=lines[:3])
        require(lines, f"Missing compiled {feature} symbol evidence")
    selected_marker = b"OpenHTJ2K 0.27.0 decode;"
    require(contains_bytes(main, selected_marker), "Missing selected OpenHTJ2K decoder engine-info marker")
    report["compiled_features"]["openht_engine_info_marker"] = selected_marker.decode()
    validate_license(app, receipt, report)
    report["tool_versions"] = {
        "xcode": command(["xcodebuild", "-version"], report, optional=True).strip(),
        "sdk": command(["xcrun", "--sdk", "macosx", "--show-sdk-version"], report, optional=True).strip(),
        "compiler": command(["xcrun", "clang", "--version"], report, optional=True).strip(),
        "cmake": command(["cmake", "--version"], report, optional=True).splitlines()[:1],
    }
    report["success"] = True
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--openht-receipt", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    report = {}
    try:
        verify_release(args.app, args.source_sha, args.tag, args.openht_receipt, report=report)
    except (ValidationError, OSError, ValueError, plistlib.InvalidFileException) as error:
        report.update(success=False, error=str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    if report.get("success"):
        print(f"Verified native release {args.tag}: {report['executable_sha256']}")
        return 0
    print(f"ERROR: {report.get('error', 'release validation failed')}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
