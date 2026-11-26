#!/usr/bin/env python3
"""Reduce native Aurora parity/cost evidence without running Metal or the viewer."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
QUALITY = HERE.parent / 'aurora-evaluation/analysis/quality.py'


def identity(path: Path):
    return {'name': path.name, 'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def distribution(values):
    if not values or any(not math.isfinite(v) or v < 0 for v in values):
        raise ValueError('Expected nonempty finite nonnegative measurements')
    v = sorted(values)
    n = len(v)
    return {'count': n, 'median_ms': (v[(n-1)//2]+v[n//2])/2,
            'p95_ms': v[math.ceil(.95*n)-1], 'min_ms': v[0],
            'max_ms': v[-1], 'mean_ms': sum(v)/n}


def cost(path: Path):
    raw = json.loads(path.read_text())
    if raw.get('validation'):
        raise ValueError('Validation run cannot supply optimized cost')
    if raw.get('input') != [2304,1446] or raw.get('output') != [3456,2168]:
        raise ValueError('Expected actual target-size population')
    if raw.get('warmup_worlds', 0) < 360 or len(raw['samples']) < 300:
        raise ValueError('Insufficient declared warmup/samples')
    samples = raw['samples']
    for i, row in enumerate(samples):
        if row['sample'] != i or row['world'] != raw['warmup_worlds']+i:
            raise ValueError('Noncontiguous measurements')
        elapsed = 1000*(row['gpu_end_seconds']-row['gpu_start_seconds'])
        if not (elapsed > 0 and abs(elapsed-row['total_gpu_ms']) < .00001):
            raise ValueError('GPU interval is missing or inconsistent')
    warm, end = raw['after_warmup'], raw['after_measurement']
    return {'scope': raw['scope'], 'input_identity': identity(path),
            'input': raw['input'], 'output': raw['output'],
            'gpu': distribution([x['total_gpu_ms'] for x in samples]),
            'cpu_encode': distribution([x['cpu_encode_ms'] for x in samples]),
            'cpu_submit': distribution([x['cpu_submit_ms'] for x in samples]),
            'excluded_completion_wait': distribution([x['excluded_completion_wait_ms'] for x in samples]),
            'after_warmup': warm, 'after_measurement': end,
            'owned_memory': raw.get('owned_memory', {}),
            'execution': {k:raw[k] for k in ('device','registry_id','os','thermal_state_before','thermal_state_after',
                                           'wall_start_unix_seconds','wall_end_unix_seconds') if k in raw},
            'steady_state_changes': {k:end[k]-warm[k] for k in warm},
            'input_fingerprints':raw['input_fingerprints'],
            'limitations': ['Isolated, one-world-outstanding component cost; not viewer FPS.',
                            'Complete command GPU envelope includes all native conversion, reconstruction, output-depth and glow passes; no overlapping pass durations summed.',
                            'CPU completion gates and untimed resident producer are outside GPU/host encode timers.',
                            'Typed allocation counts cover explicit owned wrappers; Metal compiler/driver internal allocations are opaque.']}


def quality(manifest: Path, main: Path, reference: Path, baseline: Path, output: Path):
    spec = importlib.util.spec_from_file_location('retained_quality', QUALITY)
    q = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(q)
    args = SimpleNamespace(manifest=manifest, main_dir=main, reference_dir=reference,
                           main_pattern='world-{frame:03d}-raw-output.rgba32f',
                           reference_pattern='world-{frame:03d}-raw-output.rgba32f',
                           label='Native Metal Aurora OFF port — unchanged perspective palm',
                           receipt=None, output=output)
    measured = q.score(args)
    original = json.loads(baseline.read_text())
    comparison = {key: {'retained_wgsl': val, 'native': measured['metrics'][key],
                        'absolute_difference': measured['metrics'][key]-val}
                  for key,val in original['metrics'].items()}
    measured['port_comparison'] = {'baseline_identity': identity(baseline),
                                  'metrics':comparison,
                                  'retained_failed_bounds':original['original_failed_bounds'],
                                  'native_failed_bounds':measured['original_failed_bounds'],
                                  'original_bounds_unchanged': measured['original_quality_checks'].keys()==original['original_quality_checks'].keys() and all(
                                      measured['original_quality_checks'][k]['bound']==v['bound'] and measured['original_quality_checks'][k]['operator']==v['operator']
                                      for k,v in original['original_quality_checks'].items())}
    output.write_text(json.dumps(measured,indent=2)+'\n')
    return measured


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='mode',required=True)
    p=sub.add_parser('cost');p.add_argument('input',type=Path);p.add_argument('output',type=Path)
    p=sub.add_parser('quality')
    for key in ('manifest','main','reference','baseline','output'):p.add_argument(key,type=Path)
    a=parser.parse_args()
    if a.mode=='cost':
        result=cost(a.input);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({k:result[k] for k in ('gpu','cpu_encode','cpu_submit','steady_state_changes')},indent=2))
    else:
        result=quality(a.manifest,a.main,a.reference,a.baseline,a.output)
        print(json.dumps({'metrics':result['metrics'],'failed_bounds':result['original_failed_bounds'],
                          'comparison':result['port_comparison']},indent=2))


if __name__=='__main__':main()
