#!/usr/bin/env python3
"""Compare retained production-path glass outputs; no rendering or counter collection."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from make_media import png

W,H,IW,IH=192,96,128,64
WARM,STABLE,MOVING,STOPPED,PHASES=108,54,18,36,18
COUNT=WARM+STABLE+MOVING+STOPPED
ROI=(slice(18,78),slice(36,94))
CONTROL=(slice(18,78),slice(150,186))
INPUT_ROI=(slice(12,52),slice(24,62))
INPUT_CONTROL=(slice(12,52),slice(102,122))


def identity(p):
    return {'name':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}


def image(p,w=W,h=H):
    a=np.fromfile(p,dtype='<f4')
    if a.size!=w*h*4:raise ValueError(f'Wrong extent: {p}')
    a=a.reshape(h,w,4)
    if not np.isfinite(a[...,:3]).all():raise ValueError(f'Nonfinite RGB: {p}')
    return a


def rgb_change(a,b,roi=ROI):
    require_output(a);require_output(b)
    return float(np.abs(a[roi][...,:3]-b[roi][...,:3]).mean(dtype=np.float64))


def detail(a,roi=ROI):
    require_output(a)
    sy,sx=roi
    return float(np.abs(a[roi][...,:3]-a[sy,slice(sx.start+1,sx.stop+1)][...,:3]).mean(dtype=np.float64))


def require_output(a):
    if a.shape!=(H,W,4):raise ValueError('Output metric requires192x96RGBA; input128x64 uses a distinct contract')
    if not np.isfinite(a[...,:3]).all():raise ValueError('Nonfinite output RGB')


def rms_change(a,b,roi=ROI):
    require_output(a);require_output(b)
    d=a[roi][...,:3].astype(np.float64)-b[roi][...,:3]
    return float(np.sqrt(np.mean(d*d)))


def gradient_change(a,b,roi=ROI):
    require_output(a);require_output(b)
    sy,sx=roi;right=(sy,slice(sx.start+1,sx.stop+1))
    ga=a[right][...,:3]-a[roi][...,:3];gb=b[right][...,:3]-b[roi][...,:3]
    return float(np.abs(ga-gb).mean(dtype=np.float64))


def relative_change(before,after):
    if not (np.isfinite(before) and np.isfinite(after)) or before<0 or after<0:
        raise ValueError('Expected finite nonnegative comparison values')
    return 100*(after/before-1) if before else None


def validate_sequence(directory):
    indices=sorted(int(p.name.split('-')[1]) for p in directory.glob('world-*-output.rgba32f'))
    if indices!=list(range(COUNT)):raise ValueError('Expected216complete chronological worlds with no duplicate/extra frame')
    metrics=directory.parent/(directory.name+'-metrics.json')
    protocol=json.loads(metrics.read_text())
    for key,value in {'warm':WARM,'stationary':STABLE,'moving':MOVING,'stopped':STOPPED,'phase_count':PHASES}.items():
        if protocol.get(key)!=value:raise ValueError(f'Unexpected/missing fixture protocol {key}')
    if (WARM+STABLE)%PHASES or MOVING%PHASES or STOPPED!=2*PHASES:
        raise ValueError('Stopped-cycle comparison must align complete jitter phases')


def displayed(a):
    return np.rint(np.power(np.clip(a[...,:3],0,1),np.float32(1/2.2))*255).astype(np.uint8)


def summarize(directory):
    validate_sequence(directory)
    count=COUNT
    paths=[directory/f'world-{i:03d}-output.rgba32f' for i in range(count)]
    if any(not p.is_file() for p in paths):raise ValueError(f'Incomplete chronological outputs: {directory}')
    outputs=[image(p) for p in paths]
    start_stop=WARM+STABLE+MOVING
    stable=[]
    for i in range(WARM,WARM+STABLE):
        a,b=outputs[i],outputs[i-1]
        stable.append({'frame_index':i,'expected_phase':i%PHASES,'covered_rgb_change':rgb_change(a,b),
                       'covered_rgb_rms_change':rms_change(a,b),'covered_gradient_change':gradient_change(a,b),
                       'uncovered_rgb_change':rgb_change(a,b,CONTROL),'covered_detail':detail(a),
                       'covered_rgb_mean':a[ROI][...,:3].mean(axis=(0,1),dtype=np.float64).tolist()})
    stopped=[]
    for i in range(start_stop,count):
        a,b=outputs[i],outputs[i-1]
        row={'stopped_world':i-start_stop+1,'frame_index':i,'expected_phase':i%PHASES,
             'covered_rgb_change':rgb_change(a,b),'uncovered_rgb_change':rgb_change(a,b,CONTROL),
             'covered_rgb_rms_change':rms_change(a,b),'covered_gradient_change':gradient_change(a,b),
             'covered_detail':detail(a),'covered_rgb_mean':a[ROI][...,:3].mean(axis=(0,1),dtype=np.float64).tolist()}
        if i>=start_stop+PHASES:row['same_phase_previous_stopped_cycle_change']=rgb_change(a,outputs[i-PHASES])
        stopped.append(row)
    masks=[]
    for i in range(WARM,WARM+PHASES):
        def read_mask(name):
            path=directory/f'world-{i:03d}-{name}.r8'
            if not path.is_file() or path.stat().st_size==0:return None
            v=np.fromfile(path,dtype='u1')
            if v.size!=IW*IH:raise ValueError('Wrong input-mask dimensions')
            return v.reshape(IH,IW)
        actual=read_mask('rejection');generated=read_mask('generated')
        z=np.fromfile(directory/f'world-{i:03d}-depth.r32f',dtype='<f4').reshape(IH,IW)
        m=np.fromfile(directory/f'world-{i:03d}-motion.rg16f',dtype='<f2').reshape(IH,IW,2).astype('<f4')
        masks.append({'frame_index':i,'expected_phase':i%PHASES,
                      'history_unsafe_covered_mean':None if actual is None else float(actual[INPUT_ROI].mean(dtype=np.float64)/255),
                      'history_unsafe_uncovered_mean':None if actual is None else float(actual[INPUT_CONTROL].mean(dtype=np.float64)/255),
                      'aurora_generated_reactivity_covered_mean':None if generated is None else float(generated[INPUT_ROI].mean(dtype=np.float64)/255),
                      'aurora_generated_reactivity_uncovered_mean':None if generated is None else float(generated[INPUT_CONTROL].mean(dtype=np.float64)/255),
                      'covered_proxy_depth_mean':float(z[INPUT_ROI].mean(dtype=np.float64)),
                      'covered_motion_max_abs_input_pixels':float(np.abs(m[INPUT_ROI]).max())})
    keys=('covered_rgb_change','covered_rgb_rms_change','covered_gradient_change','uncovered_rgb_change','covered_detail')
    means=lambda rows:{k:float(np.mean([r[k] for r in rows])) for k in keys}
    return {'stationary':means(stable),'stopped1_18':means(stopped[:18]),'stopped19_36':means(stopped[18:]),
            'remaining_same_phase_stopped_cycle_drift':float(np.mean([r['same_phase_previous_stopped_cycle_change'] for r in stopped[18:]])),
            'stationary_rows':stable,'stopped_rows':stopped,'input_rows':masks,
            'outputs_identity_sha256':hashlib.sha256(json.dumps([identity(p) for p in paths],sort_keys=True).encode()).hexdigest()},outputs


def compare_inputs(a,b):
    rows=[]
    for i in range(WARM,WARM+PHASES):
        for kind in ('color','opaque'):
            p=a/f'world-{i:03d}-{kind}.rgba32f';q=b/p.name
            x,y=image(p,IW,IH),image(q,IW,IH)
            d=np.abs(x-y)
            rows.append({'frame_index':i,'input':kind,'rgb_max_difference':float(d[...,:3].max()),
                         'rgb_mean_difference':float(d[...,:3].mean(dtype=np.float64)),
                         'alpha_max_difference':float(d[...,3].max())})
    return {'comparisons':rows,'rgb_max_difference':max(r['rgb_max_difference'] for r in rows),
            'rgb_mean_difference':float(np.mean([r['rgb_mean_difference'] for r in rows])),
            'alpha_max_difference':max(r['alpha_max_difference'] for r in rows)}


def run(a):
    if a.output.exists():raise ValueError('Preserve earlier evidence; output must be fresh')
    a.output.mkdir(parents=True)
    arms=['legacy-production','pbr-single-production','pbr-multi-production','legacy-metalfx']
    result={'scope':'Actual authored glass fixture at128x64→192x96, production jitter/mip/coverage and reconstruction. Not owner-scene acceptance or FPS.',
            'history':{'warm_worlds':WARM,'stationary_worlds':STABLE,'moving_worlds':MOVING,'stopped_worlds':STOPPED,
                       'phase_count':PHASES,'phase_note':'Expected phases from the production18-phase source sequence. This reducer does not invent missing per-frame metadata.'},
            'settling_limit':'The36 retained stopped worlds permit comparing adjacent phase cycles. No independently mature final-camera reference was recorded; these are remaining-history drift and detail-response diagnostics, not mature settling error or a new quality oracle.',
            'previews':'Linear HDR RGB clamp[0,1], gamma1/2.2; PNG lossless in this display conversion. Clips deliberately12fps, not viewer time.',
            'arms':{}}
    for name in arms:
        b,c=a.baseline/name,a.candidate/name
        if not b.exists() or not c.exists():
            result['arms'][name]={'missing':'baseline' if not b.exists() else 'candidate'};continue
        before,x=summarize(b);after,y=summarize(c)
        result['arms'][name]={'baseline':before,'candidate':after,'same_phase_input_parity':compare_inputs(b,c),
                              'stationary_change_percent':relative_change(before['stationary']['covered_rgb_change'],after['stationary']['covered_rgb_change']),
                              'stationary_rms_change_percent':relative_change(before['stationary']['covered_rgb_rms_change'],after['stationary']['covered_rgb_rms_change']),
                              'stationary_gradient_change_percent':relative_change(before['stationary']['covered_gradient_change'],after['stationary']['covered_gradient_change']),
                              'stationary_detail_ratio':after['stationary']['covered_detail']/before['stationary']['covered_detail'] if before['stationary']['covered_detail'] else None}
        for frame,label in ((125,'stationary-phase17'),(197,'stop18-phase17'),(215,'stop36-phase17')):
            pair=np.concatenate((displayed(x[frame]),displayed(y[frame])),axis=1)
            png(a.output/f'{name}-{label}-baseline-left-candidate-right.png',pair)
            png(a.output/f'{name}-{label}-pane-1to1.png',np.concatenate((pair[18:78,36:94],pair[18:78,228:286]),axis=1))
        for start,end,label in ((108,162,'stationary54'),(162,216,'move18-stop36')):
            dest=a.output/f'{name}-{label}-12fps.mp4'
            cmd=['ffmpeg','-nostdin','-v','error','-f','rawvideo','-pixel_format','rgb24','-video_size','384x96','-framerate','12','-i','pipe:0','-an','-c:v','libx264','-crf','12','-pix_fmt','yuv444p','-movflags','+faststart',str(dest)]
            p=subprocess.Popen(cmd,stdin=subprocess.PIPE)
            try:
                for i in range(start,end):p.stdin.write(np.concatenate((displayed(x[i]),displayed(y[i])),axis=1).tobytes())
                p.stdin.close()
                if p.wait():raise RuntimeError('ffmpeg failed')
            finally:
                if p.poll() is None:p.terminate();p.wait()
    (a.output/'glass-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({name:{'stationary_change_percent':v.get('stationary_change_percent'),'stationary_detail_ratio':v.get('stationary_detail_ratio'),'input_parity':v.get('same_phase_input_parity',{}).get('rgb_max_difference'),'baseline':v.get('baseline',{}).get('stationary'),'candidate':v.get('candidate',{}).get('stationary')} for name,v in result['arms'].items()},indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('baseline','candidate','output'):p.add_argument('--'+name,type=Path,required=True)
    run(p.parse_args())
