#!/usr/bin/env python3
"""Reduce retained actual-resolution cost records; never executes GPU work."""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics

ARMS = (('metalfx-a0', 'metalfx-a0.json', 'native'),
        ('aurora-off', 'aurora-off/benchmark.json', 'wgpu'),
        ('aurora-ordinary', 'aurora-ordinary/benchmark.json', 'wgpu'),
        ('metalfx-a1', 'metalfx-a1.json', 'native'))


def identity(path):
    return {'name':path.name, 'bytes':path.stat().st_size,
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}


def stats(values):
    values = sorted(float(x) for x in values)
    if not values or not all(math.isfinite(x) for x in values):
        raise ValueError('Expected nonempty finite observations')
    return {'count':len(values), 'min_ms':values[0], 'median_ms':statistics.median(values),
            'p95_ms':values[math.ceil(.95*len(values))-1],
            'mean_ms':statistics.mean(values), 'max_ms':values[-1]}


def wgpu_envelope(sample, period):
    # Stage intervals may overlap. Never sum them into a fictitious frame cost.
    fields = ('adapter_start_tick', 'first_fsr_begin_tick', 'color_end_tick', 'display_depth_end_tick')
    if not math.isfinite(period) or period <= 0 or any(not isinstance(sample[k], int) or sample[k] < 0 for k in fields):
        raise ValueError('Expected finite positive clock period and nonnegative integer ticks')
    start = min(sample['adapter_start_tick'], sample['first_fsr_begin_tick'])
    end = max(sample['color_end_tick'], sample['display_depth_end_tick'])
    if not all(math.isfinite(x) for x in (start,end,period)) or end <= start or period <= 0:
        raise ValueError('Invalid positive WGPU envelope or timestamp period')
    return (end-start)*period/1e6


def load_arm(path, kind, quiescent):
    v = json.loads(path.read_text()); samples = v['samples']
    if len(samples) < 360 or [s['sample'] for s in samples] != list(range(len(samples))):
        raise ValueError('Need at least360 contiguous measured samples')
    if kind == 'native':
        d=v['dimensions']; shape=(d['input_width'],d['input_height'],d['output_width'],d['output_height'])
        warm=v['warmup_worlds']; phases=d['phases']; fingerprints=v['phase_input_fingerprints_fnv64']
        invalid=v['validation_run'] or any(x is not None for x in v['environment'].values())
        if quiescent and (not v['quiescent_component_mode'] or v['maximum_pending_worlds'] != 1):
            raise ValueError('Native primary comparison must have one pending world')
        gpu_key='gpu_total_ms'; clock='native_MTLCommandBuffer_GPU_seconds'
        for s in samples:
            expected=(s['gpu_end_seconds']-s['gpu_start_seconds'])*1000
            if not math.isfinite(expected) or expected <= 0 or abs(expected-s[gpu_key]) > 1e-6:
                raise ValueError('Native GPU seconds/duration mismatch')
    else:
        d=v['dimensions']; shape=(d['width'],d['height'],d['output_width'],d['output_height'])
        warm=v['warm_worlds']; phases=v['jitter_phases']; fingerprints=v['phase_fingerprints_fnv64']
        invalid=v['validation_enabled'] or v['debug_assertions'] or bool(v['environment'])
        if quiescent and (not v['quiescent'] or v['in_flight_limit'] != 1):
            raise ValueError('WGPU primary comparison must have one pending world')
        gpu_key='total_gpu_ms'; clock='wgpu_stage_counter_ticks_uncalibrated_to_native_seconds'
        for s in samples:
            if abs(wgpu_envelope(s,v['timestamp_period_ns'])-s[gpu_key]) > 1e-6:
                raise ValueError('WGPU stage ticks/duration mismatch')
    if v['device']['name'] != 'Apple M1 Max' or (kind == 'wgpu' and v['device']['backend'] != 'Metal'):
        raise ValueError('Expected the actual Metal M1 Max device')
    declared=v.get('measured_worlds',v.get('measured_samples'))
    if declared != len(samples):
        raise ValueError('Declared and actual sample counts differ')
    if invalid:
        raise ValueError('Measurement contains validation/debug/injected environment overhead')
    if shape != (2304,1446,3456,2168) or warm < 360 or phases != 18 or len(fingerprints) != 18:
        raise ValueError('Unexpected target dimensions, warmup or phase contract')
    if any(s['phase'] != s['sample'] % 18 or not math.isfinite(s[gpu_key]) or s[gpu_key] <= 0 for s in samples):
        raise ValueError('Invalid phase sequence or positive GPU duration')
    return v, samples, gpu_key, clock, fingerprints


def reduce_arm(v, samples, kind, gpu_key):
    result={'gpu_complete_envelope':stats(s[gpu_key] for s in samples),
            'source_timing_scope':v.get('timing_scope',v.get('timing_boundaries')),
            'memory':{}, 'cpu':{}}
    if kind == 'native':
        result['clock_calibration']=v['calibration']
        result['source_p50_lower_rank_ms']=v['gpu_total_ms']['p50']
        result['gpu_adapter_interval']=stats(s['gpu_adapter_ms'] for s in samples)
        for out,key in [('encode','cpu_encode_ns'),('submit','cpu_submit_ns'),
                        ('completion_gate_excluded','host_completion_gate_ns'),
                        ('producer_wait_excluded','excluded_producer_wait_ns')]:
            result['cpu'][out]=stats(s[key]/1e6 for s in samples)
        m=v['memory']; keys=('after_drain_bytes','after_native_scaler_creation_bytes','after_preparation_bytes',
            'after_warmup_bytes','algorithm_outputs_bytes','common_resident_phase_pool_bytes','device_base_bytes',
            'explicit_gpu_buffer_creations_preparation','explicit_gpu_texture_creations',
            'explicit_owned_except_common_phase_pool_bytes','explicit_persistent_texture_bytes',
            'fixed_source_input_bytes','observed_warm_residual_device_bytes','opaque_internal_limit',
            'preparation_staging_peak_bytes','preparation_staging_retained_during_benchmark',
            'steady_gpu_resource_creations','steady_observed_max_bytes','steady_observed_min_bytes')
        result['memory']={k:m[k] for k in keys}
    else:
        result['timestamp_period_ns']=v['timestamp_period_ns']
        for out,key in [('gpu_adapter_interval','adapter_gpu_ms'),('fsr_stage_envelope','fsr_pass_envelope_gpu_ms')]:
            result[out]=stats(s[key] for s in samples)
        result['intraworld_stage_overlap']={
            'adapter_end_after_first_fsr_begin_samples':sum(s['adapter_end_tick']>s['first_fsr_begin_tick'] for s in samples),
            'display_depth_end_before_color_end_samples':sum(s['display_depth_end_tick']<s['color_end_tick'] for s in samples),
            'note':'Adapter and FSR stage intervals are elapsed envelopes, not additive exclusive shader times.'}
        for out,key in [('encode','cpu_encode_wall_ms'),('submit','cpu_submit_wall_ms'),
                        ('completion_gate_excluded','completion_gate_wall_ms'),
                        ('producer_wait_excluded','input_producer_wait_wall_ms')]:
            if all(key in s for s in samples):result['cpu'][out]=stats(s[key] for s in samples)
        m=v['memory']; result['memory']=m
        result['memory_counter_limit']='Metal HAL buffer_memory/texture_memory/memory_allocations zeros are unimplemented counters, not proof of zero memory or allocations.'
    return result


def reduce(study, pipelined, uniform):
    result={'schema_version':1,'scope':'Optimized isolated component cost on target-size synthetic perspective MASK/HDR inputs; not image-quality acceptance, HomeFPS, exclusive shader self-time or a future native-MSL estimate.',
      'statistics':'Raw samples recomputed with midpoint median and nearest-rank p95. Native original p50 uses a lower-rank convention and is preserved separately; controls are not pooled as independent trials.',
      'dimensions':{'input':[2304,1446],'output':[3456,2168]},'primary':{},'pipelined_diagnostic':{},'source_identities':{},
      'clock_domains':'Native MTLCommandBuffer timestamps are GPU seconds with sampleTimestamps calibration. WGPU values are stage-counter ticks with recorded ns/tick; there is no cross-API clock alignment. Durations are comparable; absolute timestamps are not joined.',
      'gpu_boundary_limit':'Aurora complete envelope includes conversion, reconstructed-depth clear, completeFSRgraph, RCASifselected and display-depth. Normal208B/world uniform GPUtransfer precedes observedstagebounds; its exact isolatedGPUcost is missing. CPU encoding includes the queue write. See the bounded observed transfer calibration; do not add its scheduler/host-inclusive span as exactGPUselftime.',
      'allocation_limit':'CurrentAllocatedSize is live device footprint, not a cumulative allocation counter or guaranteed residency. Stable before/after values do not prove absent hidden driver temporaries. Native/ WGPU input pools have different allocation/payload accounting; do not equate them.'}
    receipt_path=study/'receipt.json'; receipt=json.loads(receipt_path.read_text())
    if receipt['measurement_environment'] or [r['label'] for r in receipt['runs']] != [a[0] for a in ARMS] or any(r['exit_code'] != 0 for r in receipt['runs']):
        raise ValueError('Invalid bracketing order, execution or measurement environment')
    result['ordered_execution_receipt']={'identity':identity(receipt_path), 'checkpoint':receipt['checkpoint'],
        'started_utc':receipt['started_utc'], 'finished_utc':receipt['finished_utc'],
        'protocol_sha256':receipt['protocol_sha256'], 'shared_workload_sha256':receipt['shared_workload_sha256'],
        'binary_identities':{key:{'name':Path(value['path']).name,'sha256':value['sha256']} for key,value in receipt['identities'].items()},
        'runs':[{key:r[key] for key in ('label','started_utc','finished_utc','exit_code','binary_sha256','data_sha256')} for r in receipt['runs']]}
    csv_rows=[]; expected_fp=None
    for label,relative,kind in ARMS:
        path=study/relative;v,samples,gpu_key,clock,fp=load_arm(path,kind,True)
        if expected_fp is None:expected_fp=fp
        if fp != expected_fp:raise ValueError('Phase inputs differ across implementations')
        result['source_identities'][f'primary/{label}']=identity(path)
        execution=next(r for r in receipt['runs'] if r['label']==label)
        if execution['data_sha256'] != result['source_identities'][f'primary/{label}']['sha256']:
            raise ValueError('Changed raw primary data after execution receipt')
        result['primary'][label]=reduce_arm(v,samples,kind,gpu_key)
        result['primary'][label].update({'warmup_worlds':v.get('warmup_worlds',v.get('warm_worlds')),
            'measured_worlds':len(samples),'device':v['device']})
        for sample in samples:csv_rows.append({'record_study':'primary_quiescent','record_arm':label,'record_clock_domain':clock,
            **({'record_timestamp_period_ns':v['timestamp_period_ns']} if kind=='wgpu' else {}),**sample})
    result['phase_input_fingerprints_fnv64']=expected_fp
    a0=result['primary']['metalfx-a0']['gpu_complete_envelope'];a1=result['primary']['metalfx-a1']['gpu_complete_envelope']
    result['bracketing_native_drift']={'median_difference_ms':a1['median_ms']-a0['median_ms'],
        'median_percent':100*(a1['median_ms']/a0['median_ms']-1),
        'p95_difference_ms':a1['p95_ms']-a0['p95_ms'],'p95_percent':100*(a1['p95_ms']/a0['p95_ms']-1)}
    for label,relative,kind in ARMS:
        path=pipelined/relative;v,samples,gpu_key,clock,fp=load_arm(path,kind,False)
        if fp != expected_fp:raise ValueError('Diagnostic phase inputs differ')
        result['source_identities'][f'pipelined/{label}']=identity(path)
        diagnostic={'role':'Retained superseded3-in-flight scheduling diagnostic, not primary isolated component cost.',
                    'stage_or_command_envelope':stats(s[gpu_key] for s in samples)}
        if kind=='wgpu':
            period=v['timestamp_period_ns'];ends=[max(s['color_end_tick'],s['display_depth_end_tick']) for s in samples]
            diagnostic['terminal_completion_intervals']=stats((b-a)*period/1e6 for a,b in zip(ends,ends[1:]))
            diagnostic['first_start_to_last_terminal_per_world_ms']=(max(ends)-min(min(s['adapter_start_tick'],s['first_fsr_begin_tick']) for s in samples))*period/1e6/len(samples)
            diagnostic['interpretation']='Earlier-world write-after-read/resource hazards are included in these overlapping stage envelopes; approximately23ms is not the isolated reconstruction service time and not a query clock bug.'
        result['pipelined_diagnostic'][label]=diagnostic
        for sample in samples:csv_rows.append({'record_study':'superseded_pipelined_diagnostic','record_arm':label,'record_clock_domain':clock,
            **({'record_timestamp_period_ns':v['timestamp_period_ns']} if kind=='wgpu' else {}),**sample})
    v=json.loads(uniform.read_text()); samples=v['samples']
    if len(samples)!=64 or v['bytes_per_write']!=208:raise ValueError('Unexpected uniform calibration')
    key='observed_copy_plus_host_and_scheduling_upper_envelope_ms'
    result['uniform_transfer_calibration']={'source_identity':identity(uniform),'sample_count':64,'bytes_per_write':208,
        'observed_upper_envelope':stats(s[key] for s in samples),'scope':v['scope'],'api':v['api'],'order':v['order'],'proof':v['proof'],
        'limitation':'This observed64sample envelope contains host wait/submission/scheduling. It is not isolated GPUtransfer time, not directly additive and not a universal bound.'}
    for s in samples:csv_rows.append({'record_study':'uniform_transfer_calibration','record_arm':'wgpu208B','record_clock_domain':'native_GPU_seconds_plus_relative_CPU_seconds',**s})
    return result,csv_rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--study',type=Path,required=True);parser.add_argument('--pipelined',type=Path,required=True)
    parser.add_argument('--uniform-calibration',type=Path,required=True);parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args();result,rows=reduce(args.study,args.pipelined,args.uniform_calibration)
    args.output_dir.mkdir(parents=True,exist_ok=True);out=args.output_dir/'cost-samples.csv'
    meta=['record_study','record_arm','record_clock_domain','record_timestamp_period_ns']
    columns=meta+sorted(set().union(*(row.keys() for row in rows))-set(meta))
    with out.open('w',newline='') as handle:
        writer=csv.DictWriter(handle,fieldnames=columns,lineterminator='\n');writer.writeheader();writer.writerows(rows)
    result['raw_sample_export']={**identity(out),'rows':len(rows),
        'missing_fields':'EmptyCSVcells are absent in the source, not zeros or inferred records. Source per-sample names and units are preserved.'}
    (args.output_dir/'cost-summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v['gpu_complete_envelope'] for k,v in result['primary'].items()},indent=2))
if __name__=='__main__':main()
