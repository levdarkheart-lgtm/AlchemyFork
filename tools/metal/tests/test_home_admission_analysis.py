"""Sanitized closed-file tests; no runtime, real scene, identity or GPU evidence."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import home_admission_analysis as reducer
import home_admission_fixtures as fixture


def metrics(rows, name):
    return [row for row in rows if row.get('name') == name]


def retained(rows):
    """Intentional well-formed stream with a different retained population."""
    events = [row for row in rows if row['event'] not in ('header', 'footer')]
    for serial, row in enumerate(events, 1):
        row['sequence'] = serial
    footer = next(row for row in rows if row['event'] == 'footer')
    for key in ('attempted', 'queued', 'written'):
        footer[key] = len(events)
    return rows


class HomeAdmissionAnalysisTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = fixture.synthetic_records()
        cls.context = fixture.synthetic_context()

    def fresh(self):
        return copy.deepcopy(self.base), copy.deepcopy(self.context)

    def analyze(self, rows=None, context=None):
        return reducer.analyze_records(rows if rows is not None else copy.deepcopy(self.base),
                                       context if context is not None else copy.deepcopy(self.context))

    def test_complete_sanitized_five_measure_example(self):
        report = self.analyze()
        self.assertTrue(report['qualified_for_comparison'])
        self.assertTrue(all(part['qualified'] for part in report['measures'].values()))
        cadence = report['measures']['cadence']['eligible_start_period_ns']
        self.assertEqual(cadence['count'], 500)
        self.assertEqual(cadence['p95'], 100_000_000)
        block = report['measures']['admission']['observations'][0]
        self.assertEqual(block['wrapper_wait_wall_ns'], 38_000_000)
        self.assertEqual(block['targets'][0]['phase'], 'lighting_chain')
        self.assertFalse(block['readiness']['adopt_succeeded_with_lease_before_block'])
        self.assertTrue(block['readiness']['adopt_started_after_block_return'])
        trial = report['measures']['input_presentation']['observations'][0]
        self.assertEqual(trial['event_to_presented_world_interval_ns'], [129_999_981, 130_000_019])
        self.assertNotEqual(trial['presentation']['callback_observed_mach_ns'],
                            trial['presentation']['presentation_mach_interval_ns'][0])

    def test_analysis_exit_zero_does_not_qualify_comparison(self):
        rows, context = self.fresh()
        context['checks']['sole_openhtj2k'] = False
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, ctx, out = root/'native.jsonl', root/'context.json', root/'reduced'
            source.write_text(''.join(json.dumps(row)+'\n' for row in rows))
            ctx.write_text(json.dumps(context))
            completed = subprocess.run([sys.executable, str(Path(reducer.__file__)), str(source),
                                        '--context', str(ctx), '--output', str(out)],
                                       capture_output=True, text=True, check=False)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            report = json.loads((out/'summary.json').read_text())
            self.assertIs(report['qualified_for_comparison'], False)
            with self.assertRaises(reducer.Unqualified):
                reducer.require_measure_qualified([report], [report['run']], 'cadence')

    def test_selected_measure_gate_is_independent_and_exact_runset(self):
        first, second = self.analyze(), self.analyze()
        first['run'], second['run'] = 'A', 'B'
        second['measures']['input_presentation']['qualified'] = False
        second['qualified_for_comparison'] = False
        compared = reducer.require_measure_qualified([first, second], ['A', 'B'], 'cadence')
        self.assertEqual(compared, ['A', 'B'])
        with self.assertRaises(reducer.Unqualified):
            reducer.require_measure_qualified([first, second], ['A', 'B'], 'input_presentation')
        with self.assertRaises(reducer.Unqualified):
            reducer.require_measure_qualified([first, second], ['A', 'C'], 'cadence')

    def test_frame_hole_rejects_even_499_remaining_periods(self):
        rows, context = self.fresh()
        row = next(row for row in rows if row['event'] == 'frame'
                   and row['start_mach_ns'] == fixture.BASE + 170*fixture.NS)
        rows.remove(row)
        report = self.analyze(retained(rows), context)
        self.assertFalse(report['measures']['cadence']['qualified'])
        self.assertIn('iteration_gap_or_reorder', report['measures']['cadence']['qualification_reasons'])

    def test_duplicate_iteration_or_foreign_session_rejects_structure(self):
        for mutate in ('duplicate_iteration', 'foreign_session'):
            with self.subTest(mutate=mutate):
                rows, context = self.fresh()
                frames = [row for row in rows if row['event'] == 'frame']
                if mutate == 'duplicate_iteration':
                    frames[4]['iteration_id'] = frames[3]['iteration_id']
                else:
                    metrics(rows, 'view_camera')[0]['session_id'] = 2
                self.assertFalse(self.analyze(rows, context)['qualified_for_comparison'])

    def test_cutoff_before_fixed_window_rejects_cadence(self):
        rows, context = self.fresh()
        rows = [row for row in rows if row['event'] in ('header', 'footer')
                or row['mach_ns'] < fixture.BASE + 190*fixture.NS]
        report = self.analyze(retained(rows), context)
        self.assertFalse(report['measures']['cadence']['qualified'])
        self.assertIn('fixed_window_not_covered', report['measures']['cadence']['qualification_reasons'])

    def test_observed_dimension_mismatch_rejects_cadence(self):
        rows, context = self.fresh()
        frame = next(row for row in rows if row['event'] == 'frame'
                     and row['start_mach_ns'] == fixture.BASE + 170*fixture.NS)
        frame['drawable_width'] += 1
        self.assertFalse(self.analyze(rows, context)['measures']['cadence']['qualified'])

    def test_native_attempt_sequence_reorder_allowed_but_duplicate_not(self):
        rows, context = self.fresh()
        metrics(rows, 'main_admission_block')[0]['sequence'], metrics(rows, 'main_admission_unblock')[0]['sequence'] = (
            metrics(rows, 'main_admission_unblock')[0]['sequence'], metrics(rows, 'main_admission_block')[0]['sequence'])
        self.assertTrue(self.analyze(rows, context)['qualified_for_comparison'])
        metrics(rows, 'main_admission_unblock')[0]['sequence'] = metrics(rows, 'main_admission_block')[0]['sequence']
        self.assertFalse(self.analyze(rows, context)['qualified_for_comparison'])

    def test_footer_population_mismatch_rejects(self):
        rows, context = self.fresh()
        rows[-1]['written'] -= 1
        self.assertFalse(self.analyze(rows, context)['qualified_for_comparison'])

    def test_global_loss_does_not_invalidate_complete_observed_joins(self):
        rows, context = self.fresh()
        row = metrics(rows, 'admission_target_pool')[0]
        rows.remove(row)
        rows[-1]['dropped_full'] = 1
        rows[-1]['queued'] -= 1
        rows[-1]['written'] -= 1
        report = self.analyze(rows, context)
        self.assertTrue(report['measures']['cadence']['qualified'])
        for name in ('admission', 'preparation', 'memory', 'input_presentation'):
            self.assertTrue(report['measures'][name]['qualified'])
            self.assertFalse(report['measures'][name]['global_population_completeness_established'])
        self.assertTrue(report['measures']['input_presentation']['observations'][0]
                        ['earliest_presentation_population_proven'])

    def test_old_session_same_numeric_command_is_not_same_token(self):
        rows, context = self.fresh()
        metrics(rows, 'main_admission_target')[0]['values'][2] = 999
        report = self.analyze(rows, context)
        self.assertFalse(report['measures']['admission']['qualified'])
        self.assertIn('old_or_inactive_command_token', report['measures']['admission']['qualification_reasons'])

    def test_missing_command_commit_or_gather_begin_fails_affected_measure(self):
        for event, measure in (('command_commit_end', 'admission'), ('view_gather_begin', 'preparation')):
            with self.subTest(event=event):
                rows, context = self.fresh()
                row = next(row for row in rows if row.get('event') == event or row.get('name') == event)
                rows.remove(row)
                report = self.analyze(retained(rows), context)
                self.assertFalse(report['measures'][measure]['qualified'])
                self.assertTrue(report['measures']['cadence']['qualified'])

    def test_zero_blocks_requires_lossless_active_producer_and_cap(self):
        rows, context = self.fresh()
        rows = [row for row in rows if not row.get('name', '').startswith('main_admission_')]
        context['expected_group_cap'] = 3
        for row in metrics(rows, 'main_group_command'):
            row['values'][7] = 3
        report = self.analyze(retained(rows), context)
        admission = report['measures']['admission']
        self.assertTrue(admission['qualified'])
        self.assertEqual(admission['exact_main_admission_block_count'], 0)
        self.assertEqual(admission['wrapper_wait_wall_ns']['count'], 0)
        rows[-1]['attempted'] += 1
        rows[-1]['dropped_full'] = 1
        self.assertFalse(self.analyze(rows, context)['measures']['admission']['qualified'])

    def test_missing_or_bad_camera_proof_rejects_input(self):
        for damage in ('missing_chunk', 'bad_padding', 'nonfinite_bits', 'bad_hash'):
            with self.subTest(damage=damage):
                rows, context = self.fresh()
                bits = [row for row in metrics(rows, 'view_camera_bits') if row['values'][0] == 5]
                if damage == 'missing_chunk':
                    rows.remove(bits[0])
                    retained(rows)
                elif damage == 'bad_padding':
                    bits[-1]['values'][-1] = 1
                elif damage == 'nonfinite_bits':
                    bits[0]['values'][2] = 0x7f800000
                else:
                    metrics(rows, 'view_camera')[-1]['values'][2] ^= 1
                self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

    def test_complete_input_serial_range_required_not_latest_alone(self):
        rows, context = self.fresh()
        metrics(rows, 'view_input')[-1]['values'][3] = 2
        self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

    def test_isolation_and_clock_validity_are_required(self):
        for damage in ('not_isolated', 'event_after_view', 'host_invalid'):
            with self.subTest(damage=damage):
                rows, context = self.fresh()
                if damage == 'not_isolated':
                    context['input_trials'][0]['isolated'] = False
                elif damage == 'event_after_view':
                    metrics(rows, 'input_dispatch')[0]['values'][6:8] = [fixture.BASE+230*fixture.NS]*2
                else:
                    metrics(rows, 'drawable_host_clock')[0]['values'][5] = 0
                self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

    def test_retained_world_cannot_be_replaced_by_attempted_changed_view(self):
        rows, context = self.fresh()
        stable_frame = next(row['frame_id'] for row in metrics(rows, 'view_camera') if row['values'][0] == 4)
        changed_frame = next(row['frame_id'] for row in metrics(rows, 'view_camera') if row['values'][0] == 5)
        for name, positions in (('ui_background_source', [0]), ('ui_background_submission', [0]),
                                ('drawable_present_submit', [3]), ('drawable_presented', [2])):
            for row in metrics(rows, name):
                if row['frame_id'] < changed_frame:
                    continue
                for position in positions:
                    row['values'][position] = stable_frame
        report = self.analyze(rows, context)
        self.assertFalse(report['measures']['input_presentation']['qualified'])
        self.assertIn('no_qualified_presentation_of_changed_world',
                      report['measures']['input_presentation']['qualification_reasons'])

    def test_callback_timestamp_does_not_replace_presented_time(self):
        expected = self.analyze()['measures']['input_presentation']['observations'][0]
        rows, context = self.fresh()
        metrics(rows, 'drawable_presented')[0]['values'][4] += 30*fixture.NS
        metrics(rows, 'drawable_presented')[0]['mach_ns'] += 30*fixture.NS
        actual = self.analyze(rows, context)['measures']['input_presentation']['observations'][0]
        self.assertEqual(actual['event_to_presented_world_interval_ns'],
                         expected['event_to_presented_world_interval_ns'])
        metrics(rows, 'drawable_presented')[0]['values'][5] = 0
        self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

    def test_actual_resource_generation_and_ui_completion_required(self):
        for damage in ('generation', 'incomplete_submission'):
            with self.subTest(damage=damage):
                rows, context = self.fresh()
                if damage == 'generation':
                    metrics(rows, 'drawable_source_identity')[0]['values'][3] += 1
                else:
                    metrics(rows, 'ui_background_submission')[0]['values'][4] = 4
                self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

    def test_submitted_ui_with_whole_drawable_tuple_missing_is_failed_join(self):
        rows, context = self.fresh()
        rows = [row for row in rows if not row.get('name', '').startswith('drawable_')]
        report = self.analyze(retained(rows), context)
        self.assertFalse(report['measures']['input_presentation']['qualified'])
        self.assertTrue(report['measures']['input_presentation']['presentation_exclusions'])

    def test_whole_ui_frame_loss_fails_trial_despite_later_complete_tuple(self):
        rows, context = self.fresh()
        frame = metrics(rows, 'view_camera')[-1]['frame_id']  # guard frame
        names = {'ui_background_source', 'ui_background_submission', 'drawable_present_submit',
                 'drawable_source_identity', 'drawable_host_clock', 'drawable_presented'}
        rows = [row for row in rows if not (row.get('frame_id') == frame and row.get('name') in names)]
        report = self.analyze(retained(rows), context)
        self.assertFalse(report['measures']['input_presentation']['qualified'])

    def test_trial_parent_gap_or_missing_camera_child_is_not_population_certificate(self):
        for missing in ('frame', 'view_camera'):
            with self.subTest(missing=missing):
                rows, context = self.fresh()
                frame = metrics(rows, 'view_camera')[-1]['frame_id']
                if missing == 'frame':
                    rows = [row for row in rows if not (row['event'] == 'frame' and row['end_display_frame'] == frame)]
                else:
                    rows = [row for row in rows if not (row.get('name') == 'view_camera' and row['frame_id'] == frame)]
                self.assertFalse(self.analyze(retained(rows), context)['measures']['input_presentation']['qualified'])

    def test_page_observer_boundary_and_truncation_are_required(self):
        for damage in ('truncated', 'missing_observer'):
            with self.subTest(damage=damage):
                rows, context = self.fresh()
                if damage == 'truncated':
                    metrics(rows, 'admission_frame_pages')[0]['values'][7] = 1
                else:
                    rows.remove(metrics(rows, 'admission_memory_observer')[0])
                    retained(rows)
                self.assertFalse(self.analyze(rows, context)['measures']['memory']['qualified'])

    def test_zero_frame_tag_metric_observations_retained(self):
        rows, context = self.fresh()
        # Pages/memory are still distinguished by actual observer window; zero
        # is not treated as an absent row by the reducer.
        for name in ('admission_memory', 'admission_frame_pages', 'admission_memory_observer'):
            for row in metrics(rows, name):
                row['frame_id'] = 0
        memory = self.analyze(rows, context)['measures']['memory']
        self.assertTrue(memory['qualified'])
        self.assertEqual(memory['qualified_observations'], 3)
        self.assertEqual(len(memory['complete_page_observations']), 6)



    def test_retained_different_pose_is_not_visible_stable_baseline(self):
        data = reducer.Data(copy.deepcopy(self.base))
        cameras, errors = reducer.camera_observations(data)
        self.assertFalse(errors)
        presentations, errors = reducer.presentation_observations(data, self.context, cameras)
        self.assertFalse(errors)
        stable = [cameras[view] for view in (2, 3, 4)]
        first = next(row for row in presentations if row['presentation_frame'] == stable[0]['frame'])
        first['source_camera_bits'] = list(cameras[5]['bits'])
        shown = min((row for row in presentations if row['source_view'] >= 5),
                    key=lambda row: row['presented_time_host_ns'])
        with self.assertRaisesRegex(reducer.Unqualified, 'trial_visible_baseline_differs'):
            reducer.trial_population(data, self.context, self.context['input_trials'][0], cameras,
                                     presentations, stable, shown, fixture.BASE+225_400_000_000)



    def test_unrelated_later_pending_drawable_does_not_poison_certified_response(self):
        rows, context = self.fresh()
        late = copy.deepcopy(metrics(rows, 'drawable_present_submit')[-1])
        late['frame_id'] += 1
        late['values'][0:2] = [999, 999]
        late['values'][5:7] = [late['frame_id'], late['frame_id']]
        late['mach_ns'] += 100_000_000
        rows.insert(-1, late)
        report = self.analyze(retained(rows), context)
        self.assertTrue(report['measures']['input_presentation']['qualified'])
        self.assertTrue(report['measures']['input_presentation']['presentation_exclusions'])

    def test_external_baseline_is_required_and_cannot_be_derived_from_later_view(self):
        rows, context = self.fresh()
        del context['input_trials'][0]['baseline_camera_bits']
        self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

        rows, context = self.fresh()
        context['input_trials'][0]['baseline_camera_bits'] = list(
            reducer.camera_observations(reducer.Data(rows))[0][5]['bits'])
        self.assertFalse(self.analyze(rows, context)['measures']['input_presentation']['qualified'])

    def test_old_queued_event_dispatched_after_cue_is_not_new_controlled_input(self):
        rows, context = self.fresh()
        metrics(rows, 'input_dispatch')[0]['values'][2] -= 20_000_000
        report = self.analyze(rows, context)['measures']['input_presentation']
        self.assertFalse(report['qualified'])
        self.assertIn('processed_event_or_dispatch_precedes_external_cue', report['qualification_reasons'])

    def test_whole_earliest_parent_and_camera_loss_cannot_skip_to_later_population(self):
        data = reducer.Data(copy.deepcopy(self.base))
        cameras, _ = reducer.camera_observations(data)
        presentations, _ = reducer.presentation_observations(data, self.context, cameras)
        stable = [cameras[view] for view in (2, 3, 4)]
        shown = min((row for row in presentations if row['source_view'] >= 5),
                    key=lambda row: row['presented_time_host_ns'])
        missing = cameras[5]['frame']
        del cameras[5]
        data.frames = [row for row in data.frames if row['end_display_frame'] != missing]
        data.frame_by_id.pop(missing)
        presentations = [row for row in presentations if row['presentation_frame'] != missing]
        with self.assertRaises(reducer.Unqualified):
            reducer.trial_population(data, self.context, self.context['input_trials'][0], cameras,
                                     presentations, stable, shown, fixture.BASE+225_400_000_000)

    def test_ambiguous_duplicate_camera_inside_certificate_is_not_hidden_by_valid_copy(self):
        rows, context = self.fresh()
        rows.insert(-1, copy.deepcopy(metrics(rows, 'view_camera')[2]))
        self.assertFalse(self.analyze(retained(rows), context)['measures']['input_presentation']['qualified'])


if __name__ == '__main__':
    unittest.main()
