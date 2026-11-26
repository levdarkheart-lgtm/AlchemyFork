#!/usr/bin/env python3
"""Clarify the retained palm's post-stop response; no rendering or new quality gate."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import platform
import numpy as np
import quality as q

ROI = (slice(6, 90), slice(198, 282))

def residual_change(current, previous, mature_current, mature_previous, roi=ROI):
    """Remove each phase's independently mature footprint before differencing."""
    a = current[..., :3] - mature_current[..., :3]
    b = previous[..., :3] - mature_previous[..., :3]
    return float(np.abs(a[roi] - b[roi]).mean(dtype=np.float64))

def phase_pair(first, second, first_phase, second_phase, roi=ROI):
    if first_phase != second_phase:
        raise ValueError('Post-stop cycle comparison requires matching jitter phases')
    return float(q.rgb_delta(first, second)[roi].mean(dtype=np.float64))

def summary(rows):
    names = ('phase_matched_error', 'native_quality_error', 'old_history_fraction',
             'adjacent_rgb_change', 'adjacent_edge_change', 'phase_residual_change',
             'green_excess', 'contrast_over_actual_input', 'edge_energy')
    return {key: float(np.mean([row[key] for row in rows])) for key in names}

def analyze(base: Path):
    data = base / 'shared-inputs-warm360/shared-perspective'
    manifest_path = data / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    required = {'input_width':256, 'input_height':128, 'output_width':384,
                'output_height':192, 'warm_frames':360, 'stable_frames':54,
                'moving_frames':18, 'stopped_frames':36, 'phase_count':18,
                'reference_frames':414}
    if any(manifest.get(k) != value for k, value in required.items()):
        raise ValueError('Expected the unchanged corrected 360-warm palm population')
    main = q.csv_rows(data / 'sequence.csv')
    refs = q.csv_rows(data / 'reference-sequence.csv')
    if len(main) != 468 or len(refs) != 414:
        raise ValueError('Incomplete retained main/reference metadata')
    if any(int(row['phase']) != int(row['frame']) % 18 for row in main + refs):
        raise ValueError('Unexpected phase metadata')
    native = q.image(data / 'native-detail-reference.rgba32f',384,192)
    final_native = q.image(data / 'shifted-native-detail-reference.rgba32f',384,192)
    edge = q.edge_mask(final_native, ROI)
    bg = np.fromfile(data / 'coverage-backgrounds.rgba32f', dtype='<f4').reshape(-1,4)[2]
    arms = [('metalfx', data, data, 'world-', 'reference-world-', 'metalfx-warm360.json'),
            ('aurora-none', base/'quality-warm360/off-main/results',
             base/'quality-warm360/off-reference/results', 'world-', 'world-', 'aurora-off-warm360.json'),
            ('aurora-ordinary08', base/'quality-warm360/ordinary-main/results',
             base/'quality-warm360/ordinary-reference/results', 'world-', 'world-', 'aurora-ordinary-warm360.json')]
    details = {}
    public = {}
    for name, main_dir, ref_dir, main_prefix, ref_prefix, score_name in arms:
        original_path = base / 'offline-quality' / score_name
        original = json.loads(original_path.read_text())
        files = []
        def load(index, reference=False):
            directory, prefix = (ref_dir,ref_prefix) if reference else (main_dir,main_prefix)
            path = directory / f'{prefix}{index:03d}-output.rgba32f'
            files.append(q.identity(path))
            return q.image(path,384,192)
        mature = {phase:load(q.reference_index(refs,18,phase),True) for phase in range(18)}
        initial = {phase:load(360+phase) for phase in range(18)}
        frames = {index:load(index) for index in range(431,468)}
        rows = []
        for index in range(432,468):
            phase = int(main[index]['phase']); previous_phase = int(main[index-1]['phase'])
            current = frames[index]; previous = frames[index-1]
            measures = q.stop_quality(native,final_native,initial[phase],mature[phase],current,ROI)
            delta = q.rgb_delta(current,previous)[ROI]
            input_color = q.image(data/f'world-{index:03d}-input.rgba32f',256,128)
            # Same output84px / input56px region as the existing contrast metric.
            denominator = float((input_color[4:60,132:188,1] - bg[1]).mean(dtype=np.float64))
            green = float((current[ROI][...,1] - bg[1]).mean(dtype=np.float64))
            row = {'stopped_world_count': index-431, 'frame_index':index,
                   'world_id':int(main[index]['world_id']), 'phase':phase,
                   'reference_frame_index':q.reference_index(refs,18,phase),
                   **measures, 'adjacent_rgb_change':float(delta.mean(dtype=np.float64)),
                   'adjacent_edge_change':float(delta[edge].mean(dtype=np.float64)),
                   'phase_residual_change':residual_change(current,previous,mature[phase],mature[previous_phase]),
                   'green_excess':green, 'contrast_over_actual_input':green/denominator,
                   'edge_energy':q.edge_energy(current,ROI)}
            if index >= 450:
                row['same_phase_18_world_change'] = phase_pair(frames[index-18],current,
                    int(main[index-18]['phase']),phase)
            rows.append(row)
        for count in (1,18,36):
            retained = original['stop_samples'][str(count)]
            for key in ('phase_matched_error','native_quality_error','old_history_fraction'):
                if abs(rows[count-1][key]-retained[key]) > 1e-12:
                    raise ValueError(f'{name} stop{count} changed original {key}')
        cycles = {'stopped_1_to_18':summary(rows[:18]), 'stopped_19_to_36':summary(rows[18:])}
        matched = [row['same_phase_18_world_change'] for row in rows[18:]]
        reference_contrast = []
        reference_energy = []
        for phase in range(18):
            reference_contrast.append(float((mature[phase][ROI][...,1]-bg[1]).mean(dtype=np.float64)))
            reference_energy.append(q.edge_energy(mature[phase],ROI))
        residue = [row['old_history_fraction'] for row in rows]
        localized = [row['localized_trail_review_only'] for row in rows]
        public[name] = {
            'original_score_identity':q.identity(original_path),
            'original_failed_bounds_unchanged':original['original_failed_bounds'],
            'stop_samples':{str(count):rows[count-1] for count in (1,18,36)},
            'complete_phase_cycle_means':cycles,
            'matched_phase_first_vs_second_stopped_cycle':{
                'pairs':18,'mean_rgb_change':float(np.mean(matched)), 'maximum_phase_mean_rgb_change':max(matched)},
            'mature_reference_phase_cycle':{'green_excess':float(np.mean(reference_contrast)),
                                            'edge_energy':float(np.mean(reference_energy))},
            'localized_residue_review_only':{
                'maximum_mean_old_history_fraction_over36':max(residue),
                'maximum_single_pixel_projected_fraction_over36':max(x['residue_peak'] for x in localized),
                'maximum_fraction_of_valid_old_only_pixels_over_point1':max(x['fraction_over_point1'] for x in localized),
                'valid_old_only_pixels':rows[0]['disoccluded_valid_pixels']},
            'read_output_file_count':len(files),
            'read_output_identities_aggregate_sha256':hashlib.sha256(json.dumps(files,sort_keys=True).encode()).hexdigest()}
        details[name] = {'per_stopped_world':rows, 'read_output_file_identities':files}
    result = {
        'schema_version':1,
        'scope':'Offline clarification of the already recorded warm360384x192palm outputs. No rendering, new setting, threshold, reference or original score changed.',
        'population':{'main_worlds':468,'independent_reference_worlds':414,'output':[384,192],
                      'stopped_frame_indices':[432,467],'stopped_world_ids':[433,468],
                      'stopped_worlds':36,'jitter_phases':18,'clip_playback_frames_per_second':12,
                      'clip_playback_note':'The54-frameclip contains18moving+36stopped worlds;4.5s playback is not elapsed viewer time or throughput.'},
        'definitions':{
            'own_mature_error':'Mean absolute RGB difference on the original2251changedNativepixels versus the same algorithm independently mature final-camera output at the same jitter phase.',
            'native_quality_error':'Already-recorded mean absolute RGB difference on those SAME2251pixels versus a common unjittered384x192Native detail render at the final camera. Not an ideal antialiased or temporally integrated truth.',
            'old_history_fraction':'Original projected old-image residue on252valid old-only/disoccluded pixels, clipped[0,1]. It is localized evidence, not a whole-image trail score.',
            'adjacent_change':'Consecutive stopped outputs; includes the reconstruction response to successive jitter phases.',
            'phase_residual_change':'Consecutive change after subtracting each phase own mature output. This removes the mature phase footprint but does not by itself classify drift versus oscillation.',
            'matched_phase_cycle_change':'First18stoppedworlds versus next18, pairwise equaljitterphase and unchangedcamera/inputs. Changes reflect remaining history evolution, rather than different sampling phases.',
            'green_excess':'Mean linear-green excess over the knownsky background in the original84x84ROI; contrast divides by corresponding56x56actualinput excess.',
            'native_reference_limitations':'Single unjittered output-resolution material render, zero added featuremipbias and binaryalpha coverage. Pixel-center aliasing, different sampling footprints/filtering and temporal reconstruction can all affect its RGB error. It is a common detail comparison, not a new perfect-quality oracle.',
            'no_new_quality_gate':'All extra metrics are descriptive. Original area/settling failures remain unchanged.'},
        'metadata_identities':{key:q.identity(data/key) for key in ('manifest.json','sequence.csv','reference-sequence.csv')},
        'runtime':{'python':platform.python_version(),'numpy':np.__version__},'algorithms':public}
    return result, details

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence-base',type=Path,default=Path(__file__).resolve().parents[4]/'artifacts/metal/aurora-evaluation-20261126')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--detail-output',type=Path)
    args=parser.parse_args()
    result,details=analyze(args.evidence_base)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    if args.detail_output:
        args.detail_output.parent.mkdir(parents=True,exist_ok=True)
        args.detail_output.write_text(json.dumps(details,indent=2)+'\n')
    for name,row in result['algorithms'].items():
        print(name,json.dumps({'cycles':row['complete_phase_cycle_means'],
                              'phase_matched_cycle_change':row['matched_phase_first_vs_second_stopped_cycle'],
                              'stops':{count:{key:v[key] for key in ('phase_matched_error','native_quality_error','old_history_fraction')}
                                       for count,v in row['stop_samples'].items()}}))
if __name__=='__main__':main()
