"""PPD-318 collector regressions: no viewer/profiler is launched by these tests."""
import argparse
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import plistlib
import signal
import struct
import sys
import tempfile
import time
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("quiet_capture", TOOLS / "quiet_capture.py")
capture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(capture)


class Clock:
    def __init__(self):
        self.now = 0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Recorder:
    def __init__(self, clock, end=3, code=0):
        self.pid = 987654
        self.clock, self.end, self.code = clock, end, code
        self.returncode = None
        self.signals = []

    def poll(self):
        if self.clock.now >= self.end:
            self.returncode = self.code
        return self.returncode

    def wait(self, timeout):
        if self.returncode is None:
            raise capture.subprocess.TimeoutExpired("fake recorder", timeout)
        return self.returncode

    def send_signal(self, value):
        self.signals.append(value)
        self.end = self.clock.now + .2


class CollectorTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.parent = Path(temporary.name)
        self.args = capture.parser().parse_args([
            "record", "--pid", "12345", "--executable", "/exact/viewer",
            "--exe-sha256", "a" * 64, "--source-sha", "b" * 40,
            "--output", str(self.parent / "run"), "--seconds", "3",
            "--start-timeout", "2", "--finalize-timeout", "2"])
        self.clock = Clock()
        self.recorder = Recorder(self.clock)
        self.notification = mock.Mock(name="Darwin notification")
        self.notification.name = "unique-registered-before-spawn"
        self.notification.received.side_effect = lambda: self.clock.now >= .2
        self.identity = {"pid": 12345, "executable": "/exact/viewer", "process_start": "fixed"}
        self.popen_commands = []
        self.message = b"Starting recording...\nCtrl-C to stop the recording\n"

    def run_collector(self, sizes=None, qualification=None):
        def spawn(argv, **kwargs):
            self.assertTrue(kwargs["start_new_session"])
            self.assertTrue(notifier_factory.called)  # Registration must precede spawn.
            self.popen_commands.append(argv)
            kwargs["stdout"].write(self.message)
            kwargs["stdout"].flush()
            trace = self.args.output / "overview.trace"
            trace.mkdir()
            (trace / "recorded-data").write_bytes(b"data")
            return self.recorder

        def kill_group(pid, value):
            self.assertEqual(self.recorder.pid, pid)
            self.recorder.returncode = -value

        observed = sizes or (lambda *_: (100, [self.recorder.pid, 987655]))
        with contextlib.ExitStack() as stack:
            stack.enter_context(mock.patch.object(capture, "preflight", return_value={
                "xctrace": "/exact/xctrace", "template": {"path": "/exact/Overview", "retention": [{"windowLimit": 0}]}}))
            stack.enter_context(mock.patch.object(capture, "process_identity", return_value=self.identity))
            notifier_factory = stack.enter_context(mock.patch.object(capture, "StartNotification", return_value=self.notification))
            stack.enter_context(mock.patch.object(capture.subprocess, "Popen", side_effect=spawn))
            stack.enter_context(mock.patch.object(capture, "raw_size", side_effect=observed))
            stack.enter_context(mock.patch.object(capture.os, "killpg", side_effect=kill_group))
            stack.enter_context(mock.patch.object(capture.time, "monotonic", side_effect=self.clock.monotonic))
            stack.enter_context(mock.patch.object(capture.time, "sleep", side_effect=self.clock.sleep))
            qualify = stack.enter_context(mock.patch.object(capture, "qualify", side_effect=qualification,
                return_value={"status": "actual ranges verified", "target_clock_bounds": {
                    "first_unix_ns": 200, "last_end_unix_ns": 3_000_000_200}}))
            output = stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
            code = capture.Collector(self.args).run()
            notifier_factory.assert_called_once()
        return code, json.loads((self.args.output / "session.json").read_text()), output.getvalue(), qualify

    def test_success_drives_start_event_without_duplicate_name_crash_and_keeps_parent_exit(self):
        code, receipt, output, qualified = self.run_collector()
        self.assertEqual(0, code)
        self.assertEqual("complete", receipt["status"])
        self.assertEqual(0, receipt["recorder_exit"])
        self.assertIn('"event": "record_start_requested"', output)
        self.assertIn('"event": "owner_cue_ready"', output)
        self.assertIn("Darwin", receipt["readiness_proof"])
        self.assertFalse(receipt["viewer_launched"])
        self.assertNotIn("--launch", self.popen_commands[0])
        self.assertEqual("unique-registered-before-spawn", self.popen_commands[0][-1])
        qualified.assert_called_once()
        self.notification.close.assert_called_once()

    def test_console_start_banner_is_insufficient_without_notification(self):
        self.notification.received.side_effect = None
        self.notification.received.return_value = False
        code, receipt, output, qualified = self.run_collector()
        self.assertEqual(1, code)
        self.assertIn("readiness deadline", receipt["failure"])
        self.assertNotIn('"event": "owner_cue_ready"', output)
        qualified.assert_not_called()
        self.assertEqual(0, receipt["recorder_exit"])
        self.assertIn(signal.SIGINT, self.recorder.signals)
        self.assertTrue((self.args.output / "overview.trace/recorded-data").exists())

    def test_notification_without_raw_data_does_not_cue_owner(self):
        code, receipt, output, _ = self.run_collector(sizes=lambda *_: (0, [self.recorder.pid]))
        self.assertEqual(1, code)
        self.assertIsNone(receipt["readiness"])
        self.assertNotIn('"event": "owner_cue_ready"', output)

    def test_final_size_is_checked_even_after_successful_fast_exit(self):
        self.recorder.end = 1.5
        cap = self.args.raw_cap_mib * capture.MIB
        sizes = lambda *_: (cap + 1 if self.recorder.poll() is not None else 100, [self.recorder.pid])
        code, receipt, _, qualified = self.run_collector(sizes=sizes)
        self.assertEqual(1, code)
        self.assertEqual(0, receipt["recorder_exit"])
        self.assertEqual(cap + 1, receipt["max_observed_raw_bytes"])
        self.assertIn("raw size limit", receipt["failure"])
        qualified.assert_not_called()

    def test_recorder_failure_is_not_hidden_by_saved_output_or_data(self):
        self.recorder.code = 7
        self.message += b"Output file saved as: overview.trace\n"
        code, receipt, _, qualified = self.run_collector()
        self.assertEqual(1, code)
        self.assertEqual(7, receipt["recorder_exit"])
        self.assertIn("recorder failed", receipt["failure"])
        qualified.assert_not_called()

    def test_finalization_deadline_terminates_only_owned_group_and_preserves_raw(self):
        self.recorder.end = 100
        self.message += b"Recording completed. Saving output file...\n"
        code, receipt, _, qualified = self.run_collector()
        self.assertEqual(1, code)
        self.assertIn("finalization deadline", receipt["failure"])
        self.assertLess(self.clock.now, 3)
        self.assertEqual(-signal.SIGTERM, receipt["recorder_exit"])
        self.assertIn(signal.SIGINT, self.recorder.signals)
        self.assertTrue(receipt["raw_preserved"])
        qualified.assert_not_called()

    def test_export_failure_retains_recorder_status_and_failed_receipt(self):
        code, receipt, _, _ = self.run_collector(qualification=RuntimeError("bounded export failed"))
        self.assertEqual(1, code)
        self.assertEqual(0, receipt["recorder_exit"])
        self.assertIn("bounded export failed", receipt["failure"])

    def test_optional_thread_states_adds_only_scheduler_instrument(self):
        self.args.thread_states = True
        code, _, _, qualified = self.run_collector()
        self.assertEqual(0, code)
        command = self.popen_commands[0]
        self.assertEqual(1, command.count("--instrument"))
        self.assertEqual("Thread State Trace", command[command.index("--instrument") + 1])
        self.assertNotIn("Metal Application", command)
        self.assertEqual(3, qualified.call_args.args[1])

    def test_cleanup_failure_still_writes_final_receipt(self):
        self.notification.close.side_effect = OSError("notification cleanup")
        code, receipt, _, _ = self.run_collector()
        self.assertEqual(1, code)
        self.assertEqual(0, receipt["recorder_exit"])
        self.assertIn("notification cleanup failed", receipt["failure"])

    def test_broken_raw_size_observer_does_not_strand_recorder(self):
        self.recorder.end = 100
        def broken(*_):
            raise RuntimeError("raw observer failed")
        code, receipt, _, _ = self.run_collector(sizes=broken)
        self.assertEqual(1, code)
        self.assertEqual(-signal.SIGTERM, receipt["recorder_exit"])
        self.assertIn("raw observer failed", receipt["failure"])

    def test_unresponsive_recorder_escalation_is_finite_and_keeps_failed_exit(self):
        self.recorder.end = 100
        self.recorder.send_signal = mock.Mock()  # Ignores graceful stop.
        self.notification.received.side_effect = None
        self.notification.received.return_value = False
        code, receipt, _, _ = self.run_collector()
        self.assertEqual(1, code)
        self.assertEqual(-signal.SIGTERM, receipt["recorder_exit"])
        self.assertLess(self.clock.now, 5)
        self.recorder.send_signal.assert_called_once_with(signal.SIGINT)

    def test_native_input_is_observed_without_copy_or_mutation(self):
        source = self.parent / "native.jsonl"
        data = {"event": "header", "schema": 1, "cpu_clock": "mach_absolute_time_ns",
                "calibration_before_mach_ns": 100, "calibration_unix_ns": 200,
                "calibration_after_mach_ns": 101, "source_sha": self.args.source_sha, "process_id": 12345}
        source.write_text(json.dumps(data) + "\n")
        with source.open("a") as stream:
            stream.write(json.dumps({"event": "frame", "start_mach_ns": 100,
                                     "end_mach_ns": 3_000_000_100, "mach_ns": 3_000_000_100}) + "\n")
        original = source.read_bytes()
        self.args.diagnostic_jsonl = source
        code, receipt, _, _ = self.run_collector()
        self.assertEqual(0, code)
        self.assertEqual(original, source.read_bytes())
        self.assertFalse((self.args.output / "diagnostic-copy.jsonl").exists())
        self.assertEqual(capture.sha256(source), receipt["diagnostic_coverage"]["prefix_sha256"])

    def test_requested_native_input_missing_fails_without_erasing_overview_status(self):
        self.args.diagnostic_jsonl = self.parent / "not-created.jsonl"
        code, receipt, _, _ = self.run_collector()
        self.assertEqual(1, code)
        self.assertEqual(0, receipt["recorder_exit"])
        self.assertIn("missing_uncalibrated_or_source_mismatch", receipt["failure"])
        self.assertIn("coverage", receipt)

    def native_size_fixture(self):
        source = self.parent / "native-sized.jsonl"
        rows = [{"event": "header", "schema": 1, "cpu_clock": "mach_absolute_time_ns",
                 "calibration_before_mach_ns": 100, "calibration_unix_ns": 200,
                 "calibration_after_mach_ns": 101, "source_sha": self.args.source_sha, "process_id": 12345},
                {"event": "frame", "start_mach_ns": 100, "end_mach_ns": 3_000_000_100,
                 "mach_ns": 3_000_000_100},
                {"event": "metrics", "mach_ns": 101, "padding": "x" * 600}]
        source.write_text("".join(json.dumps(row) + "\n" for row in rows))
        self.args.diagnostic_jsonl = source
        return source

    def test_explicit_native_cap_reaches_live_guard_and_final_read_only_coverage(self):
        source = self.native_size_fixture()
        self.args.diagnostic_cap_mib = 256
        original = source.read_bytes()
        # Scale only byte units; exercise the real live/final size guards and
        # parser without allocating a256MiB test input or starting a profiler.
        with mock.patch.object(capture, "MIB", 8):
            self.assertGreater(len(original), 64 * capture.MIB)
            self.assertLess(len(original), 256 * capture.MIB)
            code, receipt, _, _ = self.run_collector()
        self.assertEqual(0, code)
        self.assertEqual(256, receipt["limits"]["diagnostic_cap_mib"])
        self.assertEqual(256 * 8, receipt["diagnostic_coverage"]["maximum_input_bytes"])
        self.assertTrue(receipt["native_coverage"]["covered"])
        self.assertEqual(original, source.read_bytes())

    def test_default_native_cap_still_rejects_oversized_input_and_preserves_it(self):
        source = self.native_size_fixture()
        original = source.read_bytes()
        with mock.patch.object(capture, "MIB", 8):
            code, receipt, _, _ = self.run_collector()
        self.assertEqual(1, code)
        self.assertIn("native diagnostic input exceeds", receipt["failure"])
        self.assertEqual(64, receipt["limits"]["diagnostic_cap_mib"])
        self.assertEqual(original, source.read_bytes())

    def test_existing_run_is_never_overwritten(self):
        self.args.output.mkdir()
        sentinel = self.args.output / "keep"
        sentinel.write_text("private evidence")
        with self.assertRaises(FileExistsError):
            capture.Collector(self.args)
        self.assertEqual("private evidence", sentinel.read_text())


class DataTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def test_template_requires_explicit_zero_retention_and_preserves_hash(self):
        path = self.directory / "template"
        for objects in ([], [{"windowLimit": 100}], [{"windowLimit": 0}, {"windowLimit": 1}]):
            path.write_bytes(plistlib.dumps({"$objects": objects}))
            with self.assertRaises(RuntimeError):
                capture.template_receipt(path)
        path.write_bytes(plistlib.dumps({"$objects": [{"windowLimit": 0, "recordingMode": 2}, capture.OVERVIEW_ID]}))
        receipt = capture.template_receipt(path)
        self.assertEqual(capture.sha256(path), receipt["sha256"])
        self.assertEqual(0, receipt["retention"][0]["windowLimit"])
        path.write_bytes(plistlib.dumps({"$objects": [{"windowLimit": 0}, capture.OVERVIEW_ID,
                                                    "com.apple.xray.instrument-type.metal-application"]}))
        with self.assertRaisesRegex(RuntimeError, "only the qualified Overview"):
            capture.template_receipt(path)

    def test_darwin_notification_requires_matching_network_order_token(self):
        read_fd, write_fd = os.pipe()
        self.addCleanup(os.close, read_fd)
        self.addCleanup(os.close, write_fd)
        notification = capture.StartNotification.__new__(capture.StartNotification)
        notification.fd, notification.token, notification.pending = read_fd, 7312, b""
        self.assertFalse(notification.received())
        os.write(write_fd, struct.pack("!I", 123))
        self.assertFalse(notification.received())
        os.write(write_fd, struct.pack("!I", 7312)[:2])
        self.assertFalse(notification.received())
        os.write(write_fd, struct.pack("!I", 7312)[2:])
        self.assertTrue(notification.received())

    @unittest.skipUnless(sys.platform == "darwin", "Darwin libnotify only; no profiler involved")
    def test_real_darwin_notification_delivery_and_cleanup(self):
        notification = capture.StartNotification()
        library = notification.library
        library.notify_post.argtypes = (capture.ctypes.c_char_p,)
        library.notify_post.restype = capture.ctypes.c_uint32
        try:
            self.assertFalse(notification.received())
            self.assertEqual(0, library.notify_post(notification.name.encode()))
            deadline = time.monotonic() + 1
            received = False
            while not received and time.monotonic() < deadline:
                received = notification.received()
                time.sleep(.001)
            self.assertTrue(received)
        finally:
            notification.close()
        with self.assertRaises(OSError):
            os.fstat(notification.fd)  # notify_cancel closed it; no second close.

    def test_tail_retains_exact_bytes_rotation_offsets_and_source_untouched(self):
        source, output = self.directory / "input", self.directory / "copy"
        source.write_bytes(b"header\n")
        tail = capture.Tail(source, output, 100)
        tail.poll()
        with source.open("ab") as stream:
            stream.write(b"event\n")
        tail.poll()
        source.rename(self.directory / "old")
        source.write_bytes(b"next\n")
        tail.poll()
        self.assertEqual(b"header\nevent\nnext\n", output.read_bytes())
        self.assertEqual(b"next\n", source.read_bytes())
        self.assertEqual(2, len(tail.receipt()["generations"]))
        self.assertEqual(13, tail.receipt()["generations"][1]["destination_offset"])

    def test_tail_rejects_oversize_without_unbounded_read(self):
        source, output = self.directory / "input", self.directory / "copy"
        source.write_bytes(b"0123456789")
        tail = capture.Tail(source, output, 5)
        with self.assertRaisesRegex(RuntimeError, "size limit"):
            tail.poll()
        self.assertEqual(b"0123456789", source.read_bytes())
        self.assertEqual(0, tail.total)

    def test_size_deduplicates_trace_and_open_raw_aliases(self):
        trace = self.directory / "trace"
        trace.mkdir()
        raw = trace / "raw.ktrace"
        raw.write_bytes(b"0123456789")
        alias = self.directory / "raw.ktrace"
        os.link(raw, alias)
        self.assertEqual(10, capture.tree_size([trace, raw, alias]))

    def test_actual_target_xml_ranges_resolve_process_references(self):
        path = self.directory / "coverage.xml"
        path.write_text('''<trace-query-result><schema>
          <col><mnemonic>time</mnemonic></col><col><mnemonic>process</mnemonic></col>
          <col><mnemonic>duration</mnemonic></col></schema>
          <row><time id="1">1000000000</time><process id="2"><pid>8</pid></process><duration>2</duration></row>
          <row><time>2000000000</time><process ref="2"/><duration>100000000</duration></row>
          <row><time>10000000000</time><process><pid>99</pid></process><duration>5</duration></row>
          </trace-query-result>''')
        result = capture.table_coverage(path, 8)
        self.assertEqual(2, result["target_rows"])
        self.assertEqual(1000000000, result["first_ns"])
        self.assertEqual(2100000000, result["last_end_ns"])
        self.assertAlmostEqual(1.1, result["span_seconds"])
        self.assertEqual(1, result["maximum_consecutive_start_gap_seconds"])

    def test_diagnostic_summary_does_not_treat_partial_lines_as_complete_events(self):
        path = self.directory / "diagnostic"
        path.write_bytes(b'{"event":"header","calibration":{"utc":"private","mach_ns":100}}\n'
                         b'{"event":"command_complete","mach_ns":200}\n'
                         b'broken\n{"event":"frame_end"')
        result = capture.diagnostic_coverage(path)
        self.assertEqual(2, result["records"])
        self.assertEqual(1, result["malformed"])
        self.assertTrue(result["partial_final_line"])
        self.assertEqual(200, result["first_mach_ns"])
        self.assertIn("header", result["events"])
        self.assertEqual(capture.sha256(path), result["prefix_sha256"])
        self.assertTrue(result["read_only"])
        self.assertEqual(path.stat().st_size, result["observed_bytes"])

    def test_export_fast_final_oversize_is_rejected_and_exit_recorded(self):
        output = self.directory / "export.xml"
        process = mock.Mock(pid=987654, returncode=0)
        process.poll.return_value = 0
        def spawn(*_args, **_kwargs):
            output.write_bytes(b"larger than five")
            return process
        with mock.patch.object(capture.subprocess, "Popen", side_effect=spawn):
            with self.assertRaisesRegex(RuntimeError, "export size limit"):
                capture.bounded_export(self.directory / "trace", output, ["--toc"], 1, 5)
        receipt = json.loads(output.with_suffix(".receipt.json").read_text())
        self.assertEqual(0, receipt["exit"])
        self.assertEqual("failed", receipt["status"])
        self.assertEqual(16, receipt["final_bytes"])
        self.assertTrue(output.exists())

    def test_powermetrics_requested_without_privilege_fails_before_recording(self):
        args = argparse.Namespace(template=None, powermetrics=True, thread_states=False)
        with mock.patch.object(capture.sys, "platform", "darwin"), \
             mock.patch.object(capture.os, "uname", return_value=argparse.Namespace(machine="arm64")), \
             mock.patch.object(capture, "template_receipt", return_value={}), \
             mock.patch.object(capture, "checked", return_value="cpu_power gpu_power thermal --sample-count --format"), \
             mock.patch.object(capture.os, "geteuid", return_value=501), \
             mock.patch.object(capture.subprocess, "Popen") as spawn:
            with self.assertRaisesRegex(RuntimeError, "no sudo attempted"):
                capture.preflight(args)
            spawn.assert_not_called()

    def test_cli_rejects_unbounded_or_relative_requests_before_start(self):
        with mock.patch.object(capture, "Collector") as collector, contextlib.redirect_stderr(io.StringIO()):
            code = capture.main(["record", "--pid", "12345", "--executable", "/exact/viewer",
                                 "--exe-sha256", "a" * 64, "--source-sha", "b" * 40,
                                 "--output", "relative", "--seconds", "999999"])
        self.assertEqual(1, code)
        collector.assert_not_called()

    def test_cli_native_cap_is_explicit_and_bounded_before_start(self):
        command = ["record", "--pid", "12345", "--executable", "/exact/viewer",
                   "--exe-sha256", "a" * 64, "--source-sha", "b" * 40,
                   "--output", "/absolute/new-run"]
        self.assertEqual(64, capture.parser().parse_args(command).diagnostic_cap_mib)
        for cap in (0, 63, 257, 999999):
            with self.subTest(cap=cap), mock.patch.object(capture, "Collector") as collector, \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(1, capture.main(command + ["--diagnostic-cap-mib", str(cap)]))
                collector.assert_not_called()
        with mock.patch.object(capture, "Collector") as collector:
            collector.return_value.run.return_value = 0
            self.assertEqual(0, capture.main(command + ["--diagnostic-cap-mib", "256"]))
            self.assertEqual(256, collector.call_args.args[0].diagnostic_cap_mib)

    def test_qualifier_rejects_middle_gap_despite_matching_first_last_range(self):
        args = argparse.Namespace(output=self.directory, pid=8, thread_states=False,
                                  export_timeout=1, export_cap_mib=1)
        def export(_trace, output, _selector, _deadline, _cap):
            if output.name == "toc.xml":
                output.write_text('<trace-toc><run number="1"><info><target><process pid="8"/>'
                                  '</target><summary><duration>20</duration></summary></info><data>' +
                                  ''.join('<table schema="' + name + '"/>' for name in capture.SCHEMAS) +
                                  '</data></run></trace-toc>')
            else:
                output.write_text('<trace-query-result><schema><col><mnemonic>time</mnemonic></col>'
                                  '<col><mnemonic>process</mnemonic></col></schema>'
                                  '<row><time>0</time><process id="p"><pid>8</pid></process></row>'
                                  '<row><time>20000000000</time><process ref="p"/></row></trace-query-result>')
            return {"exit": 0}
        with mock.patch.object(capture, "bounded_export", side_effect=export):
            with self.assertRaisesRegex(RuntimeError, "timestamp gap"):
                capture.qualify(args, 20)
        receipt = json.loads((self.directory / "coverage.json").read_text())
        self.assertEqual("failed", receipt["status"])
        self.assertEqual(20, receipt["ranges"]["time-profile"]["span_seconds"])

    def test_continuous_thread_state_request_is_rejected_before_spawn(self):
        with mock.patch.object(capture, "Collector") as collector, contextlib.redirect_stderr(io.StringIO()):
            code = capture.main(["record", "--pid", "12345", "--executable", "/exact/viewer",
                                 "--exe-sha256", "a" * 64, "--source-sha", "b" * 40,
                                 "--output", "/absolute/new-run", "--seconds", "360", "--thread-states"])
        self.assertEqual(1, code)
        collector.assert_not_called()


class CoverageContractTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.args = argparse.Namespace(output=self.directory, pid=8, thread_states=False,
                                       export_timeout=1, export_cap_mib=1)
        self.source_sha = "b" * 40
        self.origin = 1_700_000_000_000_000_000

    def native(self, seconds, footer=None, partial=False, command_tail_seconds=0):
        path = self.directory / "native.jsonl"
        rows = [{"event": "header", "schema": 1, "source_sha": self.source_sha,
                 "process_id": 8,
                 "cpu_clock": "mach_absolute_time_ns", "calibration_before_mach_ns": 100_000_000_000,
                 "calibration_unix_ns": self.origin, "calibration_after_mach_ns": 100_000_000_000}]
        for second in range(seconds):
            rows.append({"event": "frame", "start_mach_ns": (100 + second) * 1_000_000_000,
                         "end_mach_ns": (101 + second) * 1_000_000_000,
                         "mach_ns": (101 + second) * 1_000_000_000})
        if command_tail_seconds:
            rows.append({"event": "command_complete", "mach_ns": (100 + command_tail_seconds) * 1_000_000_000})
        if footer:
            rows.append({"event": "footer", "stop_reason": footer, "dropped_full": 0,
                         "dropped_contention": 0, "dropped_budget": 0, "io_errors": 0})
        path.write_text("".join(json.dumps(row) + "\n" for row in rows) + ('{"event":' if partial else ''))
        summary = capture.diagnostic_coverage(path)
        overview = {"target_clock_bounds": {"first_unix_ns": self.origin,
                                             "last_end_unix_ns": self.origin + 20_000_000_000}}
        return summary, capture.qualify_native(summary, overview, self.source_sha, 8)

    def test_legacy_native_stream_without_pid_is_not_process_bound(self):
        summary, _ = self.native(20)
        summary["header"].pop("process_id")
        overview = {"target_clock_bounds": {"first_unix_ns": self.origin,
                                             "last_end_unix_ns": self.origin + 20_000_000_000}}
        result = capture.qualify_native(summary, overview, self.source_sha, 8)
        self.assertFalse(result["covered"])
        self.assertEqual("native_binding_unproved", result["native_coverage_status"])

    def test_other_native_pid_does_not_bind_same_source_and_clock(self):
        summary, _ = self.native(20)
        summary["header"]["process_id"] = 99
        overview = {"target_clock_bounds": {"first_unix_ns": self.origin,
                                             "last_end_unix_ns": self.origin + 20_000_000_000}}
        result = capture.qualify_native(summary, overview, self.source_sha, 8)
        self.assertFalse(result["covered"])
        self.assertEqual("native_pid_mismatch", result["native_coverage_status"])

    def test_duration_limit_zero_drops_before_overview_end_is_incomplete(self):
        summary, result = self.native(10, footer="duration_limit", command_tail_seconds=25)
        self.assertFalse(summary["loss_observed"])
        self.assertFalse(result["covered"])
        self.assertEqual("duration_limit", result["stop_reason"])
        self.assertEqual("incomplete_frame_iteration_coverage", result["native_coverage_status"])
        self.assertEqual(10_000_000_000, result["end_shortfall_ns"])
        # A later command completion does not manufacture missing FRAME time.
        self.assertEqual(self.origin + 10_000_000_000, result["last_frame_end_unix_ns"])

    def test_live_prefix_covers_full_span_without_requiring_footer(self):
        _, result = self.native(20)
        self.assertTrue(result["covered"])
        self.assertEqual("covered_live_prefix", result["native_coverage_status"])
        self.assertFalse(result["footer_observed"])
        self.assertIsNone(result["stop_reason"])

    def test_live_partial_final_line_is_reported_without_erasing_complete_frame_coverage(self):
        _, result = self.native(20, partial=True)
        self.assertTrue(result["covered"])
        self.assertTrue(result["partial_final_line"])
        self.assertIn("no final loss", result["uncertainty"])

    def test_footer_duration_limit_after_required_span_is_explicitly_covered(self):
        _, result = self.native(21, footer="duration_limit")
        self.assertTrue(result["covered"])
        self.assertEqual("covered_footer_duration_limit", result["native_coverage_status"])

    def export_fixture(self, families, gpu_pid=8, partial_family=None, zero_frames=False,
                       bucket_rows=None):
        def exported(_trace, output, _selector, _deadline, _cap):
            if output.name == "toc.xml":
                output.write_text('<trace-toc><run number="1"><info><target><process pid="8"/>'
                    '</target><summary><start-date>2026-11-16T16:00:00.000Z</start-date>'
                    '<duration>10</duration></summary></info><data>' +
                    ''.join('<table schema="' + name + '"/>' for name in capture.SCHEMAS) +
                    '</data></run></trace-toc>')
            else:
                names = ("time-profile",) if output.name == "time-profile.xml" else families
                rows = []
                for family in names:
                    count = 4 if family == partial_family else 10
                    buckets = ([(second * 1_000_000_000, 1_000_000_000,
                                 "0" if zero_frames else "1", "3.5") for second in range(count)]
                               if bucket_rows is None or family == "time-profile" else bucket_rows)
                    for start, duration, frames, metric in buckets:
                        owner = 8 if family in ("time-profile", "CPU Begin to Display") else gpu_pid
                        rows.append('<row><time>' + str(start) + '</time>'
                            '<duration>' + str(duration) + '</duration><name>' + family + '</name>'
                            '<frames>' + str(frames) + '</frames><value>' + str(metric) + '</value>'
                            '<process><pid>' + str(owner) + '</pid></process></row>')
                output.write_text('<trace-query-result><schema>' + ''.join('<col><mnemonic>' + name +
                    '</mnemonic></col>' for name in ("start", "duration", "name", "number-of-frames", "value-in-ms", "process")) +
                    '</schema>' + ''.join(rows) + '</trace-query-result>')
            return {"exit": 0}
        return mock.patch.object(capture, "bounded_export", side_effect=exported)

    def test_cpu_display_rows_cannot_manufacture_gpu_family_coverage(self):
        with self.export_fixture(("CPU Begin to Display",)):
            with self.assertRaisesRegex(RuntimeError, "GPU family coverage"):
                capture.qualify(self.args, 10)

    def test_both_positive_frame_gpu_families_qualify_and_define_clock_bounds(self):
        with self.export_fixture(capture.GPU_FAMILIES):
            result = capture.qualify(self.args, 10)
        self.assertEqual("verified_target_timestamp_ranges", result["status"])
        self.assertEqual(["time-profile", *capture.GPU_FAMILIES], result["target_clock_bounds"]["sources"])
        self.assertEqual(10_000_000_000, result["target_clock_bounds"]["last_end_offset_ns"])

    def test_other_pid_gpu_rows_do_not_qualify_target_cpu_display_rows(self):
        with self.export_fixture(("CPU Begin to Display", *capture.GPU_FAMILIES), gpu_pid=99):
            with self.assertRaisesRegex(RuntimeError, "GPU family coverage"):
                capture.qualify(self.args, 10)

    def test_short_gpu_family_cannot_borrow_other_family_full_span(self):
        with self.export_fixture(capture.GPU_FAMILIES, partial_family="GPU Active Time"):
            with self.assertRaisesRegex(RuntimeError, "GPU Active Time"):
                capture.qualify(self.args, 10)

    def test_zero_frame_gpu_buckets_do_not_qualify(self):
        zeros = [(second * 1_000_000_000, 1_000_000_000, "0", "0") for second in range(10)]
        with self.export_fixture(capture.GPU_FAMILIES, bucket_rows=zeros):
            with self.assertRaisesRegex(RuntimeError, "GPU family coverage"):
                capture.qualify(self.args, 10)

    def actual_gap_buckets(self):
        # Retained l9 PID62863 mean-variant bucket values around the original
        # false5.016658s gap, translated by the first positive start. No export.
        origin = 68_289_391_583
        return [(start - origin, duration, frames, value) for start, duration, frames, value in (
            (68_289_391_583, 1_002_749_333, 11, "116.154428148"),
            (69_292_140_958, 1_002_569_417, 0, "0.000000000"),
            (70_348_624_083, 948_794_083, 0, "0.000000000"),
            (71_358_381_583, 941_668_250, 0, "0.000000000"),
            (72_308_519_291, 994_220_417, 0, "0.000000000"),
            (73_306_049_583, 999_297_708, 8, "125.510348938"),
            (74_305_347_333, 1_002_828_750, 16, "119.883648352"))]

    def test_retained_four_zero_buckets_bridge_actual_positive_frame_gap(self):
        with self.export_fixture(capture.GPU_FAMILIES, bucket_rows=self.actual_gap_buckets()):
            result = capture.qualify(self.args, 10)
        self.assertEqual("verified_target_timestamp_ranges", result["status"])
        families = result["ranges"][capture.SCHEMAS[1]]["gpu_families"]
        for family in capture.GPU_FAMILIES:
            rows = families[family]
            self.assertEqual((7, 3, 4, 0), tuple(rows[k] for k in
                ("target_rows", "positive_frame_rows", "zero_frame_rows", "invalid_rows")))
            self.assertAlmostEqual(5.016658, rows["maximum_positive_frame_start_gap_seconds"])
            self.assertLess(rows["maximum_consecutive_start_gap_seconds"], 1.057)

    def test_missing_buckets_cannot_borrow_cpu_or_other_family_timestamps(self):
        missing = [row for row in self.actual_gap_buckets() if row[2] > 0]
        with self.export_fixture(capture.GPU_FAMILIES, bucket_rows=missing):
            with self.assertRaisesRegex(RuntimeError, "GPU family coverage|timestamp gap"):
                capture.qualify(self.args, 10)

    def test_malformed_gpu_buckets_do_not_bridge_or_hide_behind_valid_rows(self):
        invalid = [("0", "NaN"), ("-1", "0"), ("bad", "0"), ("0", "3.5")]
        for frames, metric in invalid:
            with self.subTest(frames=frames, metric=metric):
                rows = self.actual_gap_buckets()
                start, duration, _, _ = rows[2]
                rows[2] = (start, duration, frames, metric)
                with self.export_fixture(capture.GPU_FAMILIES, bucket_rows=rows):
                    with self.assertRaisesRegex(RuntimeError, "malformed actual target GPU family"):
                        capture.qualify(self.args, 10)
        for start, duration in (("bad", 1_000_000_000), (-1, 1_000_000_000), (2_000_000_000, 0)):
            rows = self.actual_gap_buckets()
            rows[2] = (start, duration, "0", "0")
            with self.subTest(start=start, duration=duration), \
                    self.export_fixture(capture.GPU_FAMILIES, bucket_rows=rows):
                with self.assertRaisesRegex(RuntimeError, "malformed actual target GPU family"):
                    capture.qualify(self.args, 10)

    def test_zero_buckets_of_other_pid_cannot_bridge_target_family(self):
        with self.export_fixture(("CPU Begin to Display", *capture.GPU_FAMILIES),
                                 gpu_pid=99, bucket_rows=self.actual_gap_buckets()):
            with self.assertRaisesRegex(RuntimeError, "GPU family coverage"):
                capture.qualify(self.args, 10)


if __name__ == "__main__":
    unittest.main()
