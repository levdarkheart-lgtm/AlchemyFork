#!/usr/bin/env python3
"""Summarize bounded native quiet diagnostics without changing their source.

This is offline evidence reduction, not a profiler or an automatic diagnosis.
GPU command envelopes may overlap; their union is not a hardware-busy counter.
"""
from __future__ import annotations

import argparse
from array import array
from bisect import bisect_left
from collections import Counter, OrderedDict, defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile

MAX_BYTES = 64 * 1024 * 1024
MAX_INPUT_BYTES = 256 * 1024 * 1024
MAX_LINE = 16 * 1024
MAX_COMMANDS = 8192
MAX_RECENT = 1024
MAX_FRAMES = 100000
MAX_SEQUENCES = 4000000  # Finite bitset covers the explicit 256MiB input ceiling.
MAX_UNION_INTERVALS = 100000
MAX_PERIOD_SAMPLES = 50000
MAX_GROUPS = 512
MAX_MARKERS = 1024
DEFAULT_PHASES = (("early", 0.0, 10.0), ("decline", 10.0, 60.0), ("late", 60.0, 120.0))
VIEW_NAMES = {0: "unknown", 1: "main", 2: "hero", 3: "ordinary_probe", 4: "shadow", 5: "upload"}
FRAME_FIELDS = ("iteration_id", "start_mach_ns", "end_mach_ns", "begin_display_frame",
                "end_display_frame", "begin_flags", "end_flags", "begin_startup_state",
                "end_startup_state", "begin_teleport_state", "end_teleport_state", "outcome",
                "native_exact", "drawable_width", "drawable_height", "quality")
FRAME_WIDTH = len(FRAME_FIELDS)
FRAME_INDEX = {name: i for i, name in enumerate(FRAME_FIELDS)}
METRIC_FIELDS = {
    "source_cache": ("entries", "unique_resources", "estimated_bytes", "max_bytes", "byte_rejections",
                     "entry_and_byte_rejections", "capacity_evictions", "new_retains"),
    "vertex_uploads": ("full_uploads", "partial_uploads", "uploaded_bytes", "recycled_generations",
                       "leased_orphans", "unused5", "unused6", "unused7"),
    "texture_publication": ("cow_attempts", "cow_replacements", "cow_encoded_bytes", "worker_uploads",
                            "worker_batch_commits", "worker_batch_failures", "immediate_upload_commits",
                            "immediate_upload_failures"),
    "scene_resources": ("width", "height", "tracked_texture_bytes", "tracked_device_bytes", "objects",
                        "device_current_allocated_bytes", "committed_commands", "thermal_state"),
    "local_light_workload": ("points", "bounded_points", "inside_points", "bounded_rect_area_sum",
                             "width", "height", "with_depth", "unused7"),
    "projector_workload": ("projectors", "shadowed_projectors", "default_rect_projectors", "rect_area_sum",
                           "width", "height", "with_depth", "needs_shadows"),
    "base_lighting_features": ("width", "height", "configured_probe_count", "hero_ready", "ssr_ready",
                               "ssr_trace_iterations", "ssr_samples", "tile_light_map"),
}
CAPTURE_END_MARKERS = {"quiet_capture_stopped", "quiet_capture_world_invalidated",
                       "quiet_capture_cadence_invalid", "quiet_capture_interval_timeout",
                       "quiet_capture_advance_failed", "quiet_capture_start_failed"}


def integer(value, default=None):
    if isinstance(value, bool):
        return int(value)
    return value if isinstance(value, int) and 0 <= value <= (1 << 64) - 1 else default


def tag(row, prefix=""):
    return (integer(row.get(prefix + "frame_id"), 0),
            integer(row.get(prefix + "view_kind"), 0), row.get(prefix + "view_index", -1))


def utc_from_mach(ns, header):
    try:
        before = integer(header["calibration_before_mach_ns"])
        after = integer(header["calibration_after_mach_ns"])
        unix = integer(header["calibration_unix_ns"])
        if None in (before, after, unix) or after < before:
            return None
        estimate = unix + ns - (before + after) // 2
        return datetime.fromtimestamp(estimate / 1e9, timezone.utc).isoformat()
    except (KeyError, ValueError, OverflowError, OSError):
        return None


class Stream:
    """Two bounded streaming passes, including exact raw-byte prefix hashes."""
    def __init__(self, path, maximum):
        self.path, self.maximum = Path(path), maximum
        self.bytes = 0
        self.digest = hashlib.sha256()
        self.errors = Counter()
        self.error_examples = []
        self.lines = 0
        self.unterminated_tail = False

    def problem(self, kind, line):
        self.errors[kind] += 1
        if len(self.error_examples) < 8:
            self.error_examples.append({"kind": kind, "line": line})

    def __iter__(self):
        with self.path.open("rb") as handle:
            skipping = False
            while self.bytes < self.maximum:
                raw = handle.readline(min(MAX_LINE + 1, self.maximum - self.bytes))
                if not raw:
                    break
                self.bytes += len(raw)
                self.digest.update(raw)
                ending = raw.endswith(b"\n")
                if skipping:
                    if ending:
                        skipping = False
                    continue
                self.lines += 1
                if len(raw) > MAX_LINE:
                    self.problem("oversized_line", self.lines)
                    skipping = not ending
                    continue
                if not ending:
                    self.unterminated_tail = True
                if not raw.strip():
                    self.problem("blank_line", self.lines)
                    continue
                try:
                    value = json.loads(raw)
                except (ValueError, UnicodeError, RecursionError):
                    self.problem("malformed_json", self.lines)
                    continue
                if not isinstance(value, dict):
                    self.problem("non_object_json", self.lines)
                    continue
                yield value, self.lines


@dataclass
class Moments:
    count: int = 0
    total: int = 0
    minimum: int | None = None
    maximum: int | None = None

    def add(self, value):
        self.count += 1
        self.total += value
        self.minimum = value if self.minimum is None else min(self.minimum, value)
        self.maximum = value if self.maximum is None else max(self.maximum, value)

    def result(self):
        return {"count": self.count, "sum": self.total, "min": self.minimum, "max": self.maximum,
                "mean": self.total / self.count if self.count else None}


class Budget:
    def __init__(self):
        self.intervals = 0
        self.period_samples = 0
        self.limits = Counter()


class IntervalUnion:
    def __init__(self, budget):
        self.rows = []
        self.budget = budget
        self.exact = True

    def add(self, start, end):
        if end <= start:
            return
        rows = self.rows
        at = bisect_left(rows, (start,))
        if at and rows[at - 1][1] >= start:
            at -= 1
        stop = at
        while stop < len(rows) and rows[stop][0] <= end:
            start, end = min(start, rows[stop][0]), max(end, rows[stop][1])
            stop += 1
        change = 1 - (stop - at)
        if change > 0 and self.budget.intervals + change > MAX_UNION_INTERVALS:
            self.exact = False
            self.budget.limits["union_interval_capacity"] += 1
            return
        rows[at:stop] = [(start, end)]
        self.budget.intervals += change

    def result(self):
        return {"union_wall_ns": sum(b - a for a, b in self.rows), "disjoint_intervals": len(self.rows),
                "exact": self.exact}


class Periods:
    def __init__(self, budget):
        self.moments = Moments()
        self.samples = []
        self.budget = budget

    def add(self, duration):
        self.moments.add(duration)
        if self.budget.period_samples < MAX_PERIOD_SAMPLES:
            self.samples.append(duration)
            self.budget.period_samples += 1
        else:
            self.budget.limits["period_quantile_capacity"] += 1

    def result(self):
        out = self.moments.result()
        out["cadence_hz"] = self.moments.count * 1e9 / self.moments.total if self.moments.total else None
        out["quantiles_exact"] = len(self.samples) == self.moments.count
        out["quantile_sample_count"] = len(self.samples)
        ordered = sorted(self.samples)
        for label, fraction in (("p50_ns", .5), ("p95_ns", .95), ("p99_ns", .99)):
            if not ordered or not out["quantiles_exact"]:
                out[label] = None
            else:
                offset = (len(ordered) - 1) * fraction
                lo, hi = int(offset), min(int(offset) + 1, len(ordered) - 1)
                out[label] = ordered[lo] + (ordered[hi] - ordered[lo]) * (offset - lo)
        return out


def intersections(start, end, intervals):
    return any(start < b and end > a for a, b in intervals)


def subtract(start, end, removed):
    pieces = [(start, end)]
    for a, b in removed:
        next_pieces = []
        for lo, hi in pieces:
            if a >= hi or b <= lo:
                next_pieces.append((lo, hi))
            else:
                if lo < a:
                    next_pieces.append((lo, min(a, hi)))
                if hi > b:
                    next_pieces.append((max(b, lo), hi))
        pieces = next_pieces
    return pieces


def frame_reasons(frame, started_state):
    reasons = []
    f = lambda key: frame[FRAME_INDEX[key]]
    if f("outcome") != 0:
        reasons.append("iteration_not_completed")
    if not f("native_exact"):
        reasons.append("native_exact_false")
    if f("end_display_frame") == f("begin_display_frame"):
        reasons.append("no_display_increment")
    elif f("end_display_frame") != f("begin_display_frame") + 1:
        reasons.append("display_increment_not_one")
    if f("begin_startup_state") < started_state or f("end_startup_state") < started_state:
        reasons.append("startup")
    if f("begin_teleport_state") or f("end_teleport_state"):
        reasons.append("teleport")
    for flags in (f("begin_flags"), f("end_flags")):
        if not flags & 1:
            reasons.append("state_reader_missing")
        if not flags & (1 << 1):
            reasons.append("unfocused")
        if flags & (1 << 2):
            reasons.append("teleport_display")
        if flags & ((1 << 3) | (1 << 4) | (1 << 5)):
            reasons.append("disconnect_logout_or_exit")
        if flags & (1 << 6):
            reasons.append("noninteractive")
        if not flags & (1 << 7) or not flags & (1 << 8) or flags & (1 << 9):
            reasons.append("hidden_minimized_or_missing_window")
    return sorted(set(reasons))


def metadata_captures(directory):
    result, receipts = [], []
    if directory is None:
        return result, receipts
    for child in ("quiet-early-frame-capture", "quiet-late-frame-capture"):
        path = Path(directory) / child / "metadata.jsonl"
        if not path.exists():
            receipts.append({"relative_path": child + "/metadata.jsonl", "missing": True})
            continue
        stream = Stream(path, 16 * 1024)
        last = None
        for row, _ in stream:
            if row.get("quiet_cpu_clock") == "mach_absolute_time_ns":
                last = row
        receipts.append({"relative_path": child + "/metadata.jsonl", "bytes": stream.bytes,
                         "sha256": stream.digest.hexdigest(), "errors": dict(stream.errors)})
        if not last:
            continue
        start = integer(last.get("quiet_start_request_monotonic_ns"), 0)
        end = integer(last.get("quiet_stop_return_monotonic_ns"), 0)
        if not end and last.get("status") == "start_failed":
            end = integer(last.get("quiet_start_return_monotonic_ns"), 0)
        if start:
            result.append({"start_ns": start, "end_ns": end or None, "source": "helper_cpu_api_bounds",
                           "status": last.get("status"), "start_api_uncertain": False,
                           "end_missing": not bool(end), "trigger": last.get("quiet_trigger")})
    return result, receipts


def marker_captures(markers):
    result, current, pending = [], None, None
    for row in sorted(markers, key=lambda r: r["mach_ns"]):
        name, now = row.get("name"), row["mach_ns"]
        if name == "quiet_capture_trigger":
            pending = now
        elif name == "quiet_capture_started":
            if current:
                result.append(current)
            current = {"start_ns": pending if pending is not None else now, "end_ns": None,
                       "source": "marker_trigger_or_start", "start_api_uncertain": pending is None,
                       "end_missing": True}
            pending = None
        elif name in CAPTURE_END_MARKERS:
            if current is None and pending is not None:
                current = {"start_ns": pending, "end_ns": None, "source": "marker_trigger_or_start",
                           "start_api_uncertain": False, "end_missing": True}
            if current is not None:
                current.update(end_ns=now, end_missing=False, status=name)
                result.append(current)
            current, pending = None, None
    if current:
        result.append(current)
    elif pending is not None:
        result.append({"start_ns": pending, "end_ns": None, "source": "marker_trigger_or_start",
                       "start_api_uncertain": False, "end_missing": True, "status": "trigger_without_result"})
    return result


@dataclass
class Phase:
    name: str
    start: int
    end: int
    requested_end: int
    budget: Budget
    events: Counter = field(default_factory=Counter)
    frame_rejections: Counter = field(default_factory=Counter)
    command_counts: Counter = field(default_factory=Counter)
    gpu_duration: Moments = field(default_factory=Moments)
    gpu_commands: int = 0
    gpu_unknown_owner: int = 0
    wait_counts: Counter = field(default_factory=Counter)
    waits: Moments = field(default_factory=Moments)
    first_event: int | None = None
    last_event: int | None = None

    def __post_init__(self):
        self.raw_periods = Periods(self.budget)
        self.eligible_periods = Periods(self.budget)
        self.control_periods = Periods(self.budget)
        self.gpu_union = IntervalUnion(self.budget)
        self.gpu_control_union = IntervalUnion(self.budget)
        self.wait_union = IntervalUnion(self.budget)
        self.wait_control_union = IntervalUnion(self.budget)
        self.gpu_by_owner = {}
        self.labels = {}
        self.cpu_spans = {}
        self.metrics = {}
        self.progression = {}
        self.command_latencies = {}
        self.configurations = Counter()

    def overlap(self, start, end):
        return max(self.start, start), min(self.end, end)

    def observe(self, timestamp, event):
        if self.start <= timestamp < self.end:
            self.events[event] += 1
            self.first_event = timestamp if self.first_event is None else min(self.first_event, timestamp)
            self.last_event = timestamp if self.last_event is None else max(self.last_event, timestamp)


@dataclass(slots=True)
class Command:
    identifier: int
    label: str = "unmatched_label"
    owner: tuple = (0, 0, -1)
    times: dict = field(default_factory=dict)
    calculated: set = field(default_factory=set)
    terminal: str | None = None
    created: bool | None = None


class Analyzer:
    def __init__(self, path, *, maximum=MAX_BYTES, phases=DEFAULT_PHASES,
                 world_start=None, capture_directory=None, started_state=24, bin_seconds=10):
        if not 0 < maximum <= MAX_INPUT_BYTES:
            raise ValueError("maximum input bytes must be 1..268435456")
        self.path = Path(path)
        self.maximum = maximum
        self.phase_specs = phases
        self.override_world = world_start
        self.capture_directory = capture_directory
        self.started_state = started_state
        self.bin_ns = int(bin_seconds * 1e9)
        self.budget = Budget()
        self.problems = Counter()
        self.frames = array("Q")
        self.header = None
        self.footer = None
        self.markers = []
        self.event_counts = Counter()
        self.first_ns = self.last_ns = None
        self.sequence_bits = bytearray((MAX_SEQUENCES + 7) // 8)
        self.sequence_unique = self.sequence_duplicates = self.sequence_out_of_order = 0
        self.sequence_min = self.sequence_max = 0
        self.active = OrderedDict()
        self.recent = OrderedDict()
        self.command_totals = Counter()
        self.command_missing = Counter()
        self.command_examples = []
        self.open_waits = {}
        self.wait_totals = Counter()

    def first_pass(self):
        stream = Stream(self.path, self.maximum)
        for row, line in stream:
            event = row.get("event")
            self.event_counts[str(event)] += 1
            if event == "header":
                if self.header is not None:
                    self.problems["multiple_headers"] += 1
                else:
                    self.header = row
                continue
            if event == "footer":
                if self.footer is not None:
                    self.problems["multiple_footers"] += 1
                self.footer = row
                continue
            now = integer(row.get("mach_ns"))
            if now is None:
                self.problems["invalid_event_timestamp"] += 1
                continue
            self.first_ns = now if self.first_ns is None else min(self.first_ns, now)
            self.last_ns = now if self.last_ns is None else max(self.last_ns, now)
            sequence = integer(row.get("sequence"), 0)
            if not 1 <= sequence <= MAX_SEQUENCES:
                self.problems["sequence_outside_tracking_bound"] += 1
            else:
                bit, mask = (sequence - 1) // 8, 1 << ((sequence - 1) % 8)
                if self.sequence_bits[bit] & mask:
                    self.sequence_duplicates += 1
                else:
                    self.sequence_bits[bit] |= mask
                    self.sequence_unique += 1
                if self.sequence_max and sequence < self.sequence_max:
                    self.sequence_out_of_order += 1
                self.sequence_min = min(self.sequence_min, sequence) if self.sequence_min else sequence
                self.sequence_max = max(self.sequence_max, sequence)
            if self.header and row.get("session_id") != self.header.get("session_id"):
                self.problems["session_mismatch"] += 1
            if event == "frame":
                values = [integer(row.get(name)) for name in FRAME_FIELDS]
                if any(value is None for value in values) or values[2] < values[1]:
                    self.problems["invalid_frame_record"] += 1
                elif len(self.frames) // FRAME_WIDTH >= MAX_FRAMES:
                    self.budget.limits["frame_capacity"] += 1
                else:
                    self.frames.extend(values)
            if event == "marker" or (event == "metrics" and row.get("name") == "quiet_capture_trigger"):
                if len(self.markers) < MAX_MARKERS:
                    self.markers.append({"name": row.get("name"), "mach_ns": now,
                                         "frame_id": integer(row.get("frame_id"), 0),
                                         "reason": integer(row.get("reason"), 0)})
                else:
                    self.budget.limits["marker_capacity"] += 1
        self.stream = stream

    def iter_frames(self):
        for at in range(0, len(self.frames), FRAME_WIDTH):
            yield self.frames[at:at + FRAME_WIDTH]

    def prepare_windows(self):
        starts = sorted(row["mach_ns"] for row in self.markers if row["name"] == "quiet_world_started")
        self.world_start = self.override_world if self.override_world is not None else (starts[0] if starts else None)
        endings = sorted(row["mach_ns"] for row in self.markers
                         if row["name"] in ("quiet_world_invalidated", "quiet_sequence_world_invalidated")
                         and self.world_start is not None and row["mach_ns"] >= self.world_start)
        self.world_end = endings[0] if endings else None
        helper, self.capture_receipts = metadata_captures(self.capture_directory)
        marker = marker_captures(self.markers)
        self.captures = helper + [row for row in marker if not any(
            row["start_ns"] < (item["end_ns"] or (1 << 64) - 1)
            and (row["end_ns"] or (1 << 64) - 1) > item["start_ns"] for item in helper)]
        self.capture_bounds = [(row["start_ns"], row["end_ns"] or (1 << 64) - 1) for row in self.captures]
        self.post_capture_periods = []
        pending = [row["end_ns"] for row in self.captures if row.get("end_ns")]
        previous = None
        for frame in self.iter_frames():
            if previous is not None and frame[0] == previous[0] + 1 and frame[1] > previous[1]:
                for ending in list(pending):
                    if previous[1] >= ending:
                        self.post_capture_periods.append((previous[1], frame[1]))
                        pending.remove(ending)
            previous = frame
        self.control_removed = sorted(self.capture_bounds + self.post_capture_periods)
        self.phases = []
        if self.world_start is None:
            return
        for name, lo, hi in self.phase_specs:
            start = self.world_start + round(lo * 1e9)
            requested_end = self.world_start + round(hi * 1e9)
            end = min(requested_end, self.world_end) if self.world_end is not None else requested_end
            self.phases.append(Phase(name, start, max(start, end), requested_end, self.budget))

    def analyze_frames(self):
        self.frame_totals = Counter()
        self.frame_reasons = Counter()
        previous = None
        for frame in self.iter_frames():
            self.frame_totals["frame_rows"] += 1
            reasons = frame_reasons(frame, self.started_state)
            self.frame_reasons.update(reasons)
            if not reasons:
                self.frame_totals["eligible_frame_rows"] += 1
            configuration = tuple(frame[FRAME_INDEX[key]] for key in ("drawable_width", "drawable_height", "quality"))
            for phase in self.phases:
                if phase.start <= frame[1] < phase.end:
                    if configuration in phase.configurations or len(phase.configurations) < MAX_GROUPS:
                        phase.configurations[configuration] += 1
                    else:
                        self.budget.limits["frame_configuration_groups"] += 1
            if previous is not None:
                if frame[0] != previous[0] + 1:
                    self.frame_totals["iteration_id_gaps_or_reordering"] += 1
                elif frame[1] <= previous[1]:
                    self.frame_totals["nonpositive_start_period"] += 1
                else:
                    start, end = previous[1], frame[1]
                    self.frame_totals["consecutive_raw_periods"] += 1
                    period_reasons = sorted(set(reasons + frame_reasons(previous, self.started_state)))
                    if previous[4] != frame[3]:
                        period_reasons.append("display_boundary_discontinuity")
                    captured = intersections(start, end, self.capture_bounds)
                    post = intersections(start, end, self.post_capture_periods)
                    configuration_changed = any(previous[FRAME_INDEX[key]] != frame[FRAME_INDEX[key]]
                                                for key in ("drawable_width", "drawable_height", "quality"))
                    for phase in self.phases:
                        if not (start < phase.end and end > phase.start):
                            continue
                        if start < phase.start or end > phase.end:
                            phase.frame_rejections["crosses_phase_boundary"] += 1
                            continue
                        phase.raw_periods.add(end - start)
                        phase.frame_rejections.update(period_reasons)
                        if captured:
                            phase.frame_rejections["capture_or_capture_api_overlap"] += 1
                        if post:
                            phase.frame_rejections["first_post_capture_period"] += 1
                        if configuration_changed:
                            phase.frame_rejections["quality_or_resolution_change"] += 1
                        if not period_reasons:
                            phase.eligible_periods.add(end - start)
                            if not captured and not post and not configuration_changed:
                                phase.control_periods.add(end - start)
            previous = frame

    def phase_duration(self, name, start, end):
        if end < start:
            self.problems["negative_command_latency"] += 1
            return
        for phase in self.phases:
            if phase.start <= start < phase.end:
                phase.command_latencies.setdefault(name, Moments()).add(end - start)

    def finish_command(self, command, limited=False):
        self.command_totals["identities_seen"] += 1
        if limited:
            self.command_totals["evicted_pending_state"] += 1
        elif command.terminal:
            self.command_totals[command.terminal] += 1
            required = {"command_create_begin", "command_create_end"}
            if command.terminal == "completed":
                required |= {"command_commit_begin", "command_commit_end", "command_complete"}
            missing = sorted(required - command.times.keys())
            self.command_missing.update(missing)
            if missing:
                self.command_totals["terminal_with_missing_events"] += 1
        else:
            self.command_totals["pending_at_observed_cutoff"] += 1
        if (limited or not command.terminal or command.label == "unmatched_label") and len(self.command_examples) < 12:
            self.command_examples.append({"command_id": command.identifier, "label": command.label,
                                          "frame_id": command.owner[0], "view_kind": command.owner[1],
                                          "observed_events": sorted(command.times), "terminal": command.terminal})

    def command(self, identifier):
        if identifier in self.active:
            self.active.move_to_end(identifier)
            return self.active[identifier]
        if identifier in self.recent:
            self.recent.move_to_end(identifier)
            return self.recent[identifier]
        if len(self.active) >= MAX_COMMANDS:
            _, old = self.active.popitem(last=False)
            self.finish_command(old, limited=True)
            self.budget.limits["active_command_capacity"] += 1
        command = Command(identifier)
        self.active[identifier] = command
        return command

    def terminal(self, command):
        if command.identifier in self.active:
            self.active.pop(command.identifier)
            self.recent[command.identifier] = command
        while len(self.recent) > MAX_RECENT:
            _, old = self.recent.popitem(last=False)
            self.finish_command(old)

    def gpu(self, row, command):
        start, end = row.get("gpu_start_seconds"), row.get("gpu_end_seconds")
        valid = row.get("gpu_time_valid") is True and row.get("native_status") == 4
        valid = valid and isinstance(start, (int, float)) and isinstance(end, (int, float))
        valid = valid and math.isfinite(start) and math.isfinite(end) and 0 < start < end < (1 << 64) / 1e9
        if not valid:
            self.command_totals["completed_without_valid_gpu_interval"] += 1
            return
        self.command_totals["valid_gpu_intervals"] += 1
        start, end = round(start * 1e9), round(end * 1e9)
        for phase in self.phases:
            lo, hi = phase.overlap(start, end)
            if hi <= lo:
                continue
            phase.gpu_commands += 1
            phase.gpu_duration.add(hi - lo)
            phase.gpu_union.add(lo, hi)
            for a, b in subtract(lo, hi, self.control_removed):
                phase.gpu_control_union.add(a, b)
            owner = VIEW_NAMES.get(command.owner[1], "unknown")
            if command.owner[0] == 0 or owner == "unknown":
                phase.gpu_unknown_owner += 1
            if owner not in phase.gpu_by_owner:
                phase.gpu_by_owner[owner] = IntervalUnion(self.budget)
            phase.gpu_by_owner[owner].add(lo, hi)
            label = command.label
            if label not in phase.labels and len(phase.labels) >= MAX_GROUPS:
                label = "other_label_groups"
                self.budget.limits["command_label_groups"] += 1
            phase.labels.setdefault(label, Moments()).add(hi - lo)

    def wait(self, row, command):
        event, now = row["event"], row["mach_ns"]
        caller = tag(row, "caller_")
        key = (row.get("command_id", 0), *caller)
        if event == "command_wait_begin":
            if key in self.open_waits:
                self.open_waits[key] = None
                self.wait_totals["ambiguous_nested_or_concurrent_wait"] += 1
            elif len(self.open_waits) >= MAX_COMMANDS:
                self.budget.limits["wait_state_capacity"] += 1
            else:
                self.open_waits[key] = now
            return
        if key not in self.open_waits:
            self.wait_totals["unmatched_wait_end"] += 1
            return
        start = self.open_waits.pop(key)
        if start is None or now < start:
            self.wait_totals["ambiguous_or_negative_wait"] += 1
            return
        self.wait_totals["paired_waits"] += 1
        if not key[0]:
            self.wait_totals["paired_unknown_command_waits"] += 1
        for phase in self.phases:
            lo, hi = phase.overlap(start, now)
            if hi <= lo:
                continue
            phase.waits.add(hi - lo)
            phase.wait_union.add(lo, hi)
            phase.wait_counts["caller_" + VIEW_NAMES.get(caller[1], "unknown")] += 1
            phase.wait_counts["command_" + (VIEW_NAMES.get(command.owner[1], "unknown") if command else "unknown")] += 1
            for a, b in subtract(lo, hi, self.control_removed):
                phase.wait_control_union.add(a, b)

    def metrics(self, row):
        name, values, now = row.get("name"), row.get("values"), row["mach_ns"]
        if not isinstance(name, str) or len(name) > 96 or not isinstance(values, list) or len(values) != 8 or any(integer(x) is None for x in values):
            self.problems["invalid_metrics_record"] += 1
            return
        if name.startswith("cpu_"):
            start, end = values[:2]
            if end < start:
                self.problems["invalid_cpu_span"] += 1
                return
            self.command_totals["cpu_span_records"] += 1
            for phase in self.phases:
                lo, hi = phase.overlap(start, end)
                if hi <= lo:
                    continue
                if name not in phase.cpu_spans and len(phase.cpu_spans) >= MAX_GROUPS:
                    self.budget.limits["cpu_span_groups"] += 1
                    continue
                data = phase.cpu_spans.setdefault(name, {"wall": Moments(), "union": IntervalUnion(self.budget),
                                                         "control_union": IntervalUnion(self.budget)})
                data["wall"].add(hi - lo)
                data["union"].add(lo, hi)
                for a, b in subtract(lo, hi, self.control_removed):
                    data["control_union"].add(a, b)
            return
        owner = VIEW_NAMES.get(tag(row)[1], "unknown")
        key = name + ":" + owner
        for phase in self.phases:
            if not phase.start <= now < phase.end:
                continue
            if key not in phase.metrics and len(phase.metrics) >= MAX_GROUPS:
                self.budget.limits["metric_groups"] += 1
                continue
            data = phase.metrics.setdefault(key, {"count": 0, "first_ns": now, "last_ns": now,
                                                  "first": values, "last": values,
                                                  "moments": [Moments() for _ in range(8)],
                                                  "control_excluded_rows": 0})
            data["count"] += 1
            if now < data["first_ns"]:
                data["first_ns"], data["first"] = now, values
            if now >= data["last_ns"]:
                data["last_ns"], data["last"] = now, values
            for moment, value in zip(data["moments"], values):
                moment.add(value)
            excluded = any(a <= now < b for a, b in self.control_removed)
            data["control_excluded_rows"] += excluded
            bucket = (now - phase.start) // self.bin_ns
            bkey = (key, bucket)
            if bkey not in phase.progression and len(phase.progression) >= MAX_GROUPS:
                self.budget.limits["metric_progression_bins"] += 1
                continue
            entry = phase.progression.setdefault(bkey, {"count": 0, "first": values, "last": values,
                                                        "control_excluded_rows": 0})
            entry["count"] += 1
            entry["last"] = values
            entry["control_excluded_rows"] += excluded

    def second_pass(self):
        stream = Stream(self.path, self.maximum)
        for row, _ in stream:
            event = row.get("event")
            now = integer(row.get("mach_ns"))
            if event in ("header", "footer") or now is None:
                continue
            if self.header and row.get("session_id") != self.header.get("session_id"):
                continue
            for phase in self.phases:
                phase.observe(now, event)
            if event == "metrics":
                self.metrics(row)
                continue
            if not isinstance(event, str) or not event.startswith("command_"):
                continue
            identifier = integer(row.get("command_id"), 0)
            if not identifier:
                if event in ("command_wait_begin", "command_wait_end"):
                    self.wait(row, None)
                else:
                    self.problems["zero_command_identity"] += 1
                continue
            command = self.command(identifier)
            command.owner = tag(row)
            if event in ("command_wait_begin", "command_wait_end"):
                self.wait(row, command)
                continue
            if event in command.times:
                self.problems["duplicate_command_event"] += 1
                continue
            command.times[event] = now
            if event == "command_create_begin":
                label = row.get("label")
                command.label = label[:96] if isinstance(label, str) else "unmatched_label"
            if event == "command_create_end":
                command.created = row.get("created") is True
                if not command.created:
                    command.terminal = "create_failed"
            for phase in self.phases:
                if phase.start <= now < phase.end:
                    phase.command_counts[event] += 1
            for label, begin, end in (("create_call_wall_ns", "command_create_begin", "command_create_end"),
                                      ("commit_call_wall_ns", "command_commit_begin", "command_commit_end"),
                                      ("commit_to_completion_observation_ns", "command_commit_begin", "command_complete")):
                if label not in command.calculated and begin in command.times and end in command.times:
                    self.phase_duration(label, command.times[begin], command.times[end])
                    command.calculated.add(label)
            if event == "command_complete":
                command.terminal = "completed"
                self.gpu(row, command)
                start = row.get("gpu_start_seconds")
                if isinstance(start, (int, float)) and math.isfinite(start) and start > 0 and "command_commit_begin" in command.times:
                    self.phase_duration("commit_to_gpu_start_ns", command.times["command_commit_begin"], round(start * 1e9))
            elif event == "command_abandon":
                command.terminal = "abandoned"
            if command.terminal:
                self.terminal(command)
        for command in list(self.active.values()) + list(self.recent.values()):
            self.finish_command(command)
        self.wait_totals["unmatched_wait_begin"] = len(self.open_waits)
        self.second_stream = stream

    def result(self, before, after):
        header = self.header or {}
        footer = self.footer or {}
        issues = []
        valid_header = (header.get("schema") == 1 and header.get("cpu_clock") == "mach_absolute_time_ns"
                        and header.get("gpu_clock") == "system_mach_seconds"
                        and isinstance(header.get("source_sha"), str) and len(header["source_sha"]) == 40
                        and all(c in "0123456789abcdefABCDEF" for c in header["source_sha"])
                        and isinstance(header.get("build_id"), str) and bool(header["build_id"])
                        and utc_from_mach(self.first_ns or 0, header) is not None)
        if not valid_header:
            issues.append("missing_or_invalid_header")
        if self.footer is None:
            issues.append("missing_footer_live_or_unfinalized")
        limited_input = before.st_size > self.maximum
        if limited_input:
            issues.append("input_byte_limit_prefix_only")
        changed = ((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) !=
                   (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns) or
                   self.stream.digest.digest() != self.second_stream.digest.digest())
        if changed:
            issues.append("input_changed_during_analysis")
        if self.stream.errors:
            issues.append("malformed_or_oversized_input_rows")
        if self.stream.unterminated_tail:
            issues.append("unterminated_tail")
        if self.problems:
            issues.append("record_integrity_or_pairing_problems")
        loss = {key: integer(footer.get(key), 0) for key in ("dropped_full", "dropped_contention", "dropped_budget", "names_truncated", "io_errors")}
        if any(loss.values()):
            issues.append("declared_loss_or_writer_error")
        if footer.get("stop_reason") in ("duration_limit", "byte_limit", "io_error"):
            issues.append("bounded_or_error_cutoff")
        if footer.get("pending_created_at_cutoff", 0) or footer.get("pending_committed_at_cutoff", 0):
            issues.append("pending_native_commands_at_cutoff")
        if self.budget.limits:
            issues.append("analysis_working_state_limit")
        if self.world_start is None:
            issues.append("quiet_world_start_unavailable")
        if not self.frame_totals.get("frame_rows"):
            issues.append("frame_telemetry_unavailable")
        if not self.command_totals.get("valid_gpu_intervals"):
            issues.append("gpu_execution_times_unavailable")
        attempted = integer(footer.get("attempted"))
        gaps = max(0, (attempted if attempted is not None else self.sequence_max) - self.sequence_unique)
        if gaps or self.sequence_duplicates:
            issues.append("sequence_loss_or_duplicates")
        actual_rows = sum(self.event_counts.values()) - self.event_counts["header"] - self.event_counts["footer"]
        written = integer(footer.get("written"))
        if written is not None and written != actual_rows:
            issues.append("footer_written_count_mismatch")
        phases = []
        for phase in self.phases:
            metrics = {}
            for key, value in phase.metrics.items():
                name = key.split(":", 1)[0]
                metrics[key] = {k: v for k, v in value.items() if k != "moments"}
                metrics[key]["field_names"] = METRIC_FIELDS.get(name, tuple("value" + str(i) for i in range(8)))
                metrics[key]["field_summaries"] = [m.result() for m in value["moments"]]
                metrics[key]["last_minus_first"] = [b - a for a, b in zip(value["first"], value["last"])]
            progression = [{"metric": key, "offset_start_s": bucket * self.bin_ns / 1e9,
                            "offset_end_s": (bucket + 1) * self.bin_ns / 1e9, **value}
                           for (key, bucket), value in sorted(phase.progression.items())]
            phases.append({"name": phase.name, "requested_mach_interval": [phase.start, phase.requested_end],
                           "quiet_mach_interval": [phase.start, phase.end],
                           "requested_utc_interval_estimate": [utc_from_mach(phase.start, header), utc_from_mach(phase.requested_end, header)],
                           "observed_event_mach_interval": [phase.first_event, phase.last_event],
                           "observed_event_span_ns": phase.last_event - phase.first_event if phase.first_event is not None else None,
                           "events": dict(phase.events),
                           "observed_quality_and_dimensions": [{"width": key[0], "height": key[1], "quality": key[2], "frame_rows": count}
                                                               for key, count in sorted(phase.configurations.items())],
                           "raw_consecutive_iteration_periods": phase.raw_periods.result(),
                           "eligible_display_periods": phase.eligible_periods.result(),
                           "capture_excluded_control_periods": phase.control_periods.result(),
                           "period_exclusion_occurrences_overlap": dict(phase.frame_rejections),
                           "command_events": dict(phase.command_counts),
                           "command_cpu_observation_latencies": {k: v.result() for k, v in phase.command_latencies.items()},
                           "gpu_commands_overlapping_phase": phase.gpu_commands,
                           "gpu_unknown_owner_commands": phase.gpu_unknown_owner,
                           "gpu_envelope_ns_sum_not_frame_cost": phase.gpu_duration.result(),
                           "gpu_command_envelope_union": phase.gpu_union.result(),
                           "gpu_capture_excluded_union": phase.gpu_control_union.result(),
                           "gpu_union_by_owner_overlaps": {k: v.result() for k, v in phase.gpu_by_owner.items()},
                           "gpu_envelope_by_label_overlaps": {k: v.result() for k, v in sorted(phase.labels.items())},
                           "wait_counts": dict(phase.wait_counts), "wait_wall_ns_sum_overlaps": phase.waits.result(),
                           "wait_wall_union": phase.wait_union.result(), "wait_capture_excluded_union": phase.wait_control_union.result(),
                           "cpu_span_wall_not_cpu_time": {k: {"summed_wall_ns_overlaps": v["wall"].result(),
                                                              "union": v["union"].result(),
                                                              "capture_excluded_union": v["control_union"].result()}
                                                          for k, v in sorted(phase.cpu_spans.items())},
                           "metrics": metrics, "metric_progression": progression})
        return {"schema": 1, "tool": "quiet_diagnostic_summary", "input": str(self.path.resolve()),
                "input_identity": {"device": before.st_dev, "inode": before.st_ino, "original_bytes": before.st_size,
                                   "analyzed_prefix_bytes": self.stream.bytes, "sha256": self.stream.digest.hexdigest() if not limited_input else None,
                                   "prefix_sha256": self.stream.digest.hexdigest(), "unchanged_during_analysis": not changed},
                "classification": {"status": "invalid" if not valid_header or changed else "partial" if issues else "complete_observed_stream",
                                   "issues": issues, "source_cause_conclusion": None},
                "gpu_interval_semantics": "Union of observed app-command GPU start/end execution envelopes; may include intra-command dependency or idle time. Not system-GPU completeness, hardware busy time, occupancy or additive frame cost.",
                "provenance": {key: header.get(key) for key in ("schema", "session_id", "process_id", "source_sha", "build_id", "cpu_clock", "gpu_clock",
                               "calibration_before_mach_ns", "calibration_unix_ns", "calibration_after_mach_ns", "ring_records", "max_bytes", "max_duration_ns")},
                "footer": footer, "parse_errors": dict(self.stream.errors), "parse_error_examples": self.stream.error_examples,
                "record_problems": dict(self.problems), "declared_loss": loss,
                "sequences": {"unique": self.sequence_unique, "duplicates": self.sequence_duplicates, "min": self.sequence_min,
                              "max": self.sequence_max, "missing_against_footer_attempted_or_observed_max": gaps,
                              "out_of_file_order": self.sequence_out_of_order,
                              "out_of_order_is_allowed_across_producers": True},
                "observed_event_mach_interval": [self.first_ns, self.last_ns], "event_counts": dict(self.event_counts),
                "quiet_world": {"start_mach_ns": self.world_start, "start_source": "explicit_override" if self.override_world is not None else "quiet_world_started_marker",
                                "invalidated_mach_ns": self.world_end, "started_state_value": self.started_state},
                "capture_cpu_intervals": self.captures, "capture_metadata_inputs": self.capture_receipts,
                "first_post_capture_contaminated_periods": self.post_capture_periods,
                "frame_totals": dict(self.frame_totals), "frame_ineligible_occurrences_overlap": dict(self.frame_reasons),
                "observation_availability": {"frame_rows": self.frame_totals.get("frame_rows", 0),
                                             "consecutive_iteration_periods": self.frame_totals.get("consecutive_raw_periods", 0),
                                             "valid_gpu_command_intervals": self.command_totals.get("valid_gpu_intervals", 0),
                                             "cpu_span_records": self.command_totals.get("cpu_span_records", 0)},
                "commands": dict(self.command_totals), "terminal_command_missing_events": dict(self.command_missing),
                "unmatched_or_pending_command_examples": self.command_examples, "wait_pairing": dict(self.wait_totals),
                "working_state_limits_reached": dict(self.budget.limits), "phases": phases,
                "limitations": [
                    "Raw app iteration start-to-start periods are not LLStats histories, scanout intervals or GPU frame durations. Eligibility uses observed endpoint states; missing iterations are not bridged.",
                    "GPUStartTime/GPUEndTime define completed command envelopes, not isolated GPU busy/occupancy or per-pass cost. Envelope sums and owner unions overlap and must not be added as frame cost.",
                    "CPU spans and wrapper waits are wall intervals, not sampled CPU execution. Summed overlapping intervals are separately labeled from unions.",
                    "Capture-excluded control removes CPU capture/API intervals and the first following iteration period. Capture effects may outlast these bounds; no causal correction is inferred.",
                    "Marker fallback bounds may miss start-API work unless a preceding trigger survived; helper CPU metadata is preferred. An absent end remains open, not invented.",
                    "Metric fields are source-defined operands/counters; rectangle areas may overlap, configured probes are not accepted per-pixel taps, logical cache bytes are not physical residency.",
                    "Source/build are reported from the stream, not independently authenticated. Input hashes and exact tool/source provenance must accompany evaluation. No automatic cause or performance acceptance is produced."]}

    def run(self):
        before = self.path.stat()
        self.first_pass()
        self.prepare_windows()
        self.analyze_frames()
        self.second_pass()
        return self.result(before, self.path.stat())


def markdown(report):
    def ms(value):
        return "—" if value is None else f"{value / 1e6:.3f}"
    lines = [f"Source `{report['provenance'].get('source_sha')}`, build `{report['provenance'].get('build_id')}`.",
             f"Stream status: **{report['classification']['status']}**. This report assigns no source cause.", "",
             "| Phase | Eligible periods | Control periods | Control p50 / p95 ms | GPU envelope union ms | Wait union ms |",
             "|---|---:|---:|---:|---:|---:|"]
    for phase in report["phases"]:
        e, c = phase["eligible_display_periods"], phase["capture_excluded_control_periods"]
        name = phase["name"].replace("|", "\\|").replace("\n", " ")
        lines.append(f"| {name} | {e['count']} | {c['count']} | {ms(c['p50_ns'])} / {ms(c['p95_ns'])} | "
                     f"{ms(phase['gpu_command_envelope_union']['union_wall_ns'] if phase['gpu_commands_overlapping_phase'] else None)} | "
                     f"{ms(phase['wait_wall_union']['union_wall_ns'] if phase['wait_wall_ns_sum_overlaps']['count'] else None)} |")
    lines += ["", "GPU unions are overlapping command envelopes, not hardware occupancy or additive frame costs.",
              "CPU spans/waits are wall time. Capture control exclusions are conservative and do not prove all observer effects ended."]
    if report["classification"]["issues"]:
        lines += ["", "Evidence limitations: " + ", ".join(report["classification"]["issues"]) + "."]
    return "\n".join(lines) + "\n"


def phase_spec(text):
    try:
        name, start, end = text.split(":")
        start, end = float(start), float(end)
        if not name or len(name) > 32 or not all(math.isfinite(x) for x in (start, end)) or not 0 <= start < end <= 3600:
            raise ValueError
        return name, start, end
    except ValueError as error:
        raise argparse.ArgumentTypeError("phase must be NAME:START_SECONDS:END_SECONDS within0–3600") from error


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--markdown", type=Path)
    p.add_argument("--world-start-mach-ns", type=int)
    p.add_argument("--phase", action="append", type=phase_spec)
    p.add_argument("--capture-directory", type=Path)
    p.add_argument("--max-bytes", type=int, default=MAX_BYTES,
                   help="explicit bounded input prefix in bytes (at most 268435456; default 67108864)")
    p.add_argument("--started-state", type=int, default=24)
    p.add_argument("--bin-seconds", type=float, default=10)
    return p


def safe_write(path, text, input_path):
    if path.resolve() == input_path.resolve() or (path.exists() and os.path.samefile(path, input_path)):
        raise ValueError("output must not alias the original diagnostic input")
    if path.exists() or path.is_symlink():
        raise FileExistsError("derived evidence output already exists; choose a new path")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, prefix="." + path.name, delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(text)
        # Same-directory hard-link publication is atomic and exclusive. It
        # refuses an existing file or symlink even if one races the check.
        os.link(temporary, path, follow_symlinks=False)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


def main(argv=None):
    p = parser()
    args = p.parse_args(argv)
    if not 0 < args.max_bytes <= MAX_INPUT_BYTES or not math.isfinite(args.bin_seconds) or not 0 < args.bin_seconds <= 3600:
        p.error("max-bytes must be 1..268435456 and bin-seconds must be positive and at most 3600")
    if args.world_start_mach_ns is not None and not 0 < args.world_start_mach_ns < (1 << 64):
        p.error("world-start-mach-ns must be a positive uint64")
    phases = args.phase or DEFAULT_PHASES
    if len(phases) > 8 or len({name for name, _, _ in phases}) != len(phases) or any(a[2] > b[1] for a, b in zip(sorted(phases, key=lambda x: x[1]), sorted(phases, key=lambda x: x[1])[1:])):
        p.error("phase names must be unique and windows must not overlap")
    try:
        report = Analyzer(args.input, maximum=args.max_bytes, phases=phases, world_start=args.world_start_mach_ns,
                          capture_directory=args.capture_directory, started_state=args.started_state,
                          bin_seconds=args.bin_seconds).run()
        safe_write(args.output, json.dumps(report, indent=2, allow_nan=False) + "\n", args.input)
        if args.markdown:
            if args.markdown.resolve() == args.output.resolve():
                raise ValueError("markdown and JSON outputs must be different files")
            safe_write(args.markdown, markdown(report), args.input)
    except (OSError, ValueError) as error:
        p.exit(2, f"quiet diagnostic summary: {error}\n")
    print(json.dumps({"output": str(args.output), "status": report["classification"]["status"],
                      "source_sha": report["provenance"]["source_sha"], "phases": len(report["phases"])}))
    return 2 if report["classification"]["status"] == "invalid" else 0


if __name__ == "__main__":
    raise SystemExit(main())
