"""Offline native JSONL reduction; no viewer, native cost run or profiler."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("quiet_diagnostic_summary", TOOLS / "quiet_diagnostic_summary.py")
summary = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = summary
SPEC.loader.exec_module(summary)
BASE = 1_000_000_000_000
NS = 1_000_000_000
FLAGS = 1 | (1 << 1) | (1 << 7) | (1 << 8) | (1 << 10)


class Stream:
    def __init__(self):
        self.sequence = 0
        self.rows = [{"event": "header", "schema": 1, "session_id": 1, "process_id": 123,
                      "source_sha": "b" * 40, "build_id": "fixture", "cpu_clock": "mach_absolute_time_ns",
                      "gpu_clock": "system_mach_seconds", "calibration_before_mach_ns": BASE,
                      "calibration_after_mach_ns": BASE + 10, "calibration_unix_ns": 1_700_000_000_000_000_000}]

    def add(self, event, seconds, **fields):
        self.sequence += 1
        row = {"event": event, "session_id": 1, "sequence": self.sequence,
               "mach_ns": BASE + round(seconds * NS), "frame_id": 1, "view_kind": 1, "view_index": -1}
        row.update(fields)
        self.rows.append(row)
        return row

    def marker(self, name, seconds):
        return self.add("marker", seconds, name=name)

    def frame(self, iteration, seconds, **fields):
        values = dict(iteration_id=iteration, begin_display_frame=iteration - 1, end_display_frame=iteration,
                      start_mach_ns=BASE + round(seconds * NS), end_mach_ns=BASE + round((seconds + .5) * NS),
                      begin_flags=FLAGS, end_flags=FLAGS, begin_startup_state=24, end_startup_state=24,
                      begin_teleport_state=0, end_teleport_state=0, outcome=0, native_exact=True,
                      drawable_width=64, drawable_height=48, quality=6)
        values.update(fields)
        return self.add("frame", seconds + .5, **values)

    def command(self, identifier, start, end, *, view=1, frame=1):
        common = dict(command_id=identifier, view_kind=view, frame_id=frame)
        self.add("command_create_begin", max(0, start - .2), label="Alchemy native world base lighting", **common)
        self.add("command_create_end", max(0, start - .15), created=True, **common)
        self.add("command_commit_begin", max(0, start - .1), **common)
        self.add("command_commit_end", max(0, start - .05), **common)
        self.add("command_complete", end + .1, native_status=4, gpu_time_valid=True,
                 gpu_start_seconds=BASE / NS + start, gpu_end_seconds=BASE / NS + end, **common)

    def write(self, path, footer=True, **footer_fields):
        rows = list(self.rows)
        if footer:
            values = dict(event="footer", session_id=1, mach_ns=BASE + 130 * NS, attempted=self.sequence,
                          queued=len(rows) - 1, written=len(rows) - 1, stop_reason="requested",
                          dropped_full=0, dropped_contention=0, dropped_budget=0, names_truncated=0,
                          io_errors=0, pending_created_at_cutoff=0, pending_committed_at_cutoff=0)
            values.update(footer_fields)
            rows.append(values)
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))
        return path


class SummaryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.path = self.directory / "native.jsonl"

    def analyze(self, stream, **options):
        stream.write(self.path)
        original = self.path.read_bytes()
        result = summary.Analyzer(self.path, **options).run()
        self.assertEqual(self.path.read_bytes(), original)
        self.assertEqual(result["input_identity"]["sha256"], hashlib.sha256(original).hexdigest())
        return result

    def test_raw_consecutive_periods_endpoint_eligibility_and_no_gap_bridge(self):
        stream = Stream(); stream.marker("quiet_world_started", 0)
        for iteration in range(1, 6):
            stream.frame(iteration, iteration - .9,
                         **({"begin_flags": FLAGS & ~(1 << 1), "end_flags": FLAGS & ~(1 << 1)} if iteration == 3 else {}))
        stream.frame(7, 6.1)  # Missing iteration6 must not create a two-second period.
        result = self.analyze(stream)
        phase = result["phases"][0]
        self.assertEqual(phase["raw_consecutive_iteration_periods"]["count"], 4)
        self.assertEqual(phase["eligible_display_periods"]["count"], 2)
        self.assertEqual(phase["capture_excluded_control_periods"]["count"], 2)
        self.assertEqual(phase["capture_excluded_control_periods"]["p50_ns"], NS)
        self.assertEqual(result["frame_totals"]["iteration_id_gaps_or_reordering"], 1)
        self.assertEqual(phase["period_exclusion_occurrences_overlap"]["unfocused"], 2)
        self.assertEqual(result["provenance"]["process_id"], 123)

    def test_no_display_and_teleport_are_separate_observed_exclusions(self):
        stream = Stream(); stream.marker("quiet_world_started", 0)
        stream.frame(1,.1)
        stream.frame(2,1.1,end_display_frame=1,native_exact=False)
        stream.frame(3,2.1,begin_display_frame=1,end_display_frame=2,begin_teleport_state=1)
        result = self.analyze(stream)
        self.assertEqual(result["frame_ineligible_occurrences_overlap"]["no_display_increment"], 1)
        self.assertEqual(result["frame_ineligible_occurrences_overlap"]["teleport"], 1)
        self.assertEqual(result["phases"][0]["eligible_display_periods"]["count"], 0)

    def test_quality_and_resolution_changes_remain_visible_but_leave_control(self):
        stream=Stream();stream.marker("quiet_world_started",0)
        for i in range(1,7):
            stream.frame(i,i-.9,quality=6 if i<3 else 5,
                         drawable_width=64 if i<5 else 128,drawable_height=48 if i<5 else 96)
        phase=self.analyze(stream)["phases"][0]
        self.assertEqual(phase["eligible_display_periods"]["count"],5)
        self.assertEqual(phase["capture_excluded_control_periods"]["count"],3)
        self.assertEqual(phase["period_exclusion_occurrences_overlap"]["quality_or_resolution_change"],2)
        self.assertEqual({(r["width"],r["height"],r["quality"]):r["frame_rows"]
                          for r in phase["observed_quality_and_dimensions"]},
                         {(64,48,6):2,(64,48,5):2,(128,96,5):2})

    def test_gpu_unions_are_clipped_and_not_additive_frame_cost(self):
        stream = Stream(); stream.marker("quiet_world_started", 0)
        stream.command(1,1,4); stream.command(2,3,5,view=4)
        result = self.analyze(stream)
        phase = result["phases"][0]
        self.assertEqual(phase["gpu_envelope_ns_sum_not_frame_cost"]["sum"], 5*NS)
        self.assertEqual(phase["gpu_command_envelope_union"]["union_wall_ns"], 4*NS)
        self.assertEqual(phase["gpu_union_by_owner_overlaps"]["main"]["union_wall_ns"], 3*NS)
        self.assertEqual(phase["gpu_union_by_owner_overlaps"]["shadow"]["union_wall_ns"], 2*NS)
        self.assertIsNone(result["classification"]["source_cause_conclusion"])
        self.assertEqual(result["commands"]["completed"], 2)

    def test_capture_and_first_post_capture_period_are_excluded(self):
        stream = Stream(); stream.marker("quiet_world_started",0)
        stream.add("metrics",.5,name="quiet_capture_trigger",values=[1,0,0,0,0,0,0,0])
        stream.marker("quiet_capture_started",.6); stream.marker("quiet_capture_stopped",1.5)
        for i in range(1,6): stream.frame(i,i-.9)
        result = self.analyze(stream)
        phase = result["phases"][0]
        self.assertEqual(phase["eligible_display_periods"]["count"],4)
        self.assertEqual(phase["capture_excluded_control_periods"]["count"],1)
        self.assertEqual(result["first_post_capture_contaminated_periods"],[(BASE+round(2.1*NS),BASE+round(3.1*NS))])
        self.assertEqual(phase["period_exclusion_occurrences_overlap"]["first_post_capture_period"],1)

    def test_helper_cpu_bounds_refine_one_capture_without_losing_later_marker_capture(self):
        capture = self.directory / "captures" / "quiet-early-frame-capture"
        capture.mkdir(parents=True)
        metadata = {"quiet_cpu_clock":"mach_absolute_time_ns", "quiet_start_request_monotonic_ns":BASE+NS,
                    "quiet_start_return_monotonic_ns":BASE+NS+1, "quiet_stop_return_monotonic_ns":BASE+2*NS,
                    "quiet_trigger":"quiet_early", "status":"stopped"}
        (capture/"metadata.jsonl").write_text(json.dumps(metadata)+"\n")
        stream = Stream(); stream.marker("quiet_world_started",0)
        stream.marker("quiet_capture_started",1.1); stream.marker("quiet_capture_stopped",2.1)
        stream.marker("quiet_capture_started",61); stream.marker("quiet_capture_stopped",62)
        result = self.analyze(stream,capture_directory=capture.parent)
        self.assertEqual(len(result["capture_cpu_intervals"]),2)
        self.assertEqual(result["capture_cpu_intervals"][0]["start_ns"],BASE+NS)
        self.assertEqual(result["capture_cpu_intervals"][0]["source"],"helper_cpu_api_bounds")
        self.assertEqual(result["capture_cpu_intervals"][1]["start_ns"],BASE+61*NS)

    def test_missing_capture_stop_stays_open_and_world_invalidation_censors_phases(self):
        stream = Stream(); stream.marker("quiet_world_started",0)
        stream.marker("quiet_capture_started",1); stream.marker("quiet_world_invalidated",4)
        stream.command(1,2,8)
        result = self.analyze(stream)
        self.assertIsNone(result["capture_cpu_intervals"][0]["end_ns"])
        self.assertTrue(result["capture_cpu_intervals"][0]["end_missing"])
        self.assertEqual(result["phases"][0]["quiet_mach_interval"],[BASE,BASE+4*NS])
        self.assertEqual(result["phases"][0]["gpu_command_envelope_union"]["union_wall_ns"],2*NS)
        self.assertEqual(result["phases"][0]["gpu_capture_excluded_union"]["union_wall_ns"],0)

    def test_waits_unknown_command_and_cpu_spans_have_separate_wall_unions(self):
        stream = Stream(); stream.marker("quiet_world_started",0)
        common=dict(command_id=0,frame_id=0,view_kind=0,caller_frame_id=20,caller_view_kind=1,caller_view_index=-1)
        stream.add("command_wait_begin",1,**common); stream.add("command_wait_end",4,native_status=4,**common)
        stream.add("metrics",4,name="cpu_display",values=[BASE+NS,BASE+4*NS,0,0,0,0,0,0])
        stream.add("metrics",2,name="quiet_capture_trigger",values=[1,0,0,0,0,0,0,0])
        stream.marker("quiet_capture_started",2.1); stream.marker("quiet_capture_stopped",3)
        result = self.analyze(stream)
        phase=result["phases"][0]
        self.assertEqual(phase["wait_wall_union"]["union_wall_ns"],3*NS)
        self.assertEqual(phase["wait_capture_excluded_union"]["union_wall_ns"],2*NS)
        self.assertEqual(result["wait_pairing"]["paired_unknown_command_waits"],1)
        self.assertEqual(phase["cpu_span_wall_not_cpu_time"]["cpu_display"]["capture_excluded_union"]["union_wall_ns"],2*NS)

    def test_metric_progression_preserves_units_counter_deltas_and_state_changes(self):
        stream=Stream();stream.marker("quiet_world_started",0)
        stream.add("metrics",1,name="source_cache",values=[1,2,30,100,4,0,0,5])
        stream.add("metrics",2,name="source_cache",values=[2,3,70,100,9,0,1,10])
        stream.add("metrics",61,name="local_light_workload",values=[4,3,1,80,64,48,1,0])
        result=self.analyze(stream)
        cache=result["phases"][0]["metrics"]["source_cache:main"]
        self.assertEqual(cache["last_minus_first"],[1,1,40,0,5,0,1,5])
        self.assertEqual(cache["field_names"][2],"estimated_bytes")
        self.assertEqual(result["phases"][2]["metrics"]["local_light_workload:main"]["first"][0],4)

    def test_missing_footer_loss_out_of_order_and_unmatched_events_are_explicit(self):
        stream=Stream();stream.marker("quiet_world_started",0);stream.command(1,1,2)
        stream.rows[-1],stream.rows[-2]=stream.rows[-2],stream.rows[-1]
        del stream.rows[2] # Lost create-begin, retaining its reserved sequence.
        stream.write(self.path,footer=False)
        result=summary.Analyzer(self.path).run()
        self.assertIn("missing_footer_live_or_unfinalized",result["classification"]["issues"])
        self.assertEqual(result["sequences"]["out_of_file_order"],1)
        self.assertEqual(result["sequences"]["missing_against_footer_attempted_or_observed_max"],1)
        self.assertEqual(result["terminal_command_missing_events"]["command_create_begin"],1)
        self.assertEqual(result["commands"]["completed"],1)

    def test_declared_budget_cutoff_and_pending_native_command_are_not_success(self):
        stream=Stream();stream.marker("quiet_world_started",0)
        stream.add("command_create_begin",1,command_id=1,label="Alchemy idle barrier")
        stream.add("command_create_end",1.1,command_id=1,created=True)
        stream.add("command_commit_begin",1.2,command_id=1)
        stream.write(self.path,stop_reason="byte_limit",dropped_budget=2,pending_created_at_cutoff=1,pending_committed_at_cutoff=1)
        result=summary.Analyzer(self.path).run()
        self.assertEqual(result["classification"]["status"],"partial")
        self.assertIn("pending_native_commands_at_cutoff",result["classification"]["issues"])
        self.assertEqual(result["commands"]["pending_at_observed_cutoff"],1)
        self.assertEqual(result["phases"][0]["gpu_commands_overlapping_phase"],0)

    def test_prefix_hash_line_bound_and_missing_anchor_do_not_invent_data(self):
        stream=Stream();stream.command(1,1,2);stream.write(self.path)
        raw=self.path.read_bytes();limit=raw.index(b"\n")+1
        result=summary.Analyzer(self.path,maximum=limit).run()
        self.assertIsNone(result["input_identity"]["sha256"])
        self.assertEqual(result["input_identity"]["prefix_sha256"],hashlib.sha256(raw[:limit]).hexdigest())
        self.assertEqual(result["phases"],[])
        self.assertIn("quiet_world_start_unavailable",result["classification"]["issues"])
        self.path.write_bytes(raw+b"x"*(summary.MAX_LINE+100)+b"\n")
        result=summary.Analyzer(self.path).run()
        self.assertEqual(result["parse_errors"]["oversized_line"],1)

    def test_state_capacity_and_quantile_limits_are_labeled_not_silently_exact(self):
        stream=Stream();stream.marker("quiet_world_started",0)
        for i in range(1,4):stream.add("command_create_begin",i,command_id=i,label="Alchemy idle barrier")
        for i in range(1,5):stream.frame(i,i-.9)
        stream.write(self.path)
        with mock.patch.object(summary,"MAX_COMMANDS",1),mock.patch.object(summary,"MAX_PERIOD_SAMPLES",1):
            result=summary.Analyzer(self.path).run()
        self.assertIn("active_command_capacity",result["working_state_limits_reached"])
        periods=result["phases"][0]["raw_consecutive_iteration_periods"]
        self.assertFalse(periods["quantiles_exact"])
        self.assertIsNone(periods["p95_ns"])

    def test_cli_custom_windows_outputs_and_alias_guard(self):
        stream=Stream();stream.command(1,1,2);stream.write(self.path)
        output=self.directory/"summary.json";md=self.directory/"summary.md"
        with contextlib.redirect_stdout(io.StringIO()):
            code=summary.main([str(self.path),"--output",str(output),"--markdown",str(md),
                               "--world-start-mach-ns",str(BASE),"--phase","early:0:3"])
        self.assertEqual(code,0)
        self.assertIn("not hardware occupancy",md.read_text())
        report=json.loads(output.read_text());self.assertEqual(report["quiet_world"]["start_source"],"explicit_override")
        original=self.path.read_bytes()
        with self.assertRaises(ValueError):summary.safe_write(self.path,"bad",self.path)
        alias=self.directory/"hardlink";os.link(self.path,alias)
        with self.assertRaises(ValueError):summary.safe_write(alias,"bad",self.path)
        with self.assertRaises(FileExistsError):summary.safe_write(output,"bad",self.path)
        original_output=output.read_bytes()
        self.assertEqual(json.loads(original_output)["provenance"]["source_sha"],"b"*40)
        dangling=self.directory/"dangling-output"
        dangling.symlink_to(self.directory/"absent-target")
        with self.assertRaises(FileExistsError):summary.safe_write(dangling,"bad",self.path)
        self.assertTrue(dangling.is_symlink())
        self.assertEqual(output.read_bytes(),original_output)
        self.assertEqual(self.path.read_bytes(),original)

    def test_invalid_header_missing_gpu_values_and_malformed_tail_fail_closed(self):
        stream=Stream();stream.marker("quiet_world_started",0)
        stream.rows[0]["cpu_clock"]="unverified_clock"
        stream.command(1,1,2);stream.rows[-1].update(gpu_time_valid=False,gpu_start_seconds=None,gpu_end_seconds=None)
        stream.write(self.path)
        with self.path.open("ab")as f:f.write(b'{"unfinished":')
        result=summary.Analyzer(self.path).run()
        self.assertEqual(result["classification"]["status"],"invalid")
        self.assertEqual(result["parse_errors"]["malformed_json"],1)
        self.assertEqual(result["commands"]["completed_without_valid_gpu_interval"],1)

    @unittest.skipUnless(os.environ.get("AL_QUIET_DIAGNOSTIC_SERIALIZED_FIXTURE"),"explicit retained native fixture path enables source-format integration")
    def test_actual_retained_native_serializer_stream(self):
        path=Path(os.environ["AL_QUIET_DIAGNOSTIC_SERIALIZED_FIXTURE"])
        original=path.read_bytes();header=json.loads(original.splitlines()[0]);footer=json.loads(original.splitlines()[-1])
        result=summary.Analyzer(path,world_start=header["calibration_after_mach_ns"]).run()
        self.assertEqual(path.read_bytes(),original)
        self.assertEqual(result["classification"]["status"],"partial")
        self.assertIn("frame_telemetry_unavailable",result["classification"]["issues"])
        self.assertEqual(result["commands"]["completed"],footer["commands_completed"])
        self.assertEqual(result["wait_pairing"]["paired_waits"],96)
        self.assertEqual(result["sequences"]["missing_against_footer_attempted_or_observed_max"],0)
        phase=result["phases"][0]
        self.assertEqual(phase["gpu_commands_overlapping_phase"],96)
        self.assertLessEqual(phase["gpu_command_envelope_union"]["union_wall_ns"],phase["gpu_envelope_ns_sum_not_frame_cost"]["sum"])


if __name__=="__main__":unittest.main()
