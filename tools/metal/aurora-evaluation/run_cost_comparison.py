#!/usr/bin/env python3
"""Run the fixed, optimized MetalFX/Aurora/MetalFX bracket; no viewer login."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def observation(command):
    result=subprocess.run(command,capture_output=True,text=True)
    return {'argv':command,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True)
    parser.add_argument('--aurora',type=Path,required=True)
    parser.add_argument('--library',type=Path,required=True)
    parser.add_argument('--uniform-calibration',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--quiescent',action='store_true',help='Isolated component correction: one world and producer complete before encoding')
    args=parser.parse_args()
    fields=['native','aurora','library']
    if args.uniform_calibration:fields.append('uniform_calibration')
    for field in fields:
        value=getattr(args,field).resolve();setattr(args,field,value)
        if not value.is_file():parser.error(f'Missing {field}: {value}')
    output=args.output.resolve()
    if output.exists():parser.error('Output must be fresh; preserve prior records')
    if digest(args.library)!='5d10cbcaefd65d98e50fb2894870a3364e30527b28b7dfd6addc7c7436c09e8a':
        parser.error('Native control resolve library differs from preserved production identity')
    output.mkdir(parents=True)
    env=os.environ.copy()
    removed=[]
    for key in list(env):
        if key.startswith(('MTL_','WGPU_','AURORA_FSR_')) or key in ('DYLD_INSERT_LIBRARIES','METAL_DEVICE_WRAPPER_TYPE'):
            removed.append(key);del env[key]
    identities={k:{'path':str(getattr(args,k)),'sha256':digest(getattr(args,k))} for k in ('native','aurora','library')}
    record={'schema':1,'checkpoint':'c31ffd288bcc555891634fb02070b5d58fd75b34',
        'started_utc':now(),'identities':identities,'removed_environment_keys':sorted(removed),
        'measurement_environment':{k:v for k,v in env.items() if k.startswith(('MTL_','WGPU_','AURORA_FSR_'))},
        'protocol_sha256':digest(ROOT/'tools/metal/aurora-evaluation/workload/benchmark_protocol.json'),
        'shared_workload_sha256':digest(ROOT/'tools/metal/aurora-evaluation/workload/input_workload.c'),
        'power_before':observation(['pmset','-g','batt']),'thermal_before':observation(['pmset','-g','therm']),
        'quiescent_component_mode':args.quiescent,'runs':[]}
    plan=[('metalfx-a0','native',None),('aurora-off','aurora','off'),
          ('aurora-ordinary','aurora','ordinary'),('metalfx-a1','native',None)]
    for label,method,arm in plan:
        if digest(getattr(args,method))!=identities[method]['sha256']:
            raise RuntimeError(f'{method} executable changed during comparison')
        if method=='native':
            data=output/f'{label}.json'
            command=[str(args.native),'--output',str(data),'--library',str(args.library),
                     '--warmup','360','--samples','360','--max-inflight','1' if args.quiescent else '3']
        else:
            data=output/label/'benchmark.json'
            command=[str(args.aurora),'benchmark',str(data.parent),'--rcas',arm,'--warmup','360','--samples','360']
        if args.quiescent:command.append('--quiescent')
        receipt={'label':label,'argv':command,'started_utc':now(),'binary_sha256':identities[method]['sha256'],
                 'data_path':str(data),'power':observation(['pmset','-g','batt']),
                 'thermal':observation(['pmset','-g','therm'])}
        print(f'{receipt["started_utc"]} START {label}',flush=True)
        with (output/f'{label}.stdout.log').open('w') as stdout,(output/f'{label}.stderr.log').open('w') as stderr:
            result=subprocess.run(command,cwd=ROOT,env=env,stdout=stdout,stderr=stderr)
        receipt.update(exit_code=result.returncode,finished_utc=now())
        if data.exists():receipt['data_sha256']=digest(data)
        record['runs'].append(receipt)
        (output/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
        print(f'{receipt["finished_utc"]} END {label} exit={result.returncode}',flush=True)
        if result.returncode:raise SystemExit(result.returncode)
    if args.uniform_calibration:
        binary=args.uniform_calibration.resolve();directory=output/'uniform-transfer-bound'
        receipt={'label':'uniform-transfer-bound','argv':[str(binary),str(directory)],
                 'started_utc':now(),'binary_sha256':digest(binary)}
        print(f'{receipt["started_utc"]} START uniform-transfer-bound',flush=True)
        with (output/'uniform-transfer-bound.stdout.log').open('w') as stdout,(output/'uniform-transfer-bound.stderr.log').open('w') as stderr:
            result=subprocess.run(receipt['argv'],cwd=ROOT,env=env,stdout=stdout,stderr=stderr)
        receipt.update(exit_code=result.returncode,finished_utc=now());record['uniform_calibration']=receipt
        (output/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
        print(f'{receipt["finished_utc"]} END uniform-transfer-bound exit={result.returncode}',flush=True)
        if result.returncode:raise SystemExit(result.returncode)
    record.update(finished_utc=now(),power_after=observation(['pmset','-g','batt']),thermal_after=observation(['pmset','-g','therm']))
    (output/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Completed the fixed comparison; raw samples retained.',flush=True)

if __name__=='__main__':
    main()
