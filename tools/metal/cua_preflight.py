#!/usr/bin/env python3
"""Fail closed unless Computer Use is binding the intended isolated viewer."""

import argparse
import ctypes
import hashlib
import os
import plistlib
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import NamedTuple


class Process(NamedTuple):
    pid: int
    ppid: int
    command: str
    executable: str | None = None


def path_is_within(path: Path, root: Path) -> bool:
    return path == root or path.is_relative_to(root)


def ordinary_profile_roots():
    return [
        Path.home() / "Library/Application Support/AlchemyNext",
        Path.home() / "Library/Caches/AlchemyNext",
    ]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def app_identity(app_path: Path):
    app = app_path.expanduser().resolve()
    errors = []
    if app.suffix != ".app" or not app.is_dir():
        return None, [f"Application bundle does not exist: {app}"]
    plist_path = app / "Contents/Info.plist"
    try:
        with plist_path.open("rb") as stream:
            metadata = plistlib.load(stream)
        executable_name = metadata["CFBundleExecutable"]
        version = metadata["CFBundleShortVersionString"]
    except (KeyError, OSError, plistlib.InvalidFileException) as exc:
        return None, [f"Cannot read application identity: {exc}"]
    executable = app / "Contents/MacOS" / executable_name
    if not executable.is_file() or not os.access(executable, os.X_OK):
        errors.append(f"Bundle executable is missing or not executable: {executable}")
    if not any(marker in executable_name for marker in ("Development", "Diagnostic")):
        errors.append(
            "Computer Use validation requires a Development or Diagnostic viewer"
        )
    identity = {
        "app": app,
        "executable": executable,
        "channel": executable_name,
        "version": str(version),
    }
    return identity, errors


def parse_process_table(text: str):
    processes = []
    for line in text.splitlines():
        match = re.match(r"\s*(\d+)\s+(\d+)\s+(.*\S)\s*$", line)
        if match:
            processes.append(
                Process(int(match.group(1)), int(match.group(2)), match.group(3))
            )
    return processes


def read_process_table():
    result = subprocess.run(
        ["/bin/ps", "-www", "-axo", "pid=,ppid=,command="],
        check=True,
        capture_output=True,
        text=True,
    )
    processes = parse_process_table(result.stdout)
    return [
        process._replace(executable=process_executable_path(process.pid))
        if command_is_viewer_main(process.command)
        else process
        for process in processes
    ]


def process_executable_path(pid: int) -> str | None:
    """Return the kernel-reported executable path for a macOS process."""
    if sys.platform != "darwin":
        return None
    try:
        libproc = ctypes.CDLL("/usr/lib/libproc.dylib")
        buffer = ctypes.create_string_buffer(4096)
        libproc.proc_pidpath.argtypes = (
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.c_uint32,
        )
        libproc.proc_pidpath.restype = ctypes.c_int
        length = libproc.proc_pidpath(pid, buffer, ctypes.sizeof(buffer))
    except (OSError, ValueError):
        return None
    if length <= 0:
        return None
    return os.path.realpath(os.fsdecode(buffer.value))


def command_is_exact_executable(
    command: str, executable: Path, actual_executable: str | None = None
) -> bool:
    value = str(executable)
    if command == value or command.startswith(value + " "):
        return True
    if not actual_executable:
        return False
    return os.path.realpath(actual_executable) == os.path.realpath(value)


def command_is_viewer_main(command: str) -> bool:
    marker = ".app/Contents/MacOS/"
    marker_index = command.find(marker)
    if marker_index < 0:
        return False
    bundle_text = command[: marker_index + len(".app")]
    bundle_name = Path(bundle_text).name.lower()
    return bundle_name.startswith(("alchemy", "second life", "firestorm"))


def process_environment(pid: int) -> str:
    result = subprocess.run(
        ["/bin/ps", "eww", "-p", str(pid), "-o", "command="],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def process_open_files(pid: int):
    lsof = shutil.which("lsof")
    if not lsof:
        raise RuntimeError("lsof is required for isolated-profile validation")
    result = subprocess.run(
        [lsof, "-a", "-p", str(pid), "-Fn"],
        check=True,
        capture_output=True,
        text=True,
    )
    return [line[1:] for line in result.stdout.splitlines() if line.startswith("n")]


def validate_runtime_snapshot(
    identity,
    profile: Path,
    processes,
    environment_text: str,
    open_files,
    log_text: str,
):
    errors = []
    executable = identity["executable"]
    exact = [
        process
        for process in processes
        if command_is_exact_executable(
            process.command, executable, process.executable
        )
    ]
    viewers = [process for process in processes if command_is_viewer_main(process.command)]
    if len(exact) != 1:
        errors.append(
            f"Expected one exact viewer process, found {len(exact)}"
        )
    if len(viewers) != 1 or (exact and viewers[0].pid != exact[0].pid):
        errors.append(
            "Viewer census is not target-only; stop every sibling viewer before binding"
        )

    profile_token = f"ALCHEMYNEXT_USER_DIR={profile}"
    if any(character.isspace() for character in str(profile)):
        errors.append("Isolated profile path must not contain whitespace")
    elif profile_token not in environment_text.split():
        errors.append("Target process does not carry the exact isolated-profile root")

    log_path = profile / "logs/Alchemy.log"
    resolved_open_files = {str(Path(item).resolve()) for item in open_files}
    if str(log_path.resolve()) not in resolved_open_files:
        errors.append("Target process does not have the isolated profile log open")

    resolved_profile = profile.resolve()
    resolved_normal_roots = [root.resolve() for root in ordinary_profile_roots()]
    if any(path_is_within(resolved_profile, root) for root in resolved_normal_roots):
        errors.append("Isolated profile root resolves inside an ordinary Alchemy profile")
    for item in resolved_open_files:
        path = Path(item)
        if any(path_is_within(path, root) for root in resolved_normal_roots):
            errors.append("Target process has an ordinary Alchemy profile path open")
            break

    expected_log_start = f"Started logging to {log_path}"
    if expected_log_start not in log_text:
        errors.append("Isolated log does not identify its own expected path")
    expected_version = f"Version: {identity['channel']} {identity['version']}"
    if expected_version not in log_text:
        errors.append("Isolated log has not established the expected channel/version")
    return errors


def inspect_live(identity, profile: Path):
    processes = read_process_table()
    exact = [
        process
        for process in processes
        if command_is_exact_executable(
            process.command, identity["executable"], process.executable
        )
    ]
    if len(exact) != 1:
        return None, [f"Expected one exact viewer process, found {len(exact)}"]
    pid = exact[0].pid
    try:
        environment_text = process_environment(pid)
        open_files = process_open_files(pid)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        return pid, [f"Cannot inspect target process {pid}: {exc}"]
    log_path = profile / "logs/Alchemy.log"
    try:
        log_text = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return pid, [f"Cannot read isolated viewer log: {exc}"]
    return pid, validate_runtime_snapshot(
        identity, profile, processes, environment_text, open_files, log_text
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path, help="exact Development/Diagnostic .app")
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--expected-sha256", required=True)
    parser.add_argument(
        "--wait", type=float, default=20.0,
        help="seconds to wait for process/log readiness (default: 20)",
    )
    args = parser.parse_args()

    identity, errors = app_identity(args.app)
    if identity is None:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    if identity["version"] != args.expected_version:
        errors.append(
            f"Version mismatch: expected {args.expected_version}, "
            f"found {identity['version']}"
        )
    if not re.fullmatch(r"[0-9a-fA-F]{64}", args.expected_sha256):
        errors.append("--expected-sha256 must contain exactly 64 hexadecimal digits")
    else:
        try:
            actual_sha = sha256_file(identity["executable"])
        except OSError as exc:
            errors.append(f"Cannot hash bundle executable: {exc}")
        else:
            if actual_sha != args.expected_sha256.lower():
                errors.append(
                    f"Executable SHA-256 mismatch: expected "
                    f"{args.expected_sha256.lower()}, found {actual_sha}"
                )
    # Preserve the spelling passed to the viewer (notably /tmp versus
    # /private/tmp) for environment and log-marker checks. Canonicalize only
    # when comparing filesystem objects inside validate_runtime_snapshot().
    profile = Path(os.path.abspath(args.profile.expanduser()))
    resolved_profile = profile.resolve()
    if any(
        path_is_within(resolved_profile, root.resolve())
        for root in ordinary_profile_roots()
    ):
        errors.append("Ordinary Alchemy profile is forbidden for Computer Use validation")
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    deadline = time.monotonic() + max(args.wait, 0.0)
    pid = None
    while True:
        pid, errors = inspect_live(identity, profile)
        if not errors or time.monotonic() >= deadline:
            break
        time.sleep(0.25)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print("Computer Use viewer preflight PASS")
    print(f"app: {identity['app']}")
    print(f"pid: {pid}")
    print(f"channel: {identity['channel']}")
    print(f"version: {identity['version']}")
    print(f"executable_sha256: {args.expected_sha256.lower()}")
    print(f"profile: {profile}")
    print(f"log: {profile / 'logs/Alchemy.log'}")
    print("bind_with: exact app path above; never display name or URL scheme")
    return 0


if __name__ == "__main__":
    sys.exit(main())
