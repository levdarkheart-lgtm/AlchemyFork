"""Sanitized synthetic Home joins; never runtime evidence or actual identity checks.

Only synthetic_records() and synthetic_context() are required by the private
reducer tests. Every call returns fresh values. No file/network/clock/GPU action
runs on import or while constructing the fixture.
"""
import struct

NS = 1_000_000_000
BASE = 1_000_000_000_000
PERIOD_NS = 100_000_000
SESSION = 1
SOURCE_SHA = "a" * 40
PROCESS_ID = 4242
DIMENSIONS = [64, 48]
QUALITY = 6
FLAGS = 1 | (1 << 1) | (1 << 7) | (1 << 8) | (1 << 10)
MASK64 = (1 << 64) - 1


def _at(milliseconds):
    return BASE + milliseconds * 1_000_000


def _frame(milliseconds):
    """The native display frame containing a positive world-age instant."""
    return 10_001 + (milliseconds + 99) // 100


def _bits(translation=0.0):
    matrix = [0.0] * 16
    for axis in range(4):
        matrix[axis * 5] = 1.0
    matrix[12] = translation  # Authored toy transform, not a real location.
    return [struct.unpack("<I", struct.pack("<f", value))[0] for value in matrix]


def _fnv(values):
    result = 14695981039346656037
    for value in values:
        result = ((result ^ value) * 1099511628211) & MASK64
    return result


def synthetic_context():
    """Matching test assertions only; these flags do not certify a live run."""
    return {
        "schema": 1,
        "run": "synthetic-only-not-runtime",
        "arm": "cap2",
        "expected_group_cap": 2,
        "expected_source_sha": SOURCE_SHA,
        "expected_process_id": PROCESS_ID,
        "expected_dimensions": list(DIMENSIONS),
        "expected_quality": QUALITY,
        "checks": {name: True for name in (
            "runtime_identity", "actual_scene", "native_fullscreen",
            "quiet_camera_and_controls_matched", "sole_openhtj2k",
            "no_capture_or_validation", "single_main_renderer")},
        "required_measures": ["cadence", "admission", "preparation", "memory", "input_presentation"],
        "input_trials": [{
            "name": "synthetic-isolated-keyboard",
            "world_age_s": [224.99, 225.4],
            "endguard_frames": 1,
            "baseline_camera_bits": _bits() + _bits(),
            "external_cue_mach_ns": _at(225_005),
            "last_input_serial_before_cue": 0,
            "isolated": True,
            "allowed_input_kinds": [4],
            "stable_views": 3,
            "restored_before_next_trial": True,
        }],
    }


def synthetic_records():
    """Eligible100ms cadence plus focused complete joins, not a world emulator.

    First FRAME ends at BASE and anchors age0; frames extend beyond260s. A
    canonical blocked group/adopt example is around160s. Three stable camera
    views precede an isolated event at225.010s; its changed view is actually used by
    a later UI frame. Logical groups/tickets, display frames, view serials,
    commands, resource generations and drawable IDs are separate namespaces.
    """
    rows = []

    def add(event, when, frame=0, **extra):
        row = {"event": event, "session_id": SESSION, "mach_ns": when,
               "frame_id": frame, "view_kind": 1, "view_index": -1}
        row.update(extra)
        rows.append(row)
        return row

    def metric(name, when, frame, values):
        if len(values) != 8:
            raise ValueError("Synthetic metrics must follow values[8]")
        return add("metrics", when, frame, name=name, values=list(values))

    def command(identifier, frame, create_ms, commit_ms, gpu_start_ms, gpu_end_ms, label):
        add("command_create_begin", _at(create_ms) + 1_000, frame, command_id=identifier, label=label)
        add("command_create_end", _at(create_ms) + 21_000, frame,
            command_id=identifier, created=True)
        add("command_commit_begin", _at(commit_ms), frame, command_id=identifier)
        add("command_commit_end", _at(commit_ms) + 20_000, frame, command_id=identifier)
        add("command_complete", _at(gpu_end_ms) + 1_000_000, frame,
            command_id=identifier, native_status=4, native_error_code=0,
            gpu_time_valid=True, gpu_start_seconds=_at(gpu_start_ms) / NS,
            gpu_end_seconds=_at(gpu_end_ms) / NS)

    def camera(sequence, group, milliseconds, changed=False, *, inherited=False):
        frame = _frame(milliseconds)
        modelview = _bits(0.25 if changed or inherited else 0.0)
        projection = _bits()
        previous_input, latest_input = (0, 1) if changed else ((1, 1) if inherited else (0, 0))
        event_lower, event_upper = (_at(225_010) - 10, _at(225_010) + 10) if changed or inherited else (0, 0)
        metric("view_input", _at(milliseconds), frame,
               [sequence, group, previous_input, latest_input,
                event_lower, event_upper, _at(225_013) if changed or inherited else 0,
                int(changed or inherited)])
        metric("view_camera", _at(milliseconds) + 100, frame,
               [sequence, group, _fnv(modelview), _fnv(projection),
                int(sequence > 1), int(changed), 1, 0])
        if sequence == 1 or changed:
            components = modelview + projection
            for first in (0, 6, 12, 18, 24, 30):
                payload = components[first:first + 6]
                payload += [0] * (6 - len(payload))
                metric("view_camera_bits", _at(milliseconds) + 200 + first,
                       frame, [sequence, first, *payload])
        return frame

    def preparation(sequence, group, milliseconds, *, changed=False, inherited=False, blocked=False):
        frame = _frame(milliseconds)
        metric("view_gather_begin", _at(milliseconds - 6), frame, [0] * 8)
        metric("view_scene_gathered", _at(milliseconds - 2), frame,
               [group, 1, 0, 0, 0, 1, 1, 1])
        metric("view_destination_generation", _at(milliseconds - 2) + 10_000,
               frame, [group, 0, 601, 2, 1, 1, 0, 0])
        camera(sequence, group, milliseconds, changed, inherited=inherited)
        adopt_start = _at(160_054) if blocked else _at(milliseconds + 1)
        adopt_end = adopt_start + 1_000_000
        encode_start = adopt_end + 1_000_000
        encode_end = encode_start + 2_000_000
        metric("view_inputs_adopted", adopt_end + 1_000, frame,
               [sequence, group, 100 + sequence, adopt_start, adopt_end, 1, 1, 1])
        metric("view_inputs_encoded", encode_end + 1_000, frame,
               [sequence, group, 100 + sequence, encode_start, encode_end, 1, 1, 0])
        return frame

    def memory(milliseconds, block, boundary, footprint, allocations, acquisitions, reuses):
        frame = _frame(milliseconds)
        sampled = _at(milliseconds)
        metric("admission_memory", sampled + 10_000, frame,
               [block, boundary, sampled, footprint, 1, allocations, 60_000_000, 10_000])
        metric("admission_target_pool", sampled + 20_000, frame,
               [block, boundary, 2, 4_000_000, 1, acquisitions, reuses, 2])
        metric("admission_frame_pages", sampled + 30_000, frame,
               [block, 1, 2_097_152, 1_048_576, 524_288, 2, 1, 0])
        metric("admission_frame_pages", sampled + 31_000, frame,
               [block, 2, 1_048_576, 0, 0, 1, 0, 0])
        metric("admission_memory_observer", sampled + 50_000, frame,
               [block, boundary, sampled, sampled + 49_000, 0, 0, 0, 0])

    # First frame ends at0; first/last100ms periods bracket the full0..260s.
    for index in range(2602):
        iteration = index + 1
        end = BASE + index * PERIOD_NS
        add("frame", end, 10_000 + iteration,
            iteration_id=iteration, begin_display_frame=9_999 + iteration,
            end_display_frame=10_000 + iteration,
            start_mach_ns=end - PERIOD_NS, end_mach_ns=end,
            begin_startup_state=24, end_startup_state=24,
            begin_flags=FLAGS, end_flags=FLAGS,
            begin_teleport_state=0, end_teleport_state=0,
            outcome=0, native_exact=True, drawable_width=DIMENSIONS[0],
            drawable_height=DIMENSIONS[1], quality=QUALITY)

    old_light_frame, old_post_frame = _frame(159_840), _frame(159_940)
    command(1, old_light_frame, 159_840, 159_842, 160_005, 160_050,
            "Alchemy native world light-map and deferred-light chain")
    command(2, old_post_frame, 159_940, 159_942, 160_006, 160_075,
            "Alchemy native world opaque and optical post")
    metric("main_group_command", _at(159_840) + 30_000, old_light_frame,
           [41, 101, 1, 2, 1, 0, 1, 2])
    metric("main_group_command", _at(159_940) + 30_000, old_post_frame,
           [42, 102, 2, 3, 2, 1, 1, 2])
    current_frame = preparation(1, 43, 160_010, blocked=True)
    metric("main_admission_block", _at(160_012), current_frame, [1, 43, 41, 1, 2, 2, 0, 2])
    metric("main_admission_target", _at(160_012) + 10_000, current_frame,
           [1, 101, SESSION, 1, 3, 1, 1, 1])
    metric("main_admission_target", _at(160_012) + 20_000, current_frame,
           [1, 102, SESSION, 2, 3, 1, 0, 1])
    add("command_wait_begin", _at(160_014), old_light_frame, command_id=1,
        caller_frame_id=current_frame, caller_view_kind=1, caller_view_index=-1)
    add("command_wait_end", _at(160_052), old_light_frame, command_id=1,
        caller_frame_id=current_frame, caller_view_kind=1, caller_view_index=-1,
        native_status=4)
    metric("main_admission_unblock", _at(160_052) + 200_000, current_frame,
           [1, 43, 41, _at(160_012), _at(160_052) + 100_000,
            38_000_000, 100_000, 1])
    metric("main_admission_observer", _at(160_052) + 300_000, current_frame,
           [1, _at(160_012), _at(160_013),
            _at(160_052) + 100_000, _at(160_052) + 250_000, 0, 0, 0])
    command(3, current_frame, 160_053, 160_062, 160_070, 160_090,
            "Alchemy native world G-buffer and ordered sky")
    metric("main_group_command", _at(160_053) + 30_000, current_frame,
           [43, 103, 3, 1, 2, 1, 1, 2])
    # More than100ms between real task samples; no fake after-wait sample.
    memory(159_839, 0, 2, 100_000_000, 80_000_000, 100, 80)
    memory(160_012, 1, 0, 105_000_000, 84_000_000, 104, 83)
    memory(160_250, 0, 2, 103_000_000, 82_000_000, 108, 86)

    for sequence, group, milliseconds in ((2, 1001, 224_720), (3, 1002, 224_820), (4, 1003, 224_920)):
        preparation(sequence, group, milliseconds)
    input_frame = _frame(225_013)
    metric("input_dispatch", _at(225_013), input_frame,
           [1, 4, 230_000_000_000, 230_002_000_000,
            _at(225_012) - 10, _at(225_012) + 10,
            _at(225_012) - 10, _at(225_013)])
    changed_frame = preparation(5, 1004, 225_020, changed=True)
    command(4, changed_frame, 225_021, 225_027, 225_030, 225_045,
            "Alchemy native world G-buffer and ordered sky")
    metric("main_group_command", _at(225_021) + 30_000, changed_frame,
           [1004, 104, 4, 1, 1, 0, 1, 2])
    # Dense population: three stable camera/UI parents, changed view with a
    # retained stable world, first actual changed-world presentation, then one
    # endguard parent. Every main owner boundary stays inside its FRAME.
    preparation(6, 1005, 225_120, inherited=True)
    preparation(7, 1006, 225_220, inherited=True)

    def ui_tuple(identifier, drawable, milliseconds, source_frame, color, color_gen, depth, depth_gen):
        frame = _frame(milliseconds)
        command(identifier, frame, milliseconds, milliseconds + 3,
                milliseconds + 4, milliseconds + 13,
                "Alchemy native world final presentation")
        observed = _at(milliseconds + 1)
        metric("ui_background_source", observed, frame,
               [source_frame, 7, color, color_gen, depth, depth_gen, 1, 0])
        metric("drawable_present_submit", observed + 10_000, frame,
               [identifier, drawable, 7, source_frame, 1, frame, frame, 1])
        metric("drawable_source_identity", observed + 11_000, frame,
               [identifier, drawable, color, color_gen, depth, depth_gen, 7, 0])
        metric("drawable_host_clock", observed + 12_000, frame,
               [identifier, drawable, observed, observed + 5, observed + 10, 1, *DIMENSIONS])
        metric("ui_background_submission", _at(milliseconds + 3) + 30_000, frame,
               [source_frame, 1, 1, 1, 6, 1, 1, 7])
        metric("drawable_presented", _at(milliseconds + 21), frame,
               [identifier, drawable, source_frame, _at(milliseconds + 16),
                _at(milliseconds + 21), 1, frame, 1])

    ui_tuple(6, 89, 224_760, _frame(224_720), 721, 9, 722, 10)
    ui_tuple(7, 90, 224_860, _frame(224_820), 731, 11, 732, 12)
    ui_tuple(8, 91, 224_960, _frame(224_920), 711, 7, 712, 8)
    ui_tuple(9, 92, 225_060, _frame(224_920), 711, 7, 712, 8)
    # Preserve original first-changed tuple IDs and225.140 host presentedTime.
    ui_tuple(5, 88, 225_124, changed_frame, 701, 5, 702, 6)
    ui_tuple(10, 93, 225_260, changed_frame, 701, 5, 702, 6)

    rows.sort(key=lambda row: row["mach_ns"])
    for sequence, row in enumerate(rows, 1):
        row["sequence"] = sequence
    header = {"event": "header", "schema": 1, "session_id": SESSION,
              "process_id": PROCESS_ID, "source_sha": SOURCE_SHA,
              "build_id": "synthetic_home", "cpu_clock": "mach_absolute_time_ns",
              "gpu_clock": "system_mach_seconds",
              "calibration_before_mach_ns": BASE - NS,
              "calibration_after_mach_ns": BASE - NS + 10,
              "calibration_unix_ns": 1_700_000_000_000_000_000,
              "lossy_nonblocking_producers": True}
    footer = {"event": "footer", "session_id": SESSION, "mach_ns": _at(260_200),
              "attempted": len(rows), "queued": len(rows), "written": len(rows),
              "stop_reason": "requested", "dropped_full": 0,
              "dropped_contention": 0, "dropped_budget": 0,
              "names_truncated": 0, "io_errors": 0,
              "commands_created": 10, "commands_committed": 10,
              "commands_completed": 10, "commands_abandoned": 0,
              "pending_created_at_cutoff": 0, "pending_committed_at_cutoff": 0}
    return [header, *rows, footer]
