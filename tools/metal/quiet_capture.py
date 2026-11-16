#!/usr/bin/env python3
"""PPD-318: bounded, attach-only quiet-scene Overview recording on macOS.

This tool never launches a viewer, logs in, changes settings, or deletes evidence.
`preflight` inspects installed tools/templates without recording. `record` is an
explicit profiling operation on an already running, exact executable/PID. Run it
only after owner authorization. Wait for the JSON `owner_cue_ready` event before
asking the owner to log in. `mark --output RUN --phase ...` records observations;
`mark ... --phase stop` requests a graceful stop, without signalling the viewer.

Example (values must come from the actual staged candidate):
  quiet_capture.py record --pid PID --executable /absolute/viewer --exe-sha256 HASH \
    --source-sha COMMIT --output /private/run --seconds 360 --log /absolute/log \
    --diagnostic-jsonl /private/native-run/diagnostic.jsonl

The native diagnostic input is read-only. Its writer owns its bounds and schema.
Capture helpers may write frame captures beside that input; this tool does not
trigger them. All collector output is private (0700 directory/0600 files).
Observed size limits are sampled, including owned recorder-child raw files, and
checked again after exit; they are not a kernel-enforced peak-allocation limit.
Actual exported target timestamps, not the requested duration, qualify retention.
GPU coverage requires positive-frame GPU Begin to End and GPU Active Time rows
separately. A requested native stream must cover those ranges and CPU samples
with calibrated FRAME iteration bounds (one-second boundary tolerance). A live
prefix can cover the interval while its final loss/cutoff census remains unknown.
Powermetrics is off by default; enabling it requires existing root privilege and
supported samplers. The tool never runs sudo or prompts for privilege.
"""

import argparse
import collections
import ctypes
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import plistlib
import re
import select
import signal
import struct
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

MIB = 1024 ** 2
GIB = 1024 ** 3
SCHEMAS = ("time-profile", "metal-perf-overview-layer-duration-metric")
GPU_FAMILIES = ("GPU Begin to End", "GPU Active Time")
NATIVE_COVERAGE_TOLERANCE_NS = 1_000_000_000  # Writer/app-iteration/Overview bucket boundaries.
OVERVIEW_ID = "com.apple.dt.instruments.metal-perf-overview"
OVERVIEW_INSTRUMENTS = {OVERVIEW_ID, "com.apple.xray.instrument-type.device-thermal-state",
                        "com.apple.xray.instrument-type.coresampler2"}


def stamp():
    return {"utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "monotonic_ns": time.monotonic_ns()}


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(MIB), b""):
            result.update(block)
    return result.hexdigest()


def write_json(path, value):
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w") as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write("\n")
    temporary.replace(path)


def append_json(path, value):
    with path.open("a") as stream:
        stream.write(json.dumps(value, allow_nan=False) + "\n")


def checked(argv, timeout=10):
    result = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {argv!r}: {result.stderr[-2000:]}")
    return result.stdout.strip()


def template_receipt(path):
    data = plistlib.loads(path.read_bytes())
    modes = [v for v in data.get("$objects", []) if isinstance(v, dict) and "windowLimit" in v]
    if not modes or any(v["windowLimit"] != 0 for v in modes):
        raise RuntimeError("Overview template has unknown or rolling retention; refusing recording")
    instruments = {v for v in data.get("$objects", []) if isinstance(v, str)
                   and v.startswith(("com.apple.dt.instruments.", "com.apple.xray.instrument-type."))}
    if OVERVIEW_ID not in instruments or not instruments.issubset(OVERVIEW_INSTRUMENTS):
        raise RuntimeError("only the qualified Overview/CPU/thermal template is permitted")
    return {"path": str(path), "sha256": sha256(path), "instrument_ids": sorted(instruments),
            "retention": [{k: v[k] for k in ("windowLimit", "timeLimit", "recordingMode") if k in v}
                          for v in modes]}


def preflight(args):
    if sys.platform != "darwin":
        raise RuntimeError("macOS is required")
    if os.uname().machine != "arm64":
        raise RuntimeError("native ARM64 host is required")
    developer = Path(checked(["xcode-select", "-p"]))
    template = Path(args.template).resolve() if args.template else (
        developer.parent / "Applications/Instruments.app/Contents/Packages/GPU.instrdst/Contents/Templates/"
        "Game Performance Overview.tracetemplate")
    result = {"observed": stamp(), "template": template_receipt(template),
              "xctrace": checked(["xcrun", "--find", "xctrace"]),
              "xcode": checked(["xcodebuild", "-version"]),
              "os": checked(["sw_vers"]), "machine": os.uname().machine,
              "collector_sha256": sha256(__file__), "recording_performed": False,
              "thread_states_requested": args.thread_states,
              "powermetrics": {"requested": args.powermetrics, "available": False,
                               "effective_uid": os.geteuid(),
                               "limitation": "not requested; no privileged sampling attempted"}}
    if args.thread_states:
        instruments = checked(["xcrun", "xctrace", "list", "instruments"])
        if "Thread State Trace" not in {line.strip() for line in instruments.splitlines()}:
            raise RuntimeError("requested Thread State Trace instrument unavailable")
        result["additional_instruments"] = ["Thread State Trace"]
    if args.powermetrics:
        help_text = checked(["/usr/bin/powermetrics", "--help"])
        required = ("cpu_power", "gpu_power", "thermal", "--sample-count", "--format")
        if not all(item in help_text for item in required) or os.geteuid() != 0:
            raise RuntimeError("powermetrics requested but supported samplers/root privilege unavailable; no sudo attempted")
        result["powermetrics"].update(available=True, help_sha256=hashlib.sha256(help_text.encode()).hexdigest(),
                                      samplers=["cpu_power", "gpu_power", "thermal"], limitation=None)
    return result


def process_identity(pid, executable=None, expected_hash=None):
    """proc_pidpath is untruncated, unlike ps's display command/name fields."""
    if pid <= 1 or pid == os.getpid():
        raise RuntimeError("a pre-existing target PID greater than 1 is required")
    library = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
    library.proc_pidpath.argtypes = (ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32)
    library.proc_pidpath.restype = ctypes.c_int
    buffer = ctypes.create_string_buffer(4096)
    if library.proc_pidpath(pid, buffer, len(buffer)) <= 0:
        raise RuntimeError("target process unavailable or inaccessible")
    path = Path(os.fsdecode(buffer.value)).resolve(strict=True)
    if executable is not None and path != Path(executable).resolve(strict=True):
        raise RuntimeError(f"target executable mismatch: {path}")
    start = checked(["ps", "-p", str(pid), "-o", "lstart="])
    if not start:
        raise RuntimeError("target start identity unavailable")
    result = {"pid": pid, "executable": str(path), "process_start": start}
    if expected_hash is not None:
        actual = sha256(path)
        if actual != expected_hash.lower():
            raise RuntimeError("target executable SHA256 mismatch")
        result["executable_sha256"] = actual
    return result


class StartNotification:
    """Real Darwin notification; no first-check false positives or log wording."""
    def __init__(self):
        self.name = "org.alchemy.quiet-capture." + uuid.uuid4().hex
        self.library = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
        self.library.notify_register_file_descriptor.argtypes = (
            ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.c_int, ctypes.POINTER(ctypes.c_int))
        self.library.notify_register_file_descriptor.restype = ctypes.c_uint32
        self.library.notify_cancel.argtypes = (ctypes.c_int,)
        self.library.notify_cancel.restype = ctypes.c_uint32
        descriptor, token = ctypes.c_int(-1), ctypes.c_int(-1)
        status = self.library.notify_register_file_descriptor(self.name.encode(), ctypes.byref(descriptor),
                                                               0, ctypes.byref(token))
        if status:
            raise RuntimeError(f"tracing-start notification registration failed: {status}")
        self.fd, self.token = descriptor.value, token.value
        self.pending = b""

    def received(self):
        if select.select([self.fd], [], [], 0)[0]:
            self.pending += os.read(self.fd, 4096)
        while len(self.pending) >= 4:
            raw, self.pending = self.pending[:4], self.pending[4:]
            # Darwin notify writes network byte order (man notify(3)).
            if struct.unpack("!I", raw)[0] == self.token:
                return True
        return False

    def close(self):
        # notify_cancel closes the descriptor allocated by libnotify. Closing it
        # again can close an unrelated FD if its number has already been reused.
        self.library.notify_cancel(self.token)


def record_argv(args, flight, notification):
    return [flight["xctrace"], "record", "--template", flight["template"]["path"],
            *(["--instrument", "Thread State Trace"] if args.thread_states else []),
            "--attach", str(args.pid), "--time-limit", f"{args.seconds}s", "--output",
            str(args.output / "overview.trace"), "--no-prompt", "--notify-tracing-started", notification]


def tree_size(paths):
    sizes = {}
    for root in paths:
        candidates = root.rglob("*") if root.is_dir() else (root,)
        for path in candidates:
            try:
                stat = path.stat()
                if path.is_file():
                    sizes[(stat.st_dev, stat.st_ino)] = stat.st_size
            except FileNotFoundError:
                pass
    return sum(sizes.values())


def raw_size(trace, recorder_pid):
    """Observe only this recorder's descendants and its output, never all /tmp."""
    pids, pending = {recorder_pid}, [recorder_pid]
    while pending:
        parent = pending.pop()
        child = subprocess.run(["pgrep", "-P", str(parent)], capture_output=True, text=True, timeout=3)
        if child.returncode not in (0, 1):
            raise RuntimeError("cannot enumerate owned recorder children")
        for item in child.stdout.split():
            pid = int(item)
            if pid not in pids:
                pids.add(pid)
                pending.append(pid)
        if len(pids) > 64:
            raise RuntimeError("unexpected recorder process tree")
    opened = subprocess.run(["lsof", "-p", ",".join(map(str, sorted(pids))), "-Fn"],
                            capture_output=True, text=True, timeout=5)
    if opened.returncode not in (0, 1):
        raise RuntimeError("cannot inspect owned recorder raw files")
    paths = {trace}
    paths.update(Path(row[1:]) for row in opened.stdout.splitlines()
                 if row.startswith("n/") and row.endswith((".ktrace", ".atrc")))
    return tree_size(paths), sorted(pids)


def terminate_owned(process, grace=5):
    """Only processes created with start_new_session=True are passed here."""
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=grace)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait(timeout=grace)
    return process.returncode


class Tail:
    def __init__(self, source, destination, cap):
        self.source, self.destination, self.cap = Path(source), destination, cap
        self.offset, self.total, self.identity = 0, 0, None
        self.generations = []

    def poll(self):
        try:
            info = self.source.stat()
        except FileNotFoundError:
            return
        with self.source.open("rb") as incoming, self.destination.open("ab") as outgoing:
            info = os.fstat(incoming.fileno())
            identity = (info.st_dev, info.st_ino)
            if identity != self.identity or info.st_size < self.offset:
                self.identity, self.offset = identity, 0
                self.generations.append({**stamp(), "identity": list(identity), "destination_offset": self.total})
            incoming.seek(self.offset)
            # Bound even a producer writing continuously faster than this reader.
            remaining = info.st_size - self.offset
            if remaining + self.total > self.cap:
                raise RuntimeError(f"input retention size limit: {self.source}")
            while remaining:
                block = incoming.read(min(MIB, remaining))
                if not block:
                    break
                outgoing.write(block)
                self.offset += len(block)
                self.total += len(block)
                remaining -= len(block)

    def receipt(self):
        return {"source": str(self.source), "copy": str(self.destination), "bytes": self.total,
                "source_offset": self.offset, "generations": self.generations,
                "sha256": sha256(self.destination) if self.destination.exists() else None}


def diagnostic_coverage(path, cap=64 * MIB):
    result = {"records": 0, "malformed": 0, "events": {}, "first_mach_ns": None,
              "last_mach_ns": None, "header": None, "partial_final_line": False,
              "path": str(path), "present": path.exists(), "read_only": True,
              "markers": [], "footer": None, "frame_records": 0, "invalid_frame_records": 0,
              "first_frame_start_mach_ns": None, "last_frame_end_mach_ns": None,
              "maximum_frame_gap_ns": 0}
    previous_frame_end = None
    counts = collections.Counter()
    if not path.exists():
        return result
    with path.open("rb") as stream:
        before = os.fstat(stream.fileno())
        if before.st_size > cap:
            raise RuntimeError("native diagnostic input exceeds its declared size cap")
        remaining = before.st_size
        digest = hashlib.sha256()
        while remaining:
            line = stream.readline(remaining)
            if not line:
                break
            remaining -= len(line)
            digest.update(line)
            if not line.endswith(b"\n"):
                result["partial_final_line"] = True
                continue
            try:
                row = json.loads(line)
                if not isinstance(row, dict):
                    raise ValueError("not an object")
                event = row.get("event", row.get("type", "unknown"))
                counts[str(event)] += 1
                if event == "header":
                    result["header"] = row
                if event == "footer":
                    result["footer"] = row
                if event == "marker" and len(result["markers"]) < 128:
                    result["markers"].append(row)
                if event == "frame":
                    start, end = row.get("start_mach_ns"), row.get("end_mach_ns")
                    if isinstance(start, int) and isinstance(end, int) and 0 <= start <= end:
                        result["frame_records"] += 1
                        first, last = result["first_frame_start_mach_ns"], result["last_frame_end_mach_ns"]
                        result["first_frame_start_mach_ns"] = start if first is None else min(first, start)
                        result["last_frame_end_mach_ns"] = end if last is None else max(last, end)
                        if previous_frame_end is not None:
                            result["maximum_frame_gap_ns"] = max(result["maximum_frame_gap_ns"], start - previous_frame_end)
                        previous_frame_end = end
                    else:
                        result["invalid_frame_records"] += 1
                value = row.get("mach_ns")
                if isinstance(value, int):
                    result["first_mach_ns"] = value if result["first_mach_ns"] is None else min(value, result["first_mach_ns"])
                    result["last_mach_ns"] = value if result["last_mach_ns"] is None else max(value, result["last_mach_ns"])
                result["records"] += 1
            except (ValueError, TypeError):
                result["malformed"] += 1
        after = os.fstat(stream.fileno())
    result.update(observed_bytes=before.st_size - remaining, source_size_after=after.st_size,
                  prefix_sha256=digest.hexdigest(), source_identity=[before.st_dev, before.st_ino],
                  unchanged_during_read=(before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns))
    result["events"] = dict(counts)
    header = result["header"] or {}
    result["calibration_available"] = (header.get("schema") == 1 and
        header.get("cpu_clock") == "mach_absolute_time_ns" and
        all(isinstance(header.get(key), int) for key in ("calibration_before_mach_ns",
            "calibration_unix_ns", "calibration_after_mach_ns")) and
        header["calibration_after_mach_ns"] >= header["calibration_before_mach_ns"])
    result["stream_state"] = "footer_observed" if result["footer"] else "live_or_missing_footer"
    footer = result["footer"] or {}
    result["loss_counters"] = {key: footer.get(key) for key in (
        "dropped_full", "dropped_contention", "dropped_budget", "io_errors",
        "pending_created_at_cutoff", "pending_committed_at_cutoff", "names_truncated")}
    result["loss_observed"] = any(isinstance(footer.get(key), int) and footer[key] > 0
                                   for key in ("dropped_full", "dropped_contention", "dropped_budget", "io_errors"))
    result["interpretation"] = "Read-only original-file prefix snapshot; native schema/calibration and drop/footer records determine validity"
    return result


def utc_unix_ns(value):
    parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        raise ValueError("UTC timestamp requires an explicit timezone")
    epoch = dt.datetime(1970, 1, 1, tzinfo=dt.timezone.utc)
    delta = parsed - epoch
    return (delta.days * 86400 + delta.seconds) * 1_000_000_000 + delta.microseconds * 1000


def qualify_native(coverage, overview, source_sha, expected_pid):
    """Qualify actual FRAME iteration bounds; completion timestamps cannot fill holes."""
    result = {"native_coverage_status": "unavailable", "covered": False,
              "tolerance_ns": NATIVE_COVERAGE_TOLERANCE_NS,
              "stop_reason": (coverage.get("footer") or {}).get("stop_reason"),
              "partial_final_line": coverage.get("partial_final_line", False),
              "footer_observed": coverage.get("footer") is not None,
              "uncertainty": "A live prefix has no final loss/cutoff census; partial final line is not a complete event"}
    header = coverage.get("header") or {}
    if (not coverage.get("present") or not coverage.get("calibration_available") or
            header.get("source_sha", "").lower() != source_sha.lower()):
        result["native_coverage_status"] = "missing_uncalibrated_or_source_mismatch"
        return result
    result.update(expected_process_id=expected_pid, native_process_id=header.get("process_id"))
    if not isinstance(header.get("process_id"), int) or header["process_id"] <= 1:
        result["native_coverage_status"] = "native_binding_unproved"
        return result
    if header["process_id"] != expected_pid:
        result["native_coverage_status"] = "native_pid_mismatch"
        return result
    if coverage["malformed"] or coverage["invalid_frame_records"]:
        result["native_coverage_status"] = "malformed"
        return result
    if coverage["loss_observed"]:
        result["native_coverage_status"] = "loss_observed"
        return result
    if not coverage["frame_records"]:
        result["native_coverage_status"] = "no_frame_iteration_records"
        return result
    bounds = overview.get("target_clock_bounds") or {}
    if "first_unix_ns" not in bounds or "last_end_unix_ns" not in bounds:
        result["native_coverage_status"] = "overview_clock_bounds_unavailable"
        return result
    before, after = header["calibration_before_mach_ns"], header["calibration_after_mach_ns"]
    midpoint = (before + after) // 2
    offset = header["calibration_unix_ns"] - midpoint
    first = coverage["first_frame_start_mach_ns"] + offset
    last = coverage["last_frame_end_mach_ns"] + offset
    short_start = max(0, first - bounds["first_unix_ns"])
    short_end = max(0, bounds["last_end_unix_ns"] - last)
    result.update(first_frame_start_unix_ns=first, last_frame_end_unix_ns=last,
                  required_first_unix_ns=bounds["first_unix_ns"], required_last_end_unix_ns=bounds["last_end_unix_ns"],
                  frame_records=coverage["frame_records"], calibration_uncertainty_ns=(after - before + 1) // 2,
                  start_shortfall_ns=short_start, end_shortfall_ns=short_end,
                  maximum_frame_gap_ns=coverage["maximum_frame_gap_ns"],
                  bounds_meaning="FRAME app-iteration wall bounds versus actual target CPU samples/GPU Overview buckets; not GPU execution timestamps")
    if max(short_start, short_end, coverage["maximum_frame_gap_ns"], (after - before + 1) // 2) > NATIVE_COVERAGE_TOLERANCE_NS:
        result["native_coverage_status"] = "incomplete_frame_iteration_coverage"
        return result
    result.update(covered=True, native_coverage_status=("covered_live_prefix" if coverage["footer"] is None
                  else "covered_footer_" + str(result["stop_reason"])))
    return result


def bounded_export(trace, output, selector, deadline, cap):
    command = ["xcrun", "xctrace", "export", "--input", str(trace), *selector, "--output", str(output)]
    receipt = {"argv": command, "start": stamp(), "status": "failed", "exit": None}
    process = None
    begin = time.monotonic()
    try:
        with output.with_suffix(".export.log").open("xb") as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            while True:
                size = output.stat().st_size if output.exists() else 0
                if size > cap or log.tell() > 16 * MIB:
                    raise RuntimeError("export size limit")
                code = process.poll()
                if code is not None:
                    if code:
                        raise RuntimeError(f"export failed, exit {code}")
                    if not output.exists() or not size:
                        raise RuntimeError("export produced no data")
                    receipt.update(status="complete", bytes=size, sha256=sha256(output))
                    break
                if time.monotonic() - begin > deadline:
                    raise RuntimeError("export deadline")
                time.sleep(.1)
    except BaseException as error:
        receipt["failure"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if process is not None:
            receipt["exit"] = terminate_owned(process)
        receipt.update(end=stamp(), elapsed_seconds=time.monotonic() - begin,
                       final_bytes=output.stat().st_size if output.exists() else 0)
        write_json(output.with_suffix(".receipt.json"), receipt)
    return receipt


def table_coverage(path, pid):
    """Stream bounded XML, resolving xctrace references without holding stacks."""
    references, columns = {}, []
    rows = matched = 0
    first = last = previous = None
    maximum_gap = 0
    gpu_families = {}
    def resolve(element):
        if element.get("ref"):
            return references.get(element.get("ref"), ("", None))
        owner = element.find(".//pid")
        return ((element.text or "").strip(), int(owner.text) if owner is not None else None)
    for _, element in ET.iterparse(path, events=("end",)):
        if element.get("id"):
            references[element.get("id")] = resolve(element)
        if element.tag == "schema":
            columns = [column.findtext("mnemonic") for column in element.findall("col")]
        if element.tag == "row":
            rows += 1
            values = dict(zip(columns, map(resolve, element)))
            if pid in [value[1] for key, value in values.items() if key in ("thread", "process")]:
                matched += 1
                value = next((values[key][0] for key in ("start", "time", "timestamp") if key in values), "")
                if value.lstrip("-").isdigit():
                    start = int(value)
                    duration = values.get("duration", ("0", None))[0]
                    end = start + (int(duration) if duration.isdigit() else 0)
                    first = start if first is None else min(first, start)
                    last = end if last is None else max(last, end)
                    if previous is not None:
                        maximum_gap = max(maximum_gap, start - previous)
                    previous = start
                    family = values.get("name", (None, None))[0]
                    frames = values.get("number-of-frames", ("0", None))[0]
                    metric = values.get("value-in-ms", ("", None))[0]
                    try:
                        valid_gpu = (int(frames) > 0 and math.isfinite(float(metric)) and float(metric) >= 0
                                     and duration.isdigit() and int(duration) > 0)
                    except ValueError:
                        valid_gpu = False
                    if family in GPU_FAMILIES and valid_gpu:
                        item = gpu_families.setdefault(family, {"target_rows": 0, "first_ns": start,
                            "last_end_ns": end, "maximum_consecutive_start_gap_seconds": 0, "previous": start})
                        item["target_rows"] += 1
                        item["first_ns"] = min(item["first_ns"], start)
                        item["last_end_ns"] = max(item["last_end_ns"], end)
                        item["maximum_consecutive_start_gap_seconds"] = max(
                            item["maximum_consecutive_start_gap_seconds"], (start - item["previous"]) / 1e9)
                        item["previous"] = start
            element.clear()
    for item in gpu_families.values():
        item.pop("previous")
        item["span_seconds"] = (item["last_end_ns"] - item["first_ns"]) / 1e9
    return {"rows": rows, "target_rows": matched, "first_ns": first, "last_end_ns": last,
            "span_seconds": None if first is None else (last - first) / 1e9,
            "maximum_consecutive_start_gap_seconds": maximum_gap / 1e9,
            "gpu_families": gpu_families,
            "limitation": "Timestamp span is not continuity, hardware occupancy, or wall-frame quantiles"}


def qualify(args, expected_seconds):
    trace = args.output / "overview.trace"
    exports, ranges = {}, {}
    toc = args.output / "toc.xml"
    exports["toc"] = bounded_export(trace, toc, ["--toc"], args.export_timeout, args.export_cap_mib * MIB)
    run = ET.parse(toc).getroot().find("./run")
    target = None if run is None else run.find("info/target/process")
    if target is None or target.get("pid") != str(args.pid):
        raise RuntimeError("export target PID mismatch")
    tables = {table.get("schema") for table in run.findall("data/table")}
    required = (*SCHEMAS, *(("thread-state",) if args.thread_states else ()))
    if not set(required).issubset(tables):
        raise RuntimeError(f"missing required Overview tables: {set(required) - tables}")
    result = {"status": "failed", "exports": exports, "ranges": ranges,
              "summary": {e.tag: e.text for e in run.findall("info/summary/*") if not list(e)},
              "expected_seconds": expected_seconds, "required_span_seconds": max(1, expected_seconds - 5),
              "maximum_allowed_start_gap_seconds": 5}
    try:
        for schema in required:
            output = args.output / (schema + ".xml")
            selector = ["--xpath", '/trace-toc/run[@number="1"]/data/table[@schema="' + schema + '"]']
            exports[schema] = bounded_export(trace, output, selector, args.export_timeout, args.export_cap_mib * MIB)
            ranges[schema] = table_coverage(output, args.pid)
            if not ranges[schema]["target_rows"] or (ranges[schema]["span_seconds"] or 0) < result["required_span_seconds"]:
                raise RuntimeError(f"insufficient actual target coverage: {schema}")
            if ranges[schema]["maximum_consecutive_start_gap_seconds"] > 5:
                raise RuntimeError(f"actual target timestamp gap exceeds five seconds: {schema}")
            if schema == "metal-perf-overview-layer-duration-metric":
                for family in GPU_FAMILIES:
                    item = ranges[schema]["gpu_families"].get(family)
                    if (item is None or item["span_seconds"] < result["required_span_seconds"] or
                            item["maximum_consecutive_start_gap_seconds"] > 5):
                        raise RuntimeError(f"insufficient actual positive-frame target GPU family coverage: {family}")
        clock_ranges = [("time-profile", ranges["time-profile"])] if "time-profile" in ranges else []
        gpu = ranges.get("metal-perf-overview-layer-duration-metric", {}).get("gpu_families", {})
        clock_ranges.extend((family, gpu[family]) for family in GPU_FAMILIES if family in gpu)
        if clock_ranges:
            origin = utc_unix_ns(result["summary"]["start-date"])
            first = min(item["first_ns"] for _, item in clock_ranges)
            last = max(item["last_end_ns"] for _, item in clock_ranges)
            result["target_clock_bounds"] = {"trace_origin_unix_ns": origin, "first_offset_ns": first,
                "last_end_offset_ns": last, "first_unix_ns": origin + first,
                "last_end_unix_ns": origin + last, "sources": [name for name, _ in clock_ranges]}
        result["status"] = "verified_target_timestamp_ranges"
    except BaseException as error:
        result["failure"] = str(error)
        raise
    finally:
        write_json(args.output / "coverage.json", result)
    return result


class Collector:
    def __init__(self, args):
        self.args = args
        args.output.mkdir(mode=0o700, parents=False, exist_ok=False)
        self.state = {"schema": 1, "status": "preflight", "start": stamp(), "viewer_launched": False,
                      "settings_changed": False, "source_sha": args.source_sha,
                      "limits": vars(args).copy(), "recorder_exit": None, "raw_preserved": True,
                      "readiness": None, "max_observed_raw_bytes": 0}
        self.state["limits"] = {key: str(value) if isinstance(value, Path) else value
                                for key, value in self.state["limits"].items()}
        self.recorder = self.power = self.notification = None
        self.tails = []

    def save(self):
        write_json(self.args.output / "session.json", self.state)

    def finalize_failed_record(self, deadline):
        if self.recorder.poll() is not None:
            return
        self.recorder.send_signal(signal.SIGINT)
        self.event("failed_record_graceful_stop_requested")
        next_sample = 0
        while self.recorder.poll() is None and time.monotonic() < deadline:
            if time.monotonic() >= next_sample:
                size, _ = raw_size(self.args.output / "overview.trace", self.recorder.pid)
                self.state["max_observed_raw_bytes"] = max(size, self.state["max_observed_raw_bytes"])
                self.event("failure_finalization_raw_sample", bytes=size)
                next_sample = time.monotonic() + 1
            time.sleep(.1)

    def event(self, event_type, **fields):
        row = {"event": event_type, **stamp(), **fields}
        append_json(self.args.output / "events.jsonl", row)
        print(json.dumps(row), flush=True)

    def run(self):
        args, handles = self.args, []
        begin = time.monotonic()
        ready_at = stopping_at = finalizing_at = None
        reason = None
        last_sample = -1
        try:
            self.save()
            flight = self.state["preflight"] = preflight(args)
            identity = self.state["target"] = process_identity(args.pid, args.executable, args.exe_sha256)
            for source, name, cap in ((args.log, "viewer.log", 256 * MIB),):
                if source:
                    self.tails.append(Tail(source, args.output / name, cap))
            self.notification = StartNotification()  # Must precede recorder spawn.
            command = self.state["recorder_argv"] = record_argv(args, flight, self.notification.name)
            record_log = (args.output / "record.log").open("xb")
            handles.append(record_log)
            self.recorder = subprocess.Popen(command, stdout=record_log, stderr=subprocess.STDOUT,
                                             start_new_session=True)
            self.state.update(status="starting", recorder_pid=self.recorder.pid,
                              notification_name=self.notification.name)
            self.event("record_start_requested", recorder_pid=self.recorder.pid)
            if args.powermetrics:
                power_log = (args.output / "powermetrics.plist-stream").open("xb")
                handles.append(power_log)
                power_command = ["/usr/bin/powermetrics", "--samplers", "cpu_power,gpu_power,thermal",
                                 "--format", "plist", "--sample-rate", "1000", "--sample-count",
                                 str(math.ceil(args.seconds + args.start_timeout + args.finalize_timeout))]
                self.state["powermetrics_argv"] = power_command
                self.power = subprocess.Popen(power_command, stdout=power_log, stderr=subprocess.STDOUT,
                                              start_new_session=True)
            notified = False
            while True:
                now = time.monotonic()
                for tail in self.tails:
                    tail.poll()
                notified = notified or self.notification.received()
                code = self.recorder.poll()
                if now - last_sample >= 1 or code is not None:
                    size, children = raw_size(args.output / "overview.trace", self.recorder.pid)
                    self.state.update(final_observed_raw_bytes=size, owned_recorder_processes=children,
                                      max_observed_raw_bytes=max(size, self.state["max_observed_raw_bytes"]))
                    self.event("raw_size_sample", bytes=size, recorder_alive=code is None)
                    last_sample = now
                    if self.state["max_observed_raw_bytes"] > args.raw_cap_mib * MIB:
                        raise RuntimeError("observed raw size limit; all evidence preserved")
                    current = process_identity(args.pid)
                    if any(current[key] != identity[key] for key in ("executable", "process_start")):
                        raise RuntimeError("target PID identity changed")
                    if record_log.tell() > 16 * MIB:
                        raise RuntimeError("recorder log size limit")
                    if self.power and (self.power.poll() is not None or power_log.tell() > 32 * MIB):
                        raise RuntimeError("requested powermetrics exited early or exceeded size limit")
                    if args.diagnostic_jsonl and args.diagnostic_jsonl.exists():
                        native_size = args.diagnostic_jsonl.stat().st_size
                        self.state["diagnostic_observed_bytes"] = native_size
                        if native_size > 64 * MIB:
                            raise RuntimeError("native diagnostic input exceeds its declared size cap")
                    self.save()
                if code is not None:
                    self.state["recorder_exit"] = code
                    if code:
                        raise RuntimeError(f"recorder failed, exit {code}")
                    if ready_at is None:
                        raise RuntimeError("recorder exited without confirmed live tracing readiness")
                    break
                if ready_at is None and notified and self.state["final_observed_raw_bytes"] > 0:
                    ready_at = now
                    self.state.update(status="recording", readiness=stamp(),
                                      readiness_proof="Darwin tracing-start notification + live recorder + nonempty owned raw/trace files")
                    self.event("owner_cue_ready", notification=self.notification.name)
                if ready_at is None and now - begin > args.start_timeout:
                    raise RuntimeError("tracing-start/live-data readiness deadline")
                # Text is supplemental finalization evidence, never a readiness test.
                log_text = (args.output / "record.log").read_text(errors="replace")
                if finalizing_at is None and ("Stopping recording" in log_text or "Saving output file" in log_text):
                    finalizing_at = now
                    self.state["finalization_observed"] = stamp()
                stop = args.output / "stop-request.json"
                if stopping_at is None and (stop.exists() or (ready_at is not None and now - ready_at > args.seconds + 5)):
                    stopping_at = now
                    self.recorder.send_signal(signal.SIGINT)
                    self.state["stop_requested"] = stamp()
                    self.event("record_stop_requested", operator_request=stop.exists())
                if finalizing_at is not None or stopping_at is not None:
                    final_start = min(value for value in (finalizing_at, stopping_at) if value is not None)
                    if now - final_start > args.finalize_timeout:
                        raise RuntimeError("recorder finalization deadline")
                if now - begin > args.start_timeout + args.seconds + args.finalize_timeout + 10:
                    raise RuntimeError("recorder total wall deadline")
                time.sleep(.1)
            self.state["recording_finished"] = stamp()
            self.event("recording_saved_qualification_pending", recorder_exit=self.recorder.returncode)
            # No optional sampler should keep running during potentially slow
            # trace imports/exports. Preserve its real exit, including failure.
            if self.power is not None:
                self.state["powermetrics_exit"] = terminate_owned(self.power)
            # Recorder shutdown/export wall time is not recording duration. An
            # operator stop can shorten the request; normal finalization cannot
            # lengthen it. Actual TOC/table ranges remain independent evidence.
            expected = min(args.seconds, stopping_at - ready_at) if stopping_at is not None else args.seconds
            self.state["coverage"] = qualify(args, max(1, expected))
            self.state["status"] = "complete"
        except BaseException as error:
            reason = f"{type(error).__name__}: {error}"
            self.state.update(status="failed", failure=reason)
        finally:
            if self.recorder is not None:
                try:
                    # Give xctrace a finite chance to save its output on a
                    # failure/interrupt. The receipt remains failed even if its
                    # eventual recorder exit is zero. Escalate only if needed.
                    previous_stop = [value for value in (stopping_at, finalizing_at) if value is not None]
                    deadline = (min(previous_stop) if previous_stop else time.monotonic()) + args.finalize_timeout
                    self.finalize_failed_record(deadline)
                except Exception as error:
                    reason = reason or f"recorder cleanup failed: {error}"
                    self.state.update(status="failed", failure=reason, recorder_cleanup_failure=str(error))
                try:
                    # Even a broken size observer must not strand the recorder.
                    self.state["recorder_exit"] = terminate_owned(self.recorder)
                except Exception as error:
                    reason = reason or f"recorder termination failed: {error}"
                    self.state.update(status="failed", failure=reason, recorder_cleanup_failure=str(error))
                final_size = tree_size([args.output / "overview.trace"])
                self.state["final_trace_bytes"] = final_size
                self.state["max_observed_raw_bytes"] = max(self.state["max_observed_raw_bytes"], final_size)
                if final_size > args.raw_cap_mib * MIB:
                    reason = reason or "final output size limit"
                    self.state.update(status="failed", failure=reason)
            if self.power is not None:
                try:
                    self.state["powermetrics_exit"] = terminate_owned(self.power)
                except Exception as error:
                    reason = reason or f"powermetrics cleanup failed: {error}"
                    self.state.update(status="failed", failure=reason)
            if self.notification is not None:
                try:
                    self.notification.close()
                except Exception as error:
                    reason = reason or f"notification cleanup failed: {error}"
                    self.state.update(status="failed", failure=reason)
            for tail in self.tails:
                try:
                    tail.poll()
                except Exception as error:
                    reason = reason or str(error)
                    self.state.update(status="failed", failure=reason)
            for handle in handles:
                handle.close()
            self.state.update(end=stamp(), elapsed_seconds=time.monotonic() - begin,
                              inputs=[tail.receipt() for tail in self.tails])
            if args.diagnostic_jsonl:
                try:
                    coverage = diagnostic_coverage(args.diagnostic_jsonl)
                    self.state["diagnostic_coverage"] = coverage
                    native = qualify_native(coverage, self.state.get("coverage", {}), args.source_sha, args.pid)
                    self.state["native_coverage"] = native
                    if not native["covered"]:
                        raise RuntimeError("requested native diagnostic stream incomplete: " + native["native_coverage_status"])
                except Exception as error:
                    reason = reason or str(error)
                    self.state.update(status="failed", failure=reason)
            self.save()
            self.event("collector_finished", status=self.state["status"], recorder_exit=self.state["recorder_exit"])
        return 1 if reason else 0


def parser():
    root = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = root.add_subparsers(dest="command", required=True)
    metadata = commands.add_parser("preflight", help="installed-tool/template inspection only; no recording")
    record = commands.add_parser("record", help="explicit profiling of an already running exact PID")
    for child in (metadata, record):
        child.add_argument("--template", type=Path)
        child.add_argument("--powermetrics", action="store_true", help="require root and all requested samplers; never sudo")
        child.add_argument("--thread-states", action="store_true",
                           help="optional Thread State Trace only; adds scheduler data/cost, not detailed Metal Application")
    record.add_argument("--pid", type=int, required=True)
    record.add_argument("--executable", type=Path, required=True)
    record.add_argument("--exe-sha256", required=True)
    record.add_argument("--source-sha", required=True)
    record.add_argument("--output", type=Path, required=True, help="new absolute private run directory; parent must exist")
    record.add_argument("--seconds", type=int, default=360)
    record.add_argument("--start-timeout", type=int, default=45)
    record.add_argument("--finalize-timeout", type=int, default=90)
    record.add_argument("--raw-cap-mib", type=int, default=2048)
    record.add_argument("--export-timeout", type=int, default=90)
    record.add_argument("--export-cap-mib", type=int, default=512)
    record.add_argument("--log", type=Path)
    record.add_argument("--diagnostic-jsonl", type=Path)
    mark = commands.add_parser("mark", help="record an operator observation; phase stop requests graceful recorder stop")
    mark.add_argument("--output", type=Path, required=True)
    mark.add_argument("--phase", required=True)
    mark.add_argument("--note", default="", help="private observation, never interpreted as a scene/location fact")
    return root


def main(argv=None):
    os.umask(0o077)
    command_parser = parser()
    args = command_parser.parse_args(argv)
    try:
        if args.command == "preflight":
            print(json.dumps(preflight(args), indent=2))
            return 0
        if args.command == "mark":
            if not (args.output / "session.json").is_file():
                raise RuntimeError("existing collector session required")
            row = {"event": "operator_marker", **stamp(), "phase": args.phase, "note": args.note}
            append_json(args.output / "markers.jsonl", row)
            if args.phase == "stop":
                with (args.output / "stop-request.json").open("x") as stream:
                    json.dump(row, stream)
            return 0
        if not re.fullmatch(r"[0-9a-fA-F]{64}", args.exe_sha256):
            raise RuntimeError("--exe-sha256 must be the actual 64-digit executable SHA256")
        if not re.fullmatch(r"[0-9a-fA-F]{40}", args.source_sha):
            raise RuntimeError("--source-sha must be the full 40-digit source commit")
        for key in ("output", "executable", "log", "diagnostic_jsonl"):
            value = getattr(args, key)
            if value is not None and not value.is_absolute():
                raise RuntimeError(f"--{key.replace('_', '-')} must be absolute")
        if not 1 <= args.seconds <= 600:
            raise RuntimeError("--seconds must be between 1 and 600")
        if args.thread_states and args.seconds > 5:
            raise RuntimeError("--thread-states is limited to <=5 seconds; measured scheduler raw-data cost forbids continuous use")
        for key in ("start_timeout", "finalize_timeout", "raw_cap_mib", "export_timeout", "export_cap_mib"):
            if not 1 <= getattr(args, key) <= (4096 if key.endswith("mib") else 180):
                raise RuntimeError(f"invalid finite limit: {key}")
        return Collector(args).run()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"quiet_capture: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
