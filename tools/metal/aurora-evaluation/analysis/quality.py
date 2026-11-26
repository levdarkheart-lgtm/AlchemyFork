#!/usr/bin/env python3
"""Offline, same-output-grid reconstruction scoring. Never executes a renderer."""
from __future__ import annotations
import argparse,csv,hashlib,json,math,platform
from pathlib import Path
import numpy as np

F32=np.float32

def image(path:Path,width:int,height:int,channels:int=4)->np.ndarray:
    data=np.fromfile(path,dtype='<f4')
    if data.size!=width*height*channels:raise ValueError(f'{path.name}: wrong scalar count')
    out=data.reshape(height,width,channels) if channels>1 else data.reshape(height,width)
    checked=out[...,:3] if channels==4 else out
    if not np.isfinite(checked).all():raise ValueError(f'{path.name}: nonfinite reconstructed/reference RGB values')
    return out

def identity(path:Path)->dict:
    return {'name':path.name,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}

def leaf(rgb:np.ndarray)->np.ndarray:
    return (rgb[...,1]>rgb[...,0]*F32(1.4))&(rgb[...,1]>rgb[...,2]*F32(1.4))

def dilate(values:np.ndarray,radius:int)->np.ndarray:
    """Chebyshev maximum neighborhood, with zero outside the retained image."""
    h,w=values.shape;pad=np.pad(values,radius);out=np.zeros_like(values)
    for dy in range(2*radius+1):
        for dx in range(2*radius+1):out=np.maximum(out,pad[dy:dy+h,dx:dx+w])
    return out

def rgb_delta(a:np.ndarray,b:np.ndarray)->np.ndarray:
    d=np.abs(a[...,:3]-b[...,:3]);return ((d[...,0]+d[...,1])+d[...,2])/F32(3)

def p95(values)->float:
    v=sorted(values)
    return float(v[math.ceil(len(v)*.95)-1]) if v else 0.

def coverage(image_rgb:np.ndarray,foreground:np.ndarray,background:np.ndarray)->np.ndarray:
    numerator=np.zeros(image_rgb.shape[:2],dtype=np.float32);denominator=np.zeros_like(numerator)
    for c in range(3):
        d=foreground[...,c]-background[c];numerator+=(image_rgb[...,c]-background[c])*d;denominator+=d*d
    out=np.zeros_like(numerator);np.divide(numerator,denominator,out=out,where=denominator>F32(1e-6))
    return np.clip(out,0,1)

def mask_quality(truth:np.ndarray,actual:np.ndarray,roi)->dict:
    t=truth[roi];a=actual[roi];expected=float(t.sum(dtype=np.float64));covered=float(a.sum(dtype=np.float64))
    recall=float(np.minimum(t,dilate(actual,1)[roi]).sum(dtype=np.float64))/expected if expected else 1.
    precision=float(np.minimum(a,dilate(truth,1)[roi]).sum(dtype=np.float64))/covered if covered else 1.
    return {'recall':recall,'precision':precision,'area':covered/expected if expected else 1.}

def edge_mask(native:np.ndarray,roi)->np.ndarray:
    mask=leaf(native);lo=np.minimum(mask.astype(np.float32),1-dilate(1-mask.astype(np.float32),2));hi=dilate(mask.astype(np.float32),2)
    return (lo!=hi)[roi]

def edge_energy(rgb:np.ndarray,roi)->float:
    lum=(rgb[...,0]*F32(.2126)+rgb[...,1]*F32(.7152))+rgb[...,2]*F32(.0722)
    sy,sx=roi;right=(sy,slice(sx.start+1,sx.stop+1));down=(slice(sy.start+1,sy.stop+1),sx)
    return float((np.abs(lum[roi]-lum[right]).sum(dtype=np.float64)+np.abs(lum[roi]-lum[down]).sum(dtype=np.float64))/lum[roi].size)

def stop_quality(initial_native,final_native,initial_warm,final_warm,current,roi)->dict:
    # Match C++'s per-component division then float32 accumulation for the changed-pixel oracle.
    d=np.zeros(initial_native.shape[:2],np.float32)
    for c in range(3):d+=np.abs(initial_native[...,c]-final_native[...,c])/F32(3)
    changed=d[roi]>F32(.05)
    native_error=np.zeros_like(d);phase_error=np.zeros_like(d)
    for c in range(3):
        native_error+=np.abs(current[...,c]-final_native[...,c])/F32(3)
        phase_error+=np.abs(current[...,c]-final_warm[...,c])/F32(3)
    old_only=leaf(initial_native)&~dilate(leaf(final_native).astype(np.float32),1).astype(bool)
    numerator=np.zeros_like(d);denominator=np.zeros_like(d)
    for c in range(3):
        old=initial_warm[...,c]-final_warm[...,c];numerator+=(current[...,c]-final_warm[...,c])*old;denominator+=old*old
    valid=(old_only&(denominator>F32(.0004)))[roi];residue=np.zeros_like(d)
    np.divide(numerator,denominator,out=residue,where=denominator>F32(.0004));residue=np.clip(residue,0,1)[roi][valid]
    return {'changed_pixels':int(changed.sum()),'native_quality_error':float(native_error[roi][changed].mean(dtype=np.float64)) if changed.any() else 0.,
            'phase_matched_error':float(phase_error[roi][changed].mean(dtype=np.float64)) if changed.any() else 0.,
            'disoccluded_valid_pixels':int(valid.sum()),'old_history_fraction':float(residue.mean(dtype=np.float64)) if residue.size else 0.,
            'localized_trail_review_only':{'residue_p95':float(np.quantile(residue,.95)) if residue.size else 0.,'residue_peak':float(residue.max()) if residue.size else 0.,'fraction_over_point1':float((residue>F32(.1)).mean()) if residue.size else 0.}}

def artifacts(rgb,foreground,background,truth_coverage,roi)->dict:
    # The fixture is constant-color foreground/background. These are diagnostics, not new gates.
    fg=foreground[roi][...,:3];bg=background[:3];z=rgb[roi][...,:3]
    excess=np.maximum(np.maximum(z-np.maximum(fg,bg),np.minimum(fg,bg)-z),0)
    outside=(dilate((truth_coverage>0).astype(np.float32),2)==0)[roi];residual=z-bg
    direction=fg-bg;den=(direction*direction).sum(axis=-1);projection=np.zeros(den.shape,np.float32)
    np.divide((residual*direction).sum(axis=-1),den,out=projection,where=den>1e-6)
    e=np.max(np.abs(residual),axis=-1)[outside];signal=projection[outside]
    return {'review_only':True,'peak_channel_overshoot':float(excess.max()),'mean_channel_overshoot':float(excess.mean(dtype=np.float64)),
            'fraction_pixels_overshoot_over_2_255':float((np.max(excess,axis=-1)>F32(2/255)).mean()),'outside_two_pixel_silhouette_pixels':int(outside.sum()),
            'halo_peak_background_residual':float(e.max()) if e.size else 0.,'halo_p95_background_residual':float(np.quantile(e,.95)) if e.size else 0.,
            'halo_fraction_background_residual_over_2_255':float((e>F32(2/255)).mean()) if e.size else 0.,
            'false_coverage_positive_peak':float(np.maximum(signal,0).max()) if signal.size else 0.,'negative_coverage_peak':float(np.maximum(-signal,0).max()) if signal.size else 0.}

def csv_rows(path):
    with path.open() as handle:rows=list(csv.DictReader(handle))
    if [int(r['frame']) for r in rows]!=list(range(len(rows))):raise ValueError(f'{path.name}: incomplete/noncontiguous frames')
    if [int(r['reset']) for r in rows]!=[1]+[0]*(len(rows)-1):raise ValueError(f'{path.name}: unexpected reset sequence')
    return rows

def reference_index(rows,phase_count,phase):
    index=len(rows)-phase_count+phase
    if index<0 or index>=len(rows) or int(rows[index]['phase'])!=phase:raise ValueError('Wrong phase-matched last-cycle reference')
    return index

def cycle_drift(load,start_a,start_b,count,rows,roi,changed,bg):
    pairs=[]
    for offset in range(count):
        a=start_a+offset;b=start_b+offset
        if rows[a]['phase']!=rows[b]['phase']:raise ValueError('Maturity diagnostic phase mismatch')
        x=load(a);y=load(b);d=rgb_delta(x,y)[roi]
        pairs.append({'before_frame_index':a,'after_frame_index':b,'phase':int(rows[a]['phase']),
                      'mean_rgb_error':float(d.mean(dtype=np.float64)),'changed_region_mean_rgb_error':float(d[changed].mean(dtype=np.float64)) if changed.any() else 0.,
                      'before_green_contrast':float((x[roi][...,1]-bg[1]).mean(dtype=np.float64)),'after_green_contrast':float((y[roi][...,1]-bg[1]).mean(dtype=np.float64))})
    return {'review_only':True,'pairs':pairs,'mean_phase_matched_rgb_error':float(np.mean([r['mean_rgb_error'] for r in pairs])),
            'maximum_phase_matched_rgb_error':max(r['mean_rgb_error'] for r in pairs),'mean_changed_region_phase_error':float(np.mean([r['changed_region_mean_rgb_error'] for r in pairs]))}

def score(args)->dict:
    manifest=json.loads(args.manifest.read_text());D=args.manifest.parent
    if 'perspective' not in manifest.get('projection','').lower() or not manifest.get('depth_reversed'):raise ValueError('Only the new shared reverse-Z perspective population is valid')
    w,h=manifest['output_width'],manifest['output_height'];iw,ih=manifest['input_width'],manifest['input_height'];phase_count=manifest['phase_count'];warm=manifest['warm_frames'];stable=manifest['stable_frames']
    if (iw,ih,w,h)!=(256,128,384,192):raise ValueError('Reducer is pinned to the shared256x128-to384x192 perspective palm fixture')
    main=csv_rows(D/'sequence.csv');refs=csv_rows(D/'reference-sequence.csv');expected=warm+stable+manifest['moving_frames']+manifest['stopped_frames']
    if len(main)!=expected or len(refs)!=manifest['reference_frames']:raise ValueError('Incomplete retained histories')
    if any(int(r['phase'])!=int(r['frame'])%phase_count for r in main+refs):raise ValueError('Recorded phase sequence differs')
    native=image(D/'native-detail-reference.rgba32f',w,h);final_native=image(D/'shifted-native-detail-reference.rgba32f',w,h)
    truth=image(D/'coverage-reference.r32f',w,h,1);fg=image(D/'coverage-foreground.rgba32f',w,h);backgrounds=np.fromfile(D/'coverage-backgrounds.rgba32f',dtype='<f4').reshape(-1,4);bg=backgrounds[2]
    roi=(slice(6,90),slice(198,282));edges=edge_mask(native,roi);reference_energy=edge_energy(native,roi)
    def load(frame,reference=False):
        pattern=args.reference_pattern if reference else args.main_pattern;directory=args.reference_dir if reference else args.main_dir
        return image(directory/pattern.format(frame=frame),w,h)
    # Fail before scoring if any world/reference is absent, including warm-up.
    for reference,rows in [(False,main),(True,refs)]:
        directory=args.reference_dir if reference else args.main_dir;pattern=args.reference_pattern if reference else args.main_pattern
        for r in rows:
            path=directory/pattern.format(frame=int(r['frame']))
            if not path.is_file() or path.stat().st_size!=w*h*16:raise ValueError(f'Missing/wrong-sized output: {path.name}')
    summaries=[];metric_rows=[];quality=[];energy=[];native_errors=[];coverage_changes=[];diagnostics=[];contrast=[];initial={}
    prev=load(warm-1)
    for i in range(warm,warm+stable):
        cur=load(i);delta=rgb_delta(cur,prev)[roi];qualities=mask_quality(truth,coverage(cur,fg,bg),roi)
        variation=float(delta.mean(dtype=np.float64));edge_variation=float(delta[edges].mean(dtype=np.float64));metric_rows.append({'frame_index':i,'world_id':int(main[i]['world_id']),'phase':int(main[i]['phase']),'mean_rgb_change':variation,'edge_band_change':edge_variation})
        summaries.append((variation,edge_variation));quality.append(qualities);energy.append(edge_energy(cur,roi));native_errors.append(float(np.abs(cur[roi][...,:3]-native[roi][...,:3]).mean(dtype=np.float64)))
        toggles=(leaf(cur)!=leaf(prev))&(np.abs(cur[...,1]-prev[...,1])>F32(.05));coverage_changes.append(float(toggles[roi].mean()))
        diagnostics.append(artifacts(cur,fg,bg,truth,roi));inp=image(D/f'world-{i:03d}-input.rgba32f',iw,ih)
        # This comparison has fixed2/3 dimensions: the84pxROI maps to56whole input pixels.
        sy=slice(6*ih//h,90*ih//h);sx=slice(198*iw//w,282*iw//w);contrast.append(float((cur[roi][...,1]-bg[1]).mean(dtype=np.float64)/(inp[sy,sx,1]-bg[1]).mean(dtype=np.float64)))
        if i<warm+phase_count:initial[int(main[i]['phase'])]=cur
        prev=cur
    first_stop=warm+stable+manifest['moving_frames'];stops={}
    for count in [1,18,36]:
        index=first_stop+count-1;phase=int(main[index]['phase']);refindex=reference_index(refs,phase_count,phase)
        if int(refs[refindex]['phase'])!=phase:raise ValueError('Wrong phase-matched reference')
        stops[str(count)]={'frame_index':index,'world_id':int(main[index]['world_id']),'phase':phase,'reference_frame_index':refindex,'reference_world_id':int(refs[refindex]['world_id']),**stop_quality(native,final_native,initial[phase],load(refindex,True),load(index),roi)}
    metrics={'mean_rgb_change':float(np.mean([v[0] for v in summaries])),'edge_band_change':float(np.mean([v[1] for v in summaries])),'edge_band_p95':p95([v[1] for v in summaries]),'coverage_change':float(np.mean(coverage_changes)),
      'mean_reference_error':float(np.mean(native_errors)),'minimum_recall':min(v['recall'] for v in quality),'minimum_precision':min(v['precision'] for v in quality),'minimum_area':min(v['area'] for v in quality),'maximum_area':max(v['area'] for v in quality),'edge_energy':float(np.mean(energy)),'reference_edge_energy':reference_energy,'mean_signed_green_contrast_over_actual_input':float(np.mean(contrast))}
    bounds={'mean_rgb_change':('<=',1/255),'edge_band_change':('<=',1/255),'edge_band_p95':('<=',2/255),'coverage_change':('<=',.005),'mean_reference_error':('<=',.12),'minimum_recall':('>=',.90),'minimum_precision':('>=',.90),'minimum_area':('>=',.85),'maximum_area':('<=',1.15),'edge_energy':('>=',.5*reference_energy)}
    checks={k:{'observed':metrics[k],'operator':op,'bound':limit,'passed':metrics[k]<=limit if op=='<=' else metrics[k]>=limit} for k,(op,limit) in bounds.items()}
    for count in ['18','36']:
        for field,bound in [('phase_matched_error',1/255),('old_history_fraction',.02)]:
            checks[f'stop{count}_{field}']={'observed':stops[count][field],'operator':'<=','bound':bound,'passed':stops[count][field]<=bound}
    changed_mask=np.zeros(native.shape[:2],np.float32)
    for c in range(3):changed_mask+=np.abs(native[...,c]-final_native[...,c])/F32(3)
    changed_mask=changed_mask[roi]>F32(.05)
    maturity={'definition':'Existing output only; last two full reference cycles and each consecutive measured main cycle, no new hard quality bound.',
      'reference_last_two_cycles':cycle_drift(lambda i:load(i,True),len(refs)-2*phase_count,len(refs)-phase_count,phase_count,refs,roi,changed_mask,bg),
      'main_stationary_cycle_pairs':[cycle_drift(load,start,start+phase_count,phase_count,main,roi,changed_mask,bg) for start in range(warm,warm+stable-phase_count,phase_count)]}
    result={'schema_version':1,'label':args.label,'scope':'New shared reverse-Z perspective fixture only; no legacy orthographic score substitution. Original bounds unchanged. New artifact indicators are review-only, not numerical gates. No renderer/GPU execution by this reducer.',
      'inputs':{'manifest':identity(args.manifest),'sequence':identity(D/'sequence.csv'),'reference_sequence':identity(D/'reference-sequence.csv'),'quality_protocol':identity(Path(__file__).with_name('quality_protocol.json'))},
      'runtime':{'python':platform.python_version(),'numpy':np.__version__},'dimensions':{'input':[iw,ih],'output':[w,h]},'history':{'main_worlds':len(main),'reference_worlds':len(refs),'phase_count':phase_count,'warm_frames':warm,'stable_frames':stable,'adjacent_pairs':stable},'metrics':metrics,'original_quality_checks':checks,'original_failed_bounds':[k for k,v in checks.items() if not v['passed']],
      'stop_samples':stops,'maturity_diagnostics_review_only':maturity,'artifact_diagnostics_review_only':{'maximum_peak_channel_overshoot':max(v['peak_channel_overshoot'] for v in diagnostics),'maximum_fraction_pixels_overshoot_over_2_255':max(v['fraction_pixels_overshoot_over_2_255'] for v in diagnostics),'maximum_halo_peak_background_residual':max(v['halo_peak_background_residual'] for v in diagnostics),'maximum_halo_fraction_background_residual_over_2_255':max(v['halo_fraction_background_residual_over_2_255'] for v in diagnostics),'maximum_false_coverage_positive_peak':max(v['false_coverage_positive_peak'] for v in diagnostics),'maximum_negative_coverage_peak':max(v['negative_coverage_peak'] for v in diagnostics)},'frame_metrics':metric_rows}
    if args.receipt:result['execution_receipt_identity']=identity(args.receipt)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n');return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--main-dir',type=Path);p.add_argument('--reference-dir',type=Path);p.add_argument('--main-pattern',default='world-{frame:03d}-output.rgba32f');p.add_argument('--reference-pattern',default='reference-world-{frame:03d}-output.rgba32f');p.add_argument('--label',required=True);p.add_argument('--receipt',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.main_dir=a.main_dir or a.manifest.parent;a.reference_dir=a.reference_dir or a.manifest.parent
    r=score(a);print(json.dumps({'label':r['label'],'metrics':r['metrics'],'original_failed_bounds':r['original_failed_bounds'],'artifact_diagnostics_review_only':r['artifact_diagnostics_review_only']},indent=2))
if __name__=='__main__':main()
