#!/usr/bin/env python3
"""Closed-file native Home comparison reduction; no collection or viewer actions.

Inputs use the production25-metric field contract. No live actions are provided.
This is not a privacy sanitizer: selected raw camera bits and internal IDs remain
in outputs. Each measure qualifies independently. A completed analysis never certifies absent
joins, an entire lossy command stream, GPU occupancy or physical scanout.
"""
from collections import Counter, defaultdict
from collections.abc import Mapping
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
MAX_BYTES = 513 * 1024 * 1024  #512MiB capture plus closing/footer slack.
MAX_LINE = 64 * 1024
MAX_RECORDS = 4_000_000
MAX_COMMANDS = 1_000_000
MAX_FRAMES = 100_000
MAX_U64 = (1 << 64) - 1
NS = 1_000_000_000
PHASES = {0: 'other', 1: 'gbuffer', 2: 'lighting_chain', 3: 'opaque_optical_post'}
CHECKS = ('runtime_identity', 'actual_scene', 'native_fullscreen',
          'quiet_camera_and_controls_matched', 'sole_openhtj2k',
          'no_capture_or_validation', 'single_main_renderer')
MEASURES = ('cadence', 'admission', 'preparation', 'memory', 'input_presentation')

try:
    from . import performance_analysis as public, quiet_diagnostic_summary as native
except ImportError:  # Direct script execution uses this module's directory.
    import performance_analysis as public
    import quiet_diagnostic_summary as native

SCHEMA_PATH = ROOT / 'docs/metal/diagnostic-admission-metrics.json'
SCHEMA_DOCUMENT = json.loads(SCHEMA_PATH.read_text())
if SCHEMA_DOCUMENT.get('version') != 1 or SCHEMA_DOCUMENT.get('wire_schema') != 1:
    raise ValueError('unsupported_public_admission_contract')
SCHEMA = SCHEMA_DOCUMENT['metrics']

class Unqualified(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise Unqualified(reason)


def uint(value):
    return type(value) is int and 0 <= value <= MAX_U64


def one(rows, reason):
    require(len(rows) == 1, reason)
    return rows[0]


def stats(values):
    # Versioned observed-row semantics, including exact repeated values.
    records = [{'frame_id': 0, 'values': [value] + [0] * 7} for value in values]
    names = ('value',) + tuple(f'unused{i}' for i in range(1, 8))
    return public.metric_distributions(records, names)['fields']['value']


def union_ns(intervals):
    merged = []
    for start, end in sorted(intervals):
        if end <= start:
            continue
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return sum(end - start for start, end in merged)


def result(rows, excluded, meaning, **extra):
    reasons = sorted(set(excluded.values()))
    return {'qualified': bool(rows) and not excluded,
            'qualification_reasons': reasons if excluded else ([] if rows else ['no_observations']),
            'qualified_observations': len(rows), 'excluded_observations': len(excluded),
            'exclusions': excluded, 'observations': rows, 'meaning': meaning, **extra}


def context_reasons(context, header):
    reasons = []
    if type(context.get('schema')) is not int or context.get('schema') != 1:
        reasons.append('context_schema_not_1')
    for name in CHECKS:
        if context.get('checks', {}).get(name) is not True:
            reasons.append('context_unverified_' + name)
    if context.get('expected_source_sha') != header.get('source_sha'):
        reasons.append('configured_source_mismatch')
    if context.get('expected_process_id') != header.get('process_id'):
        reasons.append('process_identity_mismatch')
    if context.get('expected_group_cap') not in (2, 3):
        reasons.append('expected_group_cap_not_2_or_3')
    if not isinstance(context.get('run'), str) or not context['run'] or not isinstance(context.get('arm'), str):
        reasons.append('missing_run_or_arm_label')
    return reasons


class Data:
    def __init__(self, rows):
        self.rows = list(rows)
        require(len(self.rows) <= MAX_RECORDS, 'record_budget')
        require(all(isinstance(row, Mapping) for row in self.rows), 'records_not_objects')
        self.header = one([r for r in self.rows if r.get('event') == 'header'], 'header_count')
        self.footer = one([r for r in self.rows if r.get('event') == 'footer'], 'footer_count')
        self.session = self.header.get('session_id')
        require(uint(self.session) and self.session > 0, 'invalid_session')
        require(type(self.header.get('schema')) is int and self.header.get('schema') == 1
                and self.footer.get('session_id') == self.session,
                'schema_or_footer_session')
        require(uint(self.header.get('process_id')) and self.header['process_id'] > 0
                and isinstance(self.header.get('source_sha'), str) and len(self.header['source_sha']) >= 7,
                'header_process_or_source_missing')
        require(self.header.get('cpu_clock') == 'mach_absolute_time_ns'
                and self.header.get('gpu_clock') == 'system_mach_seconds', 'clock_contract')
        self.frames, self.metrics, self.commands = [], defaultdict(list), defaultdict(lambda: defaultdict(list))
        self.problems, seen, frame_iterations = Counter(), set(), set()
        for row in self.rows:
            event = row.get('event')
            if event in ('header', 'footer'):
                continue
            require(row.get('session_id') == self.session, 'foreign_enclosing_session')
            require(uint(row.get('mach_ns')) and row['mach_ns'] > 0, 'invalid_record_time')
            serial = row.get('sequence')
            require(uint(serial) and 0 < serial <= MAX_RECORDS and serial not in seen,
                    'duplicate_or_invalid_sequence')
            seen.add(serial)  # Producer order may differ from timestamp/sequence order.
            if event == 'frame':
                require(all(uint(row.get(name)) for name in native.FRAME_FIELDS
                            if name not in ('native_exact', 'quality')), 'invalid_frame_fields')
                require(type(row.get('native_exact')) is bool and type(row.get('quality')) is int,
                        'invalid_frame_boolean_or_quality')
                require(row['end_mach_ns'] >= row['start_mach_ns'], 'reversed_frame_time')
                require(row['iteration_id'] not in frame_iterations, 'duplicate_frame_iteration')
                frame_iterations.add(row['iteration_id'])
                self.frames.append(row)
                require(len(self.frames) <= MAX_FRAMES, 'frame_budget')
            elif event == 'metrics':
                values = row.get('values')
                require(isinstance(values, list) and len(values) == 8 and all(uint(x) for x in values),
                        'invalid_metric_values')
                if row.get('name') in SCHEMA:
                    self.metrics[row['name']].append(row)
            elif event.startswith('command_'):
                identifier = row.get('command_id')
                require(uint(identifier) and identifier > 0, 'invalid_command_identity')
                self.commands[self.session, identifier][event].append(row)
                require(len(self.commands) <= MAX_COMMANDS, 'command_budget')
        require(self.frames, 'no_native_frames')
        self.frame_by_id = defaultdict(list)
        for frame in self.frames:
            self.frame_by_id[frame['end_display_frame']].append(frame)
        self.world = next((frame['end_mach_ns'] for frame in self.frames
                           if frame['end_startup_state'] == 24), None)
        require(self.world is not None, 'missing_world_anchor')
        self.window = (self.world + 150 * NS, self.world + 200 * NS)
        self.loss = {name: self.footer.get(name) for name in
                     ('dropped_full', 'dropped_contention', 'dropped_budget', 'io_errors',
                      'names_truncated', 'pending_created_at_cutoff', 'pending_committed_at_cutoff')}
        require(all(uint(value) for value in self.loss.values()), 'invalid_footer_counters')
        self.lossless = not any(self.loss[name] for name in
                               ('dropped_full', 'dropped_contention', 'dropped_budget', 'io_errors', 'names_truncated'))
        counts = [self.footer.get(name) for name in ('attempted', 'queued', 'written')]
        require(all(uint(value) for value in counts) and counts[0] >= counts[1] >= counts[2]
                and counts[2] == len(self.rows)-2, 'footer_retained_row_count_mismatch')
        if self.lossless:
            require(counts[0] == counts[1] == counts[2] == len(seen)
                    and min(seen) == 1 and max(seen) == counts[0],
                    'lossless_footer_sequence_mismatch')

    def in_window(self, row):
        return self.window[0] <= row['mach_ns'] < self.window[1]

    def by(self, name, index=0):
        # Assembly (including live prefix+tail) is complete before any index is
        # requested. The row lists remain read-only throughout reduction.
        if not hasattr(self, '_indexes'):
            self._indexes = {}
        key = (name, index)
        if key in self._indexes:
            return self._indexes[key]
        grouped = defaultdict(list)
        for row in self.metrics[name]:
            grouped[row['values'][index]].append(row)
        self._indexes[key] = grouped
        return grouped

    def frame_reason(self, frame):
        values = tuple(frame[name] for name in native.FRAME_FIELDS)
        return native.frame_reasons(values, 24)


def cadence(data, context):
    lo, hi = data.window
    reasons, periods, rejected = [], [], Counter()
    dimensions = context.get('expected_dimensions')
    quality = context.get('expected_quality')
    if (not isinstance(dimensions, list) or len(dimensions) != 2
            or any(type(x) is not int or x <= 0 for x in dimensions) or type(quality) is not int):
        reasons.append('missing_expected_dimensions_or_quality')
    if min(frame['start_mach_ns'] for frame in data.frames) > lo or max(
            frame['end_mach_ns'] for frame in data.frames) < hi:
        reasons.append('fixed_window_not_covered')
    previous = None
    for frame in data.frames:
        if previous is not None:
            start, end = previous['start_mach_ns'], frame['start_mach_ns']
            if min(start,end) < hi and max(start,end) > lo:
                errors = data.frame_reason(previous) + data.frame_reason(frame)
                if end <= start:
                    errors.append('nonpositive_or_reordered_period')
                if frame['iteration_id'] != previous['iteration_id'] + 1:
                    errors.append('iteration_gap_or_reorder')
                if previous['end_display_frame'] != frame['begin_display_frame']:
                    errors.append('display_boundary_gap')
                for endpoint in (previous, frame):
                    if [endpoint['drawable_width'], endpoint['drawable_height']] != dimensions or endpoint['quality'] != quality:
                        errors.append('dimensions_or_quality_changed')
                rejected.update(set(errors))
                if not lo <= start < end <= hi:
                    rejected['crosses_fixed_boundary'] += 1
                elif not errors:
                    periods.append(end - start)
        previous = frame
    reasons.extend(name for name in rejected if name != 'crosses_fixed_boundary')
    if len(periods) < 300:
        reasons.append('fewer_than_300_consecutive_eligible_periods')
    return {'qualified': not reasons, 'qualification_reasons': sorted(set(reasons)),
            'window_mach_ns': [lo, hi], 'world_anchor_mach_ns': data.world,
            'eligible_start_period_ns': stats(periods), 'rejections': dict(rejected),
            'meaning': 'Fixed world150–200s, app start-to-start periods; not scanout. FRAME continuity does not prove other telemetry complete.'}


def command(data, identifier, gpu_required=True):
    events = data.commands.get((data.session, identifier), {})
    names = ('command_create_begin', 'command_create_end', 'command_commit_begin',
             'command_commit_end', 'command_complete')
    stages = {name: one(events.get(name, []), 'missing_or_duplicate_' + name) for name in names}
    times = [stages[name]['mach_ns'] for name in names]
    require(times == sorted(times), 'command_cpu_order')
    require(stages['command_create_end'].get('created') is True, 'command_not_created')
    complete = stages['command_complete']
    require(complete.get('native_status') == 4, 'command_completion_failed')
    start, end = complete.get('gpu_start_seconds'), complete.get('gpu_end_seconds')
    valid_gpu = (complete.get('gpu_time_valid') is True and type(start) in (int, float)
        and type(end) in (int, float) and math.isfinite(start) and math.isfinite(end)
        and 0 < start < end < MAX_U64/NS)
    gpu = [round(start * NS), round(end * NS)] if valid_gpu else None
    valid_gpu = valid_gpu and gpu[1] > gpu[0] and gpu[1] <= complete['mach_ns'] + 2
    require(valid_gpu or not gpu_required, 'command_gpu_time_or_clock_invalid')
    return {'command_id': identifier, 'creation_session': data.session,
            'creation_frame': stages['command_create_begin'].get('frame_id'),
            'label': stages['command_create_begin'].get('label'),
            'create_span_ns': times[1] - times[0], 'commit_span_ns': times[3] - times[2],
            'commit_begin_ns': times[2], 'commit_end_ns': times[3],
            'gpu_envelope_ns': gpu if valid_gpu else None,
            'gpu_envelope_duration_ns': gpu[1] - gpu[0] if valid_gpu else None,
            'gpu_start_minus_commit_end_ns': gpu[0] - times[3] if valid_gpu else None,
            'gpu_end_to_completion_observed_ns': times[4] - gpu[1] if valid_gpu else None,
            'gpu_seconds_original': [start, end], 'gpu_conversion_rounding_allowance_ns': 2,
            'meaning': 'Command envelope and CPU observation boundaries; overlaps allowed, not execution stage gaps or occupancy.'}


def wait_pairs(data, identifier):
    events = data.commands.get((data.session, identifier), {})
    rows = sorted(events.get('command_wait_begin', []) + events.get('command_wait_end', []),
                  key=lambda row: (row['mach_ns'], row['sequence']))
    pairs, pending = [], None
    for row in rows:
        if row['event'] == 'command_wait_begin':
            require(pending is None, 'overlapping_or_missing_wait_end')
            pending = row
        else:
            require(pending is not None, 'missing_wait_begin')
            require(row.get('caller_frame_id') == pending.get('caller_frame_id'), 'wait_caller_mismatch')
            pairs.append((pending, row))
            pending = None
    require(pending is None, 'missing_wait_end')
    return pairs


def admission(data, context):
    unblocks, observers = data.by('main_admission_unblock'), data.by('main_admission_observer')
    targets, truncated = data.by('main_admission_target'), data.by('main_admission_target_truncated')
    mapping = data.by('main_group_command', 2)
    qualified, excluded = [], {}
    block_rows = data.by('main_admission_block')
    for name in ('main_admission_target', 'main_admission_unblock', 'main_admission_observer'):
        for row in data.metrics[name]:
            if data.in_window(row) and not block_rows[row['values'][0]]:
                excluded['orphan/' + str(row['values'][0])] = 'admission_record_without_block'
    for row in data.metrics['main_admission_block']:
        if not data.in_window(row):
            continue
        block, requested, oldest, phase, groups, submissions, reservations, cap = row['values']
        try:
            one(block_rows[block], 'duplicate_block_identity')
            require(cap == context.get('expected_group_cap'), 'configured_group_cap_mismatch')
            require(phase in PHASES, 'invalid_requested_phase')
            require(oldest < requested and groups >= cap, 'invalid_cap_decision')
            require(not truncated[block], 'target_list_truncated')
            end = one(unblocks[block], 'unblock_count')
            observer = one(observers[block], 'observer_count')
            ev, ov = end['values'], observer['values']
            require(ev[1:3] == [requested, oldest], 'block_unblock_identity')
            require(ov[1] == ev[3] and ov[1] <= row['mach_ns'] <= ov[2] <= ov[3] <= ev[4] <= ov[4],
                    'block_observer_order')
            require(len(targets[block]) == submissions and submissions > 0, 'missing_pending_target_rows')
            joined, sampled_targets, tickets = [], [], set()
            for target in targets[block]:
                _, ticket, creation_session, identifier, status, committed, blocks, active = target['values']
                require(creation_session == data.session and active == 1 and identifier > 0,
                        'old_or_inactive_command_token')
                require(ticket not in tickets, 'duplicate_target_ticket')
                tickets.add(ticket)
                bound = one(mapping[identifier], 'command_group_mapping_count')['values']
                require(bound[1] == ticket and bound[2] == identifier, 'target_ticket_mapping')
                require(bound[3] in PHASES and committed in (0, 1) and blocks in (0, 1),
                        'invalid_target_phase_or_flags')
                sampled_targets.append({'ticket': ticket, 'creation_session': creation_session,
                    'command_id': identifier, 'logical_group': bound[0], 'phase': PHASES[bound[3]],
                    'sampled_status': status, 'sampled_is_committed': bool(committed),
                    'blocks_oldest_group': bool(blocks), 'active_at_observation': bool(active)})
                if blocks:
                    require(bound[0] == oldest and committed == 1, 'oldest_target_not_committed')
                    details = command(data, identifier)
                    waits = [(begin, finish) for begin, finish in wait_pairs(data, identifier)
                             if begin.get('caller_frame_id') == row.get('frame_id')
                             and ov[1] <= begin['mach_ns'] <= finish['mach_ns'] <= ev[4]]
                    begin, finish = one(waits, 'target_wait_pair_count')
                    a, b = begin['mach_ns'], finish['mach_ns']
                    g0, g1 = details['gpu_envelope_ns']
                    details.update(ticket=ticket, logical_group=bound[0], phase=PHASES.get(bound[3], 'invalid'),
                        sampled_status=status, sampled_is_committed=committed,
                        wait_mach_ns=[a, b], wait_wall_ns=b-a,
                        wait_gpu_envelope_intersection_ns=max(0, min(b, g1)-max(a, g0)),
                        gpu_end_to_wait_return_ns=b-g1)
                    joined.append(details)
            require(joined, 'no_joined_oldest_targets')
            qualified.append({'block': block, 'frame': row.get('frame_id'), 'requested_group': requested,
                'oldest_group': oldest, 'requested_phase': PHASES.get(phase, 'invalid'),
                'pending_groups': groups, 'pending_submissions': submissions, 'reservations': reservations,
                'configured_cap': cap, 'block_mach_ns': [ov[1], ev[4]],
                'wrapper_wait_wall_ns': ev[5], 'wait_thread_cpu_ns': ev[6] if ev[7] else None,
                'wait_non_cpu_ns': ev[5]-ev[6] if ev[7] and ev[6] <= ev[5] else None,
                'pre_observer_ns': ov[2]-ov[1], 'post_observer_ns': ov[4]-ov[3],
                'outer_observed_wall_ns': ov[4]-ov[1], 'targets': joined,
                'sampled_pending_targets': sampled_targets,
                'target_gpu_envelope_union_ns': union_ns([target['gpu_envelope_ns'] for target in joined])})
        except Unqualified as error:
            excluded[str(block)] = str(error)
    output = result(qualified, excluded,
        'Qualified observed blocks/targets only. Cap policy, sampled state and wrapper/envelope timing; no resource bottleneck/occupancy inference.',
        wrapper_wait_wall_ns=stats([row['wrapper_wait_wall_ns'] for row in qualified]),
        population_completeness_established=data.lossless and not excluded,
        global_stream_loss=data.loss)
    if not qualified and not excluded:
        groups = [row for row in data.metrics['main_group_command'] if data.in_window(row)]
        covered = (min(row['start_mach_ns'] for row in data.frames) <= data.window[0]
                   and max(row['end_mach_ns'] for row in data.frames) >= data.window[1])
        absence = (data.lossless and covered and groups and all(
            row['values'][7] == context.get('expected_group_cap') for row in groups))
        if absence:
            output.update(qualified=True, qualification_reasons=[], exact_main_admission_block_count=0,
                          zero_blocks_proven_by_lossless_active_producer=True)
    return output


def preparation(data):
    begins = defaultdict(list)
    for row in data.metrics['view_gather_begin']:
        begins[row.get('frame_id')].append(row)
    gathered = defaultdict(list)
    for row in data.metrics['view_scene_gathered']:
        gathered[row.get('frame_id')].append(row)
    adopts, encodes = data.by('view_inputs_adopted'), data.by('view_inputs_encoded')
    destinations = data.by('view_destination_generation')
    qualified, excluded = [], {}
    cameras_by_view = data.by('view_camera')
    for name in ('view_inputs_adopted', 'view_inputs_encoded'):
        for row in data.metrics[name]:
            if data.in_window(row) and not cameras_by_view[row['values'][0]]:
                excluded['orphan/' + str(row['values'][0])] = 'adopt_encode_without_camera_identity'
    for camera in data.metrics['view_camera']:
        if not data.in_window(camera):
            continue
        view, group = camera['values'][:2]
        frame = camera.get('frame_id')
        try:
            begin = one(begins[frame], 'gather_begin_count')
            end = one(gathered[frame], 'gather_end_count')
            adopt = one(adopts[view], 'adopt_count')['values']
            encode = one(encodes[view], 'encode_count')['values']
            require(begin['mach_ns'] <= end['mach_ns'] and end['values'][0] == group, 'gather_order_or_group')
            require(adopt[1] == encode[1] == group and adopt[2] == encode[2], 'input_epoch_or_group')
            require(adopt[3] <= adopt[4] <= encode[3] <= encode[4], 'adopt_encode_order')
            require(adopt[5] == adopt[6] == encode[5] == encode[6] == 1, 'adopt_or_encode_not_successful')
            dest = [dict(slot=row['values'][1], identity=row['values'][2], generation=row['values'][3],
                         valid=bool(row['values'][4]), can_issue=bool(row['values'][5]))
                    for row in destinations[group] if row.get('frame_id') == frame]
            require(len({row['slot'] for row in dest}) == len(dest), 'duplicate_destination_slot')
            qualified.append({'view': view, 'logical_group': group, 'frame': frame,
                'input_epoch': adopt[2],
                'gather_mach_ns': [begin['mach_ns'], end['mach_ns']],
                'gather_owner_wall_ns': end['mach_ns']-begin['mach_ns'],
                'adopt_mach_ns': adopt[3:5], 'adopt_wall_ns': adopt[4]-adopt[3],
                'encode_mach_ns': encode[3:5], 'encode_wall_ns': encode[4]-encode[3],
                'gathered_counts': end['values'][1:5], 'source_owner_exists': bool(end['values'][5]),
                'cpu_owner_exists': bool(end['values'][6]), 'destination_exists': bool(end['values'][7]),
                'destinations_sampled': dest, 'immutable_adopt_succeeded': True,
                'resource_lease_exists': True, 'draw_count': adopt[7]})
        except Unqualified as error:
            excluded[str(view)] = str(error)
    return result(qualified, excluded,
        'Whole capture/gather/pre-gate owner wall includes housekeeping, not exclusive CPU/culling. Successful adopt/encode are separate actual spans.',
        gather_owner_wall_ns=stats([row['gather_owner_wall_ns'] for row in qualified]),
        adopt_wall_ns=stats([row['adopt_wall_ns'] for row in qualified]),
        encode_wall_ns=stats([row['encode_wall_ns'] for row in qualified]))


def memory(data):
    observations, excluded = [], {}
    for index, row in enumerate(data.metrics['admission_memory']):
        if not data.in_window(row):
            continue
        block, boundary, sampled, footprint, valid, allocated, owned, sampling = row['values']
        if valid != 1 or sampled > row['mach_ns'] or not sampled:
            excluded[str(index)] = 'invalid_task_memory_sample'
            continue
        in_sample_window = data.window[0] <= sampled < data.window[1]
        observations.append({'mach_ns': row['mach_ns'], 'block': block, 'boundary': boundary,
            'task_sample_mach_ns': sampled, 'task_sample_age_ns': row['mach_ns']-sampled,
            'physical_sample_within_window': in_sample_window,
            'physical_footprint_bytes': footprint if in_sample_window else None, 'device_allocated_bytes': allocated,
            'owned_metal_reported_bytes': owned, 'task_sampling_ns_cached_or_current': sampling})
    observer_windows = []
    for row in data.metrics['admission_memory_observer']:
        block, boundary, start, end = row['values'][:4]
        if start <= end:
            observer_windows.append((block, boundary, start, end, row.get('frame_id')))
    pages, page_excluded = [], {}
    for index, row in enumerate(data.metrics['admission_frame_pages']):
        if not data.in_window(row):
            continue
        block, scope, active, retired, available, active_count, retired_count, truncated = row['values']
        windows = [window for window in observer_windows if window[0] == block
                   and window[4] == row.get('frame_id') and window[2] <= row['mach_ns'] <= window[3]]
        if truncated:
            page_excluded[str(index)] = 'page_storage_truncated'
        elif len(windows) != 1:
            page_excluded[str(index)] = 'page_observer_window_count'
        else:
            pages.append({'mach_ns': row['mach_ns'], 'block': block, 'boundary': windows[0][1], 'scope': scope,
                'active_bytes': active, 'retired_bytes': retired, 'available_bytes': available,
                'active_pages': active_count, 'retired_pages': retired_count})
    overhead = []
    for row in data.metrics['admission_memory_observer']:
        if data.in_window(row) and row['values'][2] <= row['values'][3]:
            overhead.append(row['values'][3]-row['values'][2])
    summaries = {}
    for boundary in (0, 1, 2):
        selected = [row for row in observations if row['boundary'] == boundary]
        summaries[str(boundary)] = {key: stats([row[key] for row in selected if row[key] is not None]) for key in
            ('physical_footprint_bytes', 'device_allocated_bytes', 'owned_metal_reported_bytes')}
    page_summaries = {}
    for boundary in (0, 1, 2):
        for scope in (1, 2):
            selected = [row for row in pages if row['boundary'] == boundary and row['scope'] == scope]
            page_summaries[f'{boundary}/{scope}'] = {key: stats([row[key] for row in selected]) for key in
                ('active_bytes', 'retired_bytes', 'available_bytes', 'active_pages', 'retired_pages')}
    pools = [{'mach_ns': row['mach_ns'], 'frame': row.get('frame_id'),
              **dict(zip(('block', 'boundary', 'cached_targets', 'cached_bytes', 'deferrals_cumulative',
                          'acquires_cumulative', 'reuses_cumulative', 'evictions_cumulative'), row['values']))}
             for row in data.metrics['admission_target_pool'] if data.in_window(row)]
    output = result(observations, excluded,
        'Sparse observed gauges in distinct overlapping memory scopes; no additive RAM or per-group marginal allocation claim.',
        by_boundary=summaries, complete_page_observations=pages, page_exclusions=page_excluded,
        page_bytes_by_boundary_scope=page_summaries, target_pool_observations=pools,
        observer_wall_ns=stats(overhead), physical_sample_distinct_timestamps=len({row['task_sample_mach_ns'] for row in observations}),
        pages_qualified=bool(pages) and not page_excluded, sampled_peak_is_not_complete_peak=True)
    if not output['pages_qualified']:
        output['qualified'] = False
        output['qualification_reasons'] = sorted(set(output['qualification_reasons'] + ['page_bytes_unqualified']))
    return output


def camera_observations(data):
    chunks, inputs = data.by('view_camera_bits'), data.by('view_input')
    cameras, excluded, previous = {}, {}, None
    for row in sorted(data.metrics['view_camera'], key=lambda item: item['values'][0]):
        serial, group, mh, ph, known, changed, finite, _ = row['values']
        try:
            require(serial not in cameras and str(serial) not in excluded, 'duplicate_view_camera')
            before = previous if previous and previous['view'] + 1 == serial else None
            if not known or changed:
                pieces = {first: [] for first in (0, 6, 12, 18, 24, 30)}
                for part in chunks[serial]:
                    first = part['values'][1]
                    require(first in pieces, 'invalid_camera_chunk_offset')
                    pieces[first].append(part)
                bits = []
                for first, rows in pieces.items():
                    part = one(rows, 'missing_or_duplicate_camera_bits')['values'][2:]
                    require(all(value <= 0xffffffff for value in part), 'camera_component_not_u32')
                    count = min(6, 32-first)
                    require(not any(part[count:]), 'camera_padding_nonzero')
                    bits.extend(part[:count])
            else:
                require(before is not None, 'unchanged_camera_without_consecutive_raw_proof')
                require(not chunks[serial], 'unexpected_unchanged_bit_chunks')
                bits = before['bits']
            require(finite == 1 and all(math.isfinite(struct.unpack('<f', struct.pack('<I', value))[0])
                                       for value in bits), 'nonfinite_camera')
            def identity(values):
                value = 14695981039346656037
                for component in values:
                    value = ((value ^ component) * 1099511628211) & MAX_U64
                return value
            require(identity(bits[:16]) == mh and identity(bits[16:]) == ph, 'raw_camera_hash_mismatch')
            if before:
                require(bool(changed) == (bits != before['bits']), 'camera_changed_flag_mismatch')
            entry = one(inputs[serial], 'view_input_count')
            vi = entry['values']
            require(vi[1] == group and entry.get('frame_id') == row.get('frame_id'), 'view_input_identity')
            require(vi[2] <= vi[3], 'input_range_reversed')
            if before:
                require(vi[2] == before['current_input'], 'view_input_chain_gap')
            camera = {'view': serial, 'group': group, 'frame': row.get('frame_id'), 'mach_ns': row['mach_ns'],
                'bits': list(bits), 'modelview_identity': mh, 'projection_identity': ph,
                'changed': bool(changed), 'change_independently_proven': before is not None,
                'previous_input': vi[2], 'current_input': vi[3], 'input_session_matches': vi[7] == 1}
            cameras[serial] = camera
            previous = camera
        except Unqualified as error:
            excluded[str(serial)] = {'reason': str(error), 'mach_ns': row['mach_ns'],
                                     'frame': row.get('frame_id')}
            previous = None
    return cameras, excluded


def presentation_observations(data, context, cameras):
    names = ('drawable_present_submit', 'drawable_source_identity', 'drawable_host_clock', 'drawable_presented')
    records = defaultdict(lambda: defaultdict(list))
    for name in names:
        for row in data.metrics[name]:
            records[row['values'][0], row['values'][1]][name].append(row)
    sources, submissions, camera_frames = defaultdict(list), defaultdict(list), defaultdict(list)
    for row in data.metrics['ui_background_source']:
        sources[row.get('frame_id')].append(row)
    for row in data.metrics['ui_background_submission']:
        submissions[row.get('frame_id')].append(row)
    for camera in cameras.values():
        camera_frames[camera['frame']].append(camera)
    all_adopts, all_encodes = data.by('view_inputs_adopted'), data.by('view_inputs_encoded')
    qualified, excluded = [], {}
    # A complete actual UI submission with a missing drawable tuple is a failed
    # join, not evidence that no presentation was requested in that frame.
    submitted_frames = defaultdict(list)
    for row in data.metrics['drawable_present_submit']:
        submitted_frames[row['values'][6]].append(row)
    for frame, candidates in sources.items():
        if (len(candidates) == 1 and candidates[0]['values'][6:8] == [1, 0]
                and len(submissions[frame]) == 1
                and submissions[frame][0]['values'][1:4] == [1, 1, 1]
                and len(submitted_frames[frame]) != 1):
            excluded['ui/' + str(frame)] = {'reason': 'submitted_ui_drawable_count',
                                           'mach_ns': candidates[0]['mach_ns'], 'frames': [frame]}
    for (identifier, drawable), grouped in records.items():
        error_frames = {row['values'][6] if name in ('drawable_present_submit', 'drawable_presented')
                        else row.get('frame_id') for name, items in grouped.items() for row in items}
        try:
            rows = {name: one(grouped[name], 'missing_or_duplicate_' + name) for name in names}
            submit, identity, host, presented = (rows[name]['values'] for name in names)
            frame, source = submit[6], submit[3]
            require(submit[4] == submit[7] == presented[5] == presented[7] == 1 and source != MAX_U64,
                    'cleared_mismatched_or_invalid_present')
            require(presented[2] == source and presented[6] == frame and source <= frame and identity[7] == 0,
                    'presented_source_changed_or_cleared')
            require(host[5] == 1 and 0 < host[3] and host[2] <= host[4]
                    <= rows['drawable_host_clock']['mach_ns'], 'host_calibration_invalid')
            require(host[6:8] == context.get('expected_dimensions'), 'presented_dimensions_mismatch')
            source_row = one(sources[frame], 'ui_source_count')['values']
            submission = one(submissions[frame], 'ui_submission_count')['values']
            require(source_row == [source, submit[2], *identity[2:6], 1, 0], 'ui_source_resource_generation_mismatch')
            require(submission[0] == source and submission[4] == 6
                    and all(submission[i] == 1 for i in (1, 2, 3, 5, 6))
                    and submission[7] == submit[2] == identity[6] and submit[5] == frame,
                    'ui_source_not_submitted_real_world')
            camera = one(camera_frames[source], 'source_frame_camera_count')
            adopt = one(all_adopts[camera['view']], 'presented_adopt_count')['values']
            encode = one(all_encodes[camera['view']], 'presented_encode_count')['values']
            require(adopt[5] == adopt[6] == encode[5] == encode[6] == 1
                    and adopt[1] == encode[1] == camera['group'] and adopt[2] == encode[2],
                    'presented_source_view_not_successful')
            life = command(data, identifier, gpu_required=False)
            lower = presented[3] + host[2] - host[3] - 2
            upper = presented[3] + host[4] - host[3] + 2
            require(0 < lower <= upper and lower <= presented[4] + 2
                    and 0 < presented[4] <= rows['drawable_presented']['mach_ns']
                    and upper >= rows['drawable_present_submit']['mach_ns']
                    and upper >= encode[4], 'presentation_clock_order')
            qualified.append({'command_id': identifier, 'drawable_id': drawable,
                'presentation_frame': frame, 'actual_source_frame': source,
                'ui_source_observed_mach_ns': sources[frame][0]['mach_ns'],
                'drawable_submit_mach_ns': rows['drawable_present_submit']['mach_ns'],
                'ui_submission_boundary_mach_ns': submissions[frame][0]['mach_ns'],
                'source_view': camera['view'], 'source_camera_bits': camera['bits'],
                'source_color': identity[2:4], 'source_depth': identity[4:6],
                'surface_generation': identity[6], 'presentation_mach_interval_ns': [lower, upper],
                'presented_time_host_ns': presented[3], 'callback_observed_mach_ns': presented[4],
                'presentation_to_callback_interval_ns': [presented[4]-upper, presented[4]-lower],
                'retained_older_world': source < frame, 'command': life})
        except Unqualified as error:
            excluded[f'{identifier}/{drawable}'] = {'reason': str(error),
                'mach_ns': min((row['mach_ns'] for rows in grouped.values() for row in rows), default=0),
                'frames': sorted(frame for frame in error_frames if type(frame) is int)}
    return qualified, excluded


def input_interval(values, allowed, view_time):
    _, kind, event_sdl, sample_sdl, before, after, begin, end = values
    require(kind in allowed and event_sdl > 0 and sample_sdl >= event_sdl
            and before <= after and before >= sample_sdl-event_sdl and begin <= end <= view_time,
            'invalid_input_kind_clock_or_order')
    age = sample_sdl-event_sdl
    return [before-age-2, after-age+2], [begin, end]


def trial_population(data, context, trial, cameras, presentations, stable, shown, trial_end):
    """Prove all mandatory parents/children, not the global sink's losslessness.

    Existing native-exact world FRAME contract supplies one main world UI pass
    per successful display increment. All such parents inside this short interval
    must exist; every parent's complete actual presentation tuple must exist.
    """
    guard = trial.get('endguard_frames', 1)
    require(type(guard) is int and 1 <= guard <= 10, 'invalid_endguard_frame_count')
    first_frame, last_frame = stable[0]['frame'], shown['presentation_frame']+guard
    require(first_frame <= last_frame and last_frame-first_frame <= 1000, 'trial_parent_budget')
    camera_frames, present_frames = defaultdict(list), defaultdict(list)
    for camera in cameras.values():
        camera_frames[camera['frame']].append(camera)
    for present in presentations:
        present_frames[present['presentation_frame']].append(present)
    input_rows = data.by('input_dispatch')
    parents, previous = [], None
    for frame in range(first_frame, last_frame+1):
        parent = one([row for row in data.frame_by_id[frame] if row['native_exact']],
                     'trial_native_frame_parent_count')
        require(not data.frame_reason(parent), 'trial_ineligible_native_parent')
        require([parent['drawable_width'], parent['drawable_height']] == context.get('expected_dimensions')
                and parent['quality'] == context.get('expected_quality'), 'trial_dimensions_or_quality_changed')
        if previous:
            require(parent['iteration_id'] == previous['iteration_id']+1
                    and parent['begin_display_frame'] == previous['end_display_frame']
                    and parent['start_mach_ns'] > previous['start_mach_ns'],
                    'trial_frame_iteration_or_display_gap')
        camera = one(camera_frames[frame], 'trial_camera_parent_child_count')
        present = one(present_frames[frame], 'trial_complete_presentation_parent_child_count')
        if frame <= stable[-1]['frame']:
            require(present['source_camera_bits'] == stable[-1]['bits'],
                    'trial_visible_baseline_differs_from_stable_reference')
        require(parent['start_mach_ns'] <= camera['mach_ns'] <= parent['end_mach_ns'],
                'trial_camera_outside_parent')
        require(all(parent['start_mach_ns'] <= present[key] <= parent['end_mach_ns'] for key in
                    ('ui_source_observed_mach_ns', 'drawable_submit_mach_ns', 'ui_submission_boundary_mach_ns')),
                'trial_ui_context_outside_parent')
        require(parent['end_mach_ns'] < trial_end, 'trial_endguard_outside_declared_window')
        if parents:
            require(camera['view'] == parents[-1]['view']+1, 'trial_camera_view_sequence_gap')
        for serial in range(camera['previous_input']+1, camera['current_input']+1):
            require(camera['input_session_matches'], 'trial_input_session_mismatch')
            values = one(input_rows[serial], 'trial_input_range_missing_or_duplicate')['values']
            input_interval(values, trial.get('allowed_input_kinds', []), camera['mach_ns'])
        parents.append({'frame': frame, 'iteration': parent['iteration_id'], 'view': camera['view'],
            'actual_source_frame': present['actual_source_frame'], 'command': present['command_id'],
            'drawable': present['drawable_id'], 'presented_time_host_ns': present['presented_time_host_ns']})
        previous = parent
    # Also reject a retained non-native/ineligible iteration between the checked
    # endpoints. Selecting only successful parents cannot hide that iteration.
    lo, hi = parents[0]['iteration'], parents[-1]['iteration']
    require(len([row for row in data.frames if lo <= row['iteration_id'] <= hi]) == len(parents),
            'trial_extra_or_ineligible_iteration')
    return {'qualified': True, 'first_frame': first_frame, 'last_frame': last_frame,
            'parent_mach_window_ns': [one([row for row in data.frame_by_id[first_frame] if row['native_exact']],
                                        'trial_first_parent_count')['start_mach_ns'], previous['end_mach_ns']],
            'endguard_frames': guard, 'native_parent_count': len(parents),
            'camera_and_actual_presentation_children_per_parent': 1, 'parents': parents,
            'meaning': 'Mandatory native-exact world parent/child population is complete in this bounded interval; global sink loss remains separate.'}


def input_presentation(data, context):
    cameras, camera_errors = camera_observations(data)
    presentations, presentation_errors = presentation_observations(data, context, cameras)
    inputs = data.by('input_dispatch')
    trials, excluded, reference, previous_restored = [], {}, None, False
    for index, trial in enumerate(context.get('input_trials', [])):
        name = trial.get('name', f'trial{index}')
        try:
            require(trial.get('isolated') is True, 'input_protocol_not_isolated')
            ages = trial.get('world_age_s')
            require(isinstance(ages, list) and len(ages) == 2 and all(type(x) in (int, float)
                    and math.isfinite(x) for x in ages) and 0 <= ages[0] < ages[1], 'invalid_trial_window')
            lo, hi = (data.world + round(age * NS) for age in ages)
            cue, baseline = trial.get('external_cue_mach_ns'), trial.get('baseline_camera_bits')
            require(uint(cue) and lo <= cue < hi and isinstance(baseline, list) and len(baseline) == 32
                    and all(uint(word) and word <= 0xffffffff for word in baseline)
                    and all(math.isfinite(struct.unpack('<f', struct.pack('<I', word))[0]) for word in baseline),
                    'external_cue_or_raw_baseline_missing_or_invalid')
            stable_count = trial.get('stable_views')
            require(type(stable_count) is int and stable_count >= 3, 'fewer_than_3_pretrial_stable_views')
            before = sorted((row for row in data.frames if row['end_mach_ns'] <= cue),
                            key=lambda row: row['iteration_id'])[-stable_count:]
            require(len(before) == stable_count and all(not data.frame_reason(row) for row in before),
                    'external_cue_baseline_parent_count_or_eligibility')
            camera_frames = defaultdict(list)
            for camera in cameras.values():
                camera_frames[camera['frame']].append(camera)
            stable = [one(camera_frames[parent['end_display_frame']], 'external_cue_camera_parent_child_count')
                      for parent in before]
            require(all(view['bits'] == baseline for view in stable)
                    and all(not view['changed'] for view in stable[1:])
                    and all(b['frame'] == a['frame']+1 for a,b in zip(stable, stable[1:])),
                    'external_cue_baseline_not_consecutively_reference_stable')
            candidates = sorted((view for view in cameras.values() if cue <= view['mach_ns'] < hi
                                 and view['bits'] != baseline), key=lambda view: view['mach_ns'])
            require(candidates, 'no_changed_main_view_in_trial')
            changed = candidates[0]
            require(changed['change_independently_proven'] and changed['bits'] != stable[-1]['bits'],
                    'first_changed_camera_not_raw_proven')
            if reference is not None:
                require(previous_restored and stable[-1]['bits'] == reference,
                        'camera_not_restored_between_trials')
            reference = stable[-1]['bits']
            previous_restored = trial.get('restored_before_next_trial') is True
            first, last = changed['previous_input'] + 1, changed['current_input']
            require(changed['input_session_matches'] and first <= last, 'no_new_processed_input_range')
            require(uint(trial.get('last_input_serial_before_cue'))
                    and first > trial['last_input_serial_before_cue'], 'pre_cue_input_serial_reused')
            event_intervals, dispatch = [], []
            for serial in range(first, last+1):
                event = one(inputs[serial], 'input_range_missing_or_duplicate')['values']
                interval, dispatched = input_interval(event, trial.get('allowed_input_kinds', []), changed['mach_ns'])
                event_intervals.append(interval)
                dispatch.append(dispatched)
            event_range = [min(pair[0] for pair in event_intervals), max(pair[1] for pair in event_intervals)]
            require(event_range[0] > 0 and event_range[1] <= changed['mach_ns'], 'event_after_view_or_invalid_clock')
            require(event_range[0] >= cue and all(pair[0] >= cue for pair in dispatch),
                    'processed_event_or_dispatch_precedes_external_cue')
            require(all(lo <= pair[0] <= pair[1] < hi for pair in dispatch), 'processed_input_outside_declared_trial')
            affected = [row for row in presentations
                        if row['source_view'] >= changed['view'] and row['source_camera_bits'] != reference
                        and row['presentation_mach_interval_ns'][1] >= changed['mach_ns']
                        and row['presentation_mach_interval_ns'][0] < hi]
            require(affected, 'no_qualified_presentation_of_changed_world')
            shown = min(affected, key=lambda row: row['presented_time_host_ns'])
            bounds = shown['presentation_mach_interval_ns']
            require(bounds[0] >= event_range[1], 'presentation_precedes_processed_events')
            population = trial_population(data, context, trial, cameras, presentations, stable, shown, hi)
            a, b = population['first_frame'], population['last_frame']
            start, end = population['parent_mach_window_ns']
            require(not any(a <= error.get('frame', 0) <= b
                            or (not error.get('frame') and start <= error['mach_ns'] <= end)
                            for error in camera_errors.values()), 'unqualified_camera_in_certified_population')
            require(not any(any(a <= frame <= b for frame in error.get('frames', []))
                            or (not error.get('frames') and start <= error['mach_ns'] <= end)
                            for error in presentation_errors.values()), 'unqualified_presentation_in_certified_population')
            trials.append({'name': name, 'processed_input_serial_range': [first,last],
                'external_cue_mach_ns': cue, 'external_baseline_camera_bits': baseline,
                'processed_input_count': last-first+1, 'event_time_union_bounds_ns': event_range,
                'dispatch_bounds_ns': [min(pair[0] for pair in dispatch), max(pair[1] for pair in dispatch)],
                'first_changed_view': changed['view'], 'first_changed_frame': changed['frame'],
                'first_changed_view_mach_ns': changed['mach_ns'],
                'presentation_is_first_complete_observed_changed_source': True,
                'earliest_presentation_population_proven': True,
                'population_certificate': population,
                'event_to_first_changed_view_interval_ns': [changed['mach_ns']-event_range[1], changed['mach_ns']-event_range[0]],
                'event_to_presented_world_interval_ns': [bounds[0]-event_range[1], bounds[1]-event_range[0]],
                'presentation': shown,
                'meaning': 'Controlled coarse-event-range association with raw camera and actual world presentation; not hardware-to-photon.'})
        except Unqualified as error:
            excluded[str(name)] = str(error)
    return result(trials, excluded,
        'Isolated declared trials only; complete processed serial range and actual selected world source. Callback time is not presentation/scanout.',
        camera_exclusions=camera_errors, presentation_exclusions=presentation_errors,
        qualified_presentations=len(presentations))


def readiness(blocks, preparation_measure):
    by_group = defaultdict(list)
    for view in preparation_measure['observations']:
        by_group[view['logical_group']].append(view)
    for block in blocks['observations']:
        choices = by_group[block['requested_group']]
        if len(choices) != 1:
            block['readiness'] = {'qualified': False, 'reason': 'requested_group_preparation_count'}
            continue
        view = choices[0]
        entry, returned = block['block_mach_ns']
        block['readiness'] = {'qualified': True,
            'gathered_container_end_before_block': view['gather_mach_ns'][1] <= entry,
            'adopt_succeeded_with_lease_before_block': view['adopt_mach_ns'][1] <= entry,
            'adopt_started_after_block_return': view['adopt_mach_ns'][0] >= returned,
            'adopt_start_minus_block_entry_ns': view['adopt_mach_ns'][0]-entry,
            'gather_end_to_block_entry_ns': entry-view['gather_mach_ns'][1],
            'source_owner_exists': view['source_owner_exists'], 'cpu_owner_exists': view['cpu_owner_exists'],
            'destination_exists': view['destination_exists'], 'sampled_destinations': view['destinations_sampled'],
            'meaning': 'Exact observed ordering; gathered containers or sampled destination eligibility are not full pre-gate packet readiness.'}


def all_wrapper_waits(data, blocks):
    rows, excluded = [], {}
    admitted = {(target['command_id'], tuple(target['wait_mach_ns']))
                for block in blocks['observations'] for target in block['targets']}
    for session, identifier in data.commands:
        try:
            for begin, end in wait_pairs(data, identifier):
                if not data.in_window(begin):
                    continue
                life = command(data, identifier)
                interval = [begin['mach_ns'], end['mach_ns']]
                rows.append({'command_id': identifier, 'creation_session': session,
                    'caller_frame': begin.get('caller_frame_id'), 'wait_mach_ns': interval,
                    'wait_wall_ns': interval[1]-interval[0], 'command': life,
                    'joined_main_admission_block': (identifier, tuple(interval)) in admitted,
                    'source_attribution': 'main_admission' if (identifier, tuple(interval)) in admitted
                        else 'other_wrapper_wait_context_not_proven'})
        except Unqualified as error:
            excluded[str(identifier)] = str(error)
    return result(rows, excluded,
        'All retained wrapper waits, including non-admission contexts. No guessed utility attribution or hardware-idle conclusion.')


def analyze_records(rows, context):
    try:
        data = Data(rows)
    except (Unqualified, TypeError, KeyError) as error:
        return {'schema': 1, 'run': context.get('run'), 'qualified_for_comparison': False,
            'qualification_reasons': ['native_input_structure:' + str(error)], 'measures': {}}
    measures = {'cadence': cadence(data, context), 'admission': admission(data, context),
                'preparation': preparation(data), 'memory': memory(data),
                'input_presentation': input_presentation(data, context)}
    readiness(measures['admission'], measures['preparation'])
    waits = all_wrapper_waits(data, measures['admission'])
    reasons = context_reasons(context, data.header)
    for name in ('admission', 'preparation', 'memory', 'input_presentation'):
        measures[name]['global_population_completeness_established'] = data.lossless
    if reasons:
        for measure in measures.values():
            measure['qualified'] = False
            measure['qualification_reasons'] = sorted(set(measure['qualification_reasons'] + reasons))
    required = context.get('required_measures', list(MEASURES))
    if (not isinstance(required, list) or not required or len(set(required)) != len(required)
            or any(name not in MEASURES for name in required)):
        reasons.append('invalid_required_measure_set')
    else:
        reasons.extend('measure_unqualified:' + name for name in required if not measures[name]['qualified'])
    return {'schema': 1, 'run': context.get('run'), 'arm': context.get('arm'),
            'qualified_for_comparison': not reasons, 'qualification_reasons': sorted(set(reasons)),
            'world_anchor_mach_ns': data.world, 'fixed_window_mach_ns': list(data.window),
            'session_id': data.session, 'source_sha_configured': data.header['source_sha'],
            'stream_footer': data.loss, 'stream_lossless': data.lossless,
            'measures': measures, 'all_wrapper_waits': waits,
            'meaning': 'Per-measure qualification is explicit. Preserve partial observations/exclusions; analysis completion is not a successful comparison.'}






def require_measure_qualified(reports, expected_runs, measure):
    """Gate one measure for the exact run set; never infer it from exit0.

    This checks declarations, not raw controls/identity or causal comparability.
    Failed unrelated measures do not silently invalidate an independent measure.
    """
    require(measure in MEASURES and isinstance(reports, list)
            and all(isinstance(report, Mapping) for report in reports),
            'invalid_comparison_measure_or_reports')
    selected = []
    for report in reports:
        part = report.get('measures', {}).get(measure, {})
        selected.append({'run': report.get('run'), 'qualified_for_comparison': part.get('qualified'),
                         'qualification_reasons': part.get('qualification_reasons')})
    try:
        return public.require_qualified(selected, expected_runs)
    except (TypeError, ValueError) as error:
        raise Unqualified('comparison_measure_not_qualified') from error


def read_jsonl(path):
    before, digest, rows = path.stat(), hashlib.sha256(), []
    require(before.st_size <= MAX_BYTES, 'native_byte_budget')
    total = 0
    with path.open('rb') as stream:
        while True:
            line = stream.readline(MAX_LINE+1)
            if not line:
                break
            total += len(line)
            require(len(line) <= MAX_LINE and total <= MAX_BYTES and len(rows) < MAX_RECORDS,
                    'native_line_or_record_budget')
            digest.update(line)
            rows.append(json.loads(line, object_pairs_hook=public._unique_object,
                parse_constant=lambda value: (_ for _ in ()).throw(Unqualified('nonfinite_json'))))
    after = path.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), 'input_changed_during_read')
    return rows, {'bytes': total, 'sha256': digest.hexdigest(), 'mtime_ns': after.st_mtime_ns}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('--context', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'output_exists')
    rows, identity = read_jsonl(args.native)
    context_bytes = args.context.read_bytes()
    require(len(context_bytes) <= 1024*1024, 'context_byte_budget')
    context = json.loads(context_bytes, object_pairs_hook=public._unique_object)
    require(isinstance(context, dict), 'context_not_object')
    report = analyze_records(rows, context)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output/'summary.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    inputs = {'native': identity, 'context': {'sha256': hashlib.sha256(context_bytes).hexdigest()},
              'helper_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'schema_path': str(SCHEMA_PATH.relative_to(ROOT)),
              'schema_sha256': hashlib.sha256(SCHEMA_PATH.read_bytes()).hexdigest(),
              'public_analysis_sha256': hashlib.sha256((ROOT/'tools/metal/performance_analysis.py').read_bytes()).hexdigest(),
              'native_analysis_sha256': hashlib.sha256((ROOT/'tools/metal/quiet_diagnostic_summary.py').read_bytes()).hexdigest()}
    (args.output/'receipt.json').write_text(json.dumps(inputs, indent=2)+'\n')
    print(json.dumps({'run': report['run'], 'qualified_for_comparison': report['qualified_for_comparison'],
                      'qualification_reasons': report['qualification_reasons']}))
    return 0  # Analysis status only; consumers must inspect explicit measure gates.


if __name__ == '__main__':
    raise SystemExit(main())
