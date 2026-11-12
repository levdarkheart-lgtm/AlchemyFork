#!/usr/bin/env bash
# Build the accepted decoder from a clean pinned source; never reuse a foreign
# binary or change the old local experiment's archive identity.
set -euo pipefail
exec python3 - "$@" <<'PY'
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

REVISION = "72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2"
REPOSITORY = "https://github.com/osamu620/OpenHTJ2K.git"

parser = argparse.ArgumentParser(description="Build pinned native ARM64 OpenHTJ2K and its provenance receipt.")
parser.add_argument("--source-dir", type=Path, required=True)
parser.add_argument("--build-dir", type=Path, required=True)
parser.add_argument("--receipt", type=Path, required=True)
parser.add_argument("--jobs", type=int, default=2)
args = parser.parse_args()
if platform.system() != "Darwin" or platform.machine() != "arm64":
    parser.error("this dependency must be built natively on Apple Silicon macOS")
if not 1 <= args.jobs <= 8:
    parser.error("--jobs must be between 1 and 8")
source, build, receipt_path = (p.expanduser().resolve() for p in
                               (args.source_dir, args.build_dir, args.receipt))
if build == source or build.is_relative_to(source) or receipt_path.is_relative_to(source):
    parser.error("build directory and receipt must be outside the clean source checkout")
build.mkdir(parents=True, exist_ok=True)
receipt_path.parent.mkdir(parents=True, exist_ok=True)
attempt_path = receipt_path.with_name(receipt_path.name + ".attempt.json")
record = dict(schema_version=1, source_revision=REVISION, repository=REPOSITORY,
              source_dir=str(source), architecture="arm64", configuration="Release",
              deployment_target="26.0", started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              commands=[], source_clean=False, success=False)

def save_attempt():
    attempt_path.write_text(json.dumps(record, indent=2) + "\n")

def run(argv, *, quiet=False):
    completed = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    row = dict(argv=[str(a) for a in argv], exit=completed.returncode,
               stdout=completed.stdout, stderr=completed.stderr)
    record["commands"].append(row)
    save_attempt()
    if not quiet:
        if completed.stdout: print(completed.stdout, end="", flush=True)
        if completed.stderr: print(completed.stderr, end="", file=sys.stderr, flush=True)
    if completed.returncode:
        raise RuntimeError(f"Command exited {completed.returncode}: {row['argv']}")
    return completed.stdout.strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def check_source():
    if run(["git", "-C", source, "rev-parse", "HEAD"], quiet=True) != REVISION:
        raise RuntimeError("OpenHTJ2K source is not the accepted pinned revision")
    if run(["git", "-C", source, "status", "--porcelain", "--untracked-files=all"], quiet=True):
        raise RuntimeError("OpenHTJ2K source is modified or contains untracked files")

try:
    # A stale successful receipt must never survive a failed rebuild.
    if receipt_path.exists(): receipt_path.unlink()
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "init", source])
        run(["git", "-C", source, "remote", "add", "origin", REPOSITORY])
        run(["git", "-C", source, "fetch", "--depth", "1", "origin", REVISION])
        run(["git", "-C", source, "checkout", "--detach", "FETCH_HEAD"])
    check_source()
    record["source_tree"] = run(["git", "-C", source, "rev-parse", "HEAD^{tree}"], quiet=True)
    record["compiler"] = run(["xcrun", "--find", "clang++"], quiet=True)
    record["compiler_version"] = run([record["compiler"], "--version"], quiet=True)
    record["sdk_version"] = run(["xcrun", "--sdk", "macosx", "--show-sdk-version"], quiet=True)
    record["cmake_version"] = run(["cmake", "--version"], quiet=True).splitlines()[0]
    record["xcode_version"] = run(["xcodebuild", "-version"], quiet=True)
    # The pinned upstream source appends -mtune=native on Apple ARM64. Change
    # the generated configuration at the end of that project, without editing
    # its pinned source. Both tuning and ISA then target the M1 baseline.
    hook = build / "alchemy-portable-options.cmake"
    hook.write_text('''function(alchemy_openhtj2k_portable_flags)
  string(REPLACE "-mtune=native" "-mtune=apple-m1" portable_flags "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${portable_flags}" PARENT_SCOPE)
endfunction()
if(PROJECT_NAME STREQUAL "open_htj2k")
  cmake_language(DEFER CALL alchemy_openhtj2k_portable_flags)
endif()
''')
    run(["cmake", "-S", source, "-B", build, "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=OFF",
         "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=26.0",
         f"-DCMAKE_CXX_COMPILER={record['compiler']}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
         f"-DCMAKE_PROJECT_INCLUDE={hook}", "-DENABLE_ARM_NEON=ON",
         "-DOPENHTJ2K_BUILD_APPS=OFF", "-DOPENHTJ2K_BUILD_TESTS=OFF",
         "-DOPENHTJ2K_RTP=OFF", "-DOPENHTJ2K_QUIC=OFF"])
    compile_commands = build / "compile_commands.json"
    commands_text = compile_commands.read_text()
    if "-mtune=native" in commands_text or "-march=native" in commands_text or "-mcpu=native" in commands_text:
        raise RuntimeError("Generated OpenHTJ2K flags still depend on the runner CPU")
    if "-mtune=apple-m1" not in commands_text or "-arch arm64" not in commands_text:
        raise RuntimeError("Generated OpenHTJ2K flags omit the native M1/ARM64 target")
    run(["cmake", "--build", build, "--config", "Release", "--target", "open_htj2k",
         "--parallel", str(args.jobs)])
    library = build / "libopenhtj2k.a"
    if not library.is_file() or library.read_bytes()[:8] != b"!<arch>\n":
        raise RuntimeError("OpenHTJ2K did not produce the expected static archive")
    if run(["xcrun", "lipo", "-archs", library], quiet=True) != "arm64":
        raise RuntimeError("OpenHTJ2K archive is not ARM64-only")
    check_source()
    record.update(source_clean=True, success=True, library_path=str(library),
                  library_sha256=sha(library), compile_commands_sha256=sha(compile_commands),
                  portable_tuning="apple-m1", finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
    save_attempt()
    receipt_path.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"source_dir": str(source), "library": str(library),
                      "receipt": str(receipt_path), "sha256": record["library_sha256"]}), flush=True)
except (OSError, RuntimeError, ValueError) as error:
    record.update(error=str(error), finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
    save_attempt()
    print(f"ERROR: {error}", file=sys.stderr)
    raise SystemExit(1)
PY
