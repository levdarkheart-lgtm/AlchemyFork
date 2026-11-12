"""Contract tests for the maintained defect register; all records are synthetic."""
import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
DOCUMENT = "docs/metal/POST_PORT_DEFECTS.md"


def record(number=1, **overrides):
    fields = {
        "Status": "open", "Confirmation": "reported", "Owner": "Integrator",
        "Discovered": "2026-11-12", "Original reference": "unknown",
        "Evidence": "[Observation](work/active/observation.md)",
        "Next action": "Identify the first rejected producer.",
        "Verification": "not_run", "Owner verification": "pending",
        "Push approval": "not_granted",
    }
    fields.update(overrides)
    return f"## PPD-{number:03d} — Synthetic observation\n\n" + "\n".join(
        f"- {key}: {value}" for key, value in fields.items()) + "\n"


def document(*records, next_id=None):
    if not records:
        records = (record(),)
    if next_id is None:
        next_id = len(records) + 1
    return f"# Post-port defects\n\n<!-- next-defect-id: PPD-{next_id:03d} -->\n\n" + "\n".join(records)


class PostPortDefectTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location("defect_harness", TOOLS / "validate_harness.py")
        self.harness = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.harness)
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.path = self.root / DOCUMENT
        self.path.parent.mkdir(parents=True)
        evidence = self.path.parent / "work/active/observation.md"
        evidence.parent.mkdir(parents=True)
        evidence.write_text("# Observation\nSynthetic tool-test evidence only.\n")

    def errors(self, text, baseline_text="", release=False):
        self.path.write_text(text)
        # Exactly the schema and shared live-reference passes used by validate().
        return self.harness.validate_post_port_defects(
            self.root, release=release, baseline_text=baseline_text) + \
            self.harness.validate_live_references(self.root, [DOCUMENT])

    def verified(self, number=1):
        return record(number, **{"Status": "verified", "Confirmation": "reproduced",
                                 "Verification": "Focused regression and owner retest passed.",
                                 "Owner verification": "passed"})

    def test_register_is_mandatory_live_and_called_by_full_validator(self):
        self.assertIn(DOCUMENT, self.harness.MANDATORY)
        self.assertIn(DOCUMENT, self.harness.LIVE_DOCUMENTS)
        self.assertTrue(any("Missing post-port" in error for error in
                            self.harness.validate_post_port_defects(self.root)))
        with mock.patch.object(self.harness, "validate_post_port_defects",
                               return_value=["defect gate reached"]) as gate:
            self.assertIn("defect gate reached", self.harness.validate(self.root, release=True))
            gate.assert_called_once_with(self.root, release=True)

    def test_open_and_verified_positive_contracts(self):
        self.assertEqual([], self.errors(document(record())))
        self.assertEqual([], self.errors(document(self.verified()), release=True))
        confirmed = record(**{"Confirmation": "source_confirmed",
                              "Original reference": "1eb44: original producer and consumer traced."})
        self.assertEqual([], self.errors(document(confirmed)))

    def test_required_fields_cannot_be_missing_or_empty(self):
        for field in ["Status", "Confirmation", "Owner", "Discovered", "Original reference",
                      "Evidence", "Next action", "Verification", "Owner verification", "Push approval"]:
            for empty in (False, True):
                with self.subTest(field=field, empty=empty):
                    text = document(record())
                    lines = text.splitlines()
                    text = "\n".join((f"- {field}: " if empty else "")
                                     if line.startswith(f"- {field}:") else line for line in lines)
                    self.assertTrue(any(field in error for error in self.errors(text)))

    def test_duplicate_field_keys_are_not_silently_overwritten(self):
        for field in ("Status", "Owner", "Evidence", "Custom note"):
            with self.subTest(field=field):
                text = document(record()) + f"\n- {field}: first\n- {field}: second\n"
                self.assertTrue(any("Duplicate field" in error for error in self.errors(text)))

    def test_enumerated_states_are_checked(self):
        for field, value in {"Status": "complete", "Confirmation": "assumed",
                             "Owner verification": "yes", "Push approval": "approved"}.items():
            with self.subTest(field=field):
                self.assertTrue(any(f"Invalid {field}" in error for error in
                                    self.errors(document(record(**{field: value})))))

    def test_discovery_date_checks_calendar_and_exact_format(self):
        for value in ("2026-02-30", "2025-02-29", "2026-1-01", "yesterday", "2026-11-12T12:00"):
            with self.subTest(date=value):
                self.assertTrue(any("Invalid Discovered" in error for error in
                                    self.errors(document(record(Discovered=value)))))

    def test_ids_and_next_counter_cannot_drop_holes_or_reorder(self):
        for records, next_id in [((record(), record()), 3),
                                  ((record(1), record(3)), 4),
                                  ((record(2), record(1)), 3),
                                  ((record(1),), 3), ((record(0),), 1)]:
            with self.subTest(next_id=next_id, records=records):
                self.assertTrue(any("Defect IDs" in error for error in
                                    self.errors(document(*records, next_id=next_id))))
        self.assertTrue(self.errors(document(record()).replace("PPD-002 -->", "PPD-000 -->")))
        self.assertTrue(self.errors(document(record()).replace("<!-- next-defect-id: PPD-002 -->", "")))
        self.assertTrue(self.errors(document(record()) + "\n<!-- next-defect-id: PPD-002 -->\n"))

    def test_history_rejects_highest_drop_even_with_counter_reset(self):
        baseline = document(record(1), self.verified(2), record(3, Status="dismissed", Resolution="Source disproves report."))
        text = document(record(1), self.verified(2))
        self.assertTrue(any("Removed historical defect IDs: PPD-003" in error
                            for error in self.errors(text, baseline_text=baseline)))

    def test_reopening_preserves_identity_and_is_allowed(self):
        baseline = document(self.verified(1), record(2))
        self.assertEqual([], self.errors(document(record(1, Status="investigating"), record(2)),
                                         baseline_text=baseline))

    def test_verified_requires_confirmation_verification_and_owner_pass(self):
        for field, value in [("Confirmation", "reported"), ("Verification", "pending"),
                             ("Verification", "not_run"), ("Verification", "pending GPU run"),
                             ("Verification", "not  run"), ("Verification", "unverified"),
                             ("Owner verification", "pending"), ("Owner verification", "failed")]:
            with self.subTest(field=field, value=value):
                text = document(self.verified()).splitlines()
                text = "\n".join(f"- {field}: {value}" if line.startswith(f"- {field}:")
                                 else line for line in text)
                self.assertTrue(any("Verified PPD" in error for error in self.errors(text)))

    def test_source_confirmation_needs_original_reference(self):
        for value in ("unknown", "pending", "not_run", "N/A"):
            with self.subTest(reference=value):
                text = document(record(**{"Confirmation": "source_confirmed", "Original reference": value}))
                self.assertTrue(any("substantive Original reference" in error for error in self.errors(text)))

    def test_dismissed_and_deferred_need_resolution(self):
        for status in ("dismissed", "deferred"):
            for value in ("", "pending", "TBD"):
                with self.subTest(status=status, resolution=value):
                    self.assertTrue(any("substantive Resolution" in error for error in
                                        self.errors(document(record(Status=status, Resolution=value)))))
            self.assertEqual([], self.errors(document(record(Status=status, Resolution="Owner documented the reason."))))

    def test_duplicate_requires_existing_other_id(self):
        for value in ("", "PPD-001", "PPD-009", "PPD-002 extra"):
            with self.subTest(target=value):
                text = document(record(**{"Status": "duplicate", "Duplicate of": value}), record(2))
                self.assertTrue(any("Invalid Duplicate of" in error for error in self.errors(text)))
        terminal = document(self.verified(), record(2, **{"Status": "duplicate", "Duplicate of": "PPD-001"}))
        self.assertEqual([], self.errors(terminal, release=True))

    def test_duplicate_cycles_fail_but_chains_to_real_records_pass(self):
        for targets in ((2, 1), (2, 3, 1), (2, 3, 2)):
            with self.subTest(targets=targets):
                records = [record(number, **{"Status": "duplicate", "Duplicate of": f"PPD-{target:03d}"})
                           for number, target in enumerate(targets, 1)]
                self.assertTrue(any("Duplicate cycle" in error for error in
                                    self.errors(document(*records), release=True)))
        chain = document(self.verified(), record(2, **{"Status": "duplicate", "Duplicate of": "PPD-001"}),
                         record(3, **{"Status": "duplicate", "Duplicate of": "PPD-002"}))
        self.assertEqual([], self.errors(chain, release=True))

    def test_evidence_needs_citation_and_existing_live_link(self):
        self.assertTrue(any("Markdown citation" in error for error in
                            self.errors(document(record(Evidence="Observed privately")))))
        self.assertTrue(any("Stale Markdown link" in error for error in
                            self.errors(document(record(Evidence="[Missing](work/active/missing.md)")))))

    def test_evidence_direct_validator_checks_files_anchors_and_repo_boundary(self):
        for evidence, expected in [("[Missing](work/active/missing.md)", "Missing evidence path"),
                                   ("[Stale](work/active/observation.md#gone)", "Stale evidence anchor"),
                                   ("[Empty]( )", "Empty evidence target"),
                                   ("[Outside](../../../outside.md)", "escapes repository")]:
            with self.subTest(evidence=evidence):
                self.path.write_text(document(record(Evidence=evidence)))
                self.assertTrue(any(expected in error for error in
                                    self.harness.validate_post_port_defects(self.root, baseline_text="")))
        for evidence in ("[Self](#post-port-defects)", "[Local](work/active/observation.md#observation)",
                         "[Primary](https://example.invalid/primary-evidence)"):
            with self.subTest(evidence=evidence):
                self.path.write_text(document(record(Evidence=evidence)))
                self.assertEqual([], self.harness.validate_post_port_defects(self.root, baseline_text=""))

    def test_regular_allows_unresolved_release_rejects_them(self):
        for status in ("open", "investigating", "ready_for_owner", "deferred"):
            with self.subTest(status=status):
                text = document(record(Status=status, Resolution="Owner deferred pending evidence."))
                self.assertEqual([], self.errors(text))
                self.assertTrue(any("Release has unresolved" in error for error in self.errors(text, release=True)))
        self.assertEqual([], self.errors(document(record(Status="dismissed", Resolution="Source disproves report.")), release=True))

    def test_source_independent_export_never_probes_parent_git(self):
        self.path.write_text(document(record()))
        with mock.patch.object(self.harness, "ROOT", self.root), \
                mock.patch.object(self.harness.subprocess, "run") as run:
            self.assertEqual([], self.harness.validate_post_port_defects(self.root))
            run.assert_not_called()

    def test_git_history_is_readonly_scoped_and_checks_head_and_parent(self):
        self.path.write_text(document(record()))
        (self.root / ".git").write_text("gitdir: test-only-placeholder\n")
        head, parent = document(record()), document(record(), self.verified(2))
        results = [subprocess.CompletedProcess([], 0, str(self.root.resolve()) + "\n"),
                   subprocess.CompletedProcess([], 0, head), subprocess.CompletedProcess([], 0, parent)]
        with mock.patch.object(self.harness, "ROOT", self.root), \
                mock.patch.object(self.harness.subprocess, "run", side_effect=results) as run:
            self.assertTrue(any("PPD-002" in error for error in self.harness.validate_post_port_defects(self.root)))
            self.assertEqual([["git", "-C", str(self.root), "rev-parse", "--show-toplevel"],
                              ["git", "-C", str(self.root), "show", f"HEAD:{DOCUMENT}"],
                              ["git", "-C", str(self.root), "show", f"HEAD^:{DOCUMENT}"]],
                             [call.args[0] for call in run.call_args_list])
        with mock.patch.object(self.harness.subprocess, "run") as run:
            self.assertEqual([], self.harness.defect_history(self.root))
            run.assert_not_called()  # Not this module's ROOT, despite a .git file.

    def test_git_parent_mismatch_and_unavailable_objects_are_skipped(self):
        (self.root / ".git").write_text("gitdir: test-only-placeholder\n")
        with mock.patch.object(self.harness, "ROOT", self.root), \
                mock.patch.object(self.harness.subprocess, "run", return_value=
                                  subprocess.CompletedProcess([], 0, str(self.root.parent))) as run:
            self.assertEqual([], self.harness.defect_history(self.root))
            self.assertEqual(1, run.call_count)
        results = [subprocess.CompletedProcess([], 0, str(self.root)),
                   subprocess.CompletedProcess([], 0, document(record())),
                   subprocess.CompletedProcess([], 128, "")]
        with mock.patch.object(self.harness, "ROOT", self.root), \
                mock.patch.object(self.harness.subprocess, "run", side_effect=results):
            self.assertEqual([document(record())], self.harness.defect_history(self.root))


if __name__ == "__main__":
    unittest.main()
