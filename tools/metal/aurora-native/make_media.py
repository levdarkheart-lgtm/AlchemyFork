#!/usr/bin/env python3
"""Lossless fixture crops and labelled-scope preview clips from retained outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib
import numpy as np


def image(path):
    x=np.fromfile(path,dtype='<f4')
    if x.size != 384*192*4:raise ValueError(f'Wrong output extent: {path}')
    x=x.reshape(192,384,4)[...,:3]
    if not np.isfinite(x).all():raise ValueError(f'Nonfinite output RGB: {path}')
    return np.rint(np.power(np.clip(x,0,1),np.float32(1/2.2))*255).astype(np.uint8)


def png(path,rgb):
    def chunk(name,payload):
        return struct.pack('>I',len(payload))+name+payload+struct.pack('>I',zlib.crc32(name+payload))
    h,w,_=rgb.shape
    rows=b''.join(b'\0'+row.tobytes() for row in rgb)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+
                     chunk(b'tEXt',b'Scope\0Synthetic fixture linear RGB clamped and gamma2.2 for preview; no scoring on preview')+
                     chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b''))


def run(a):
    if a.output.exists():raise ValueError('Keep previous evidence; output must be fresh')
    a.output.mkdir(parents=True)
    def pair(frame):
        m=image(a.metalfx_dir/f'world-{frame:03d}-output.rgba32f')
        n=image(a.native_dir/f'world-{frame:03d}-raw-output.rgba32f')
        return np.concatenate((m,n),axis=1)
    for frame,name in ((377,'stationary-phase17'),(449,'stop18-phase17'),(467,'stop36-phase17')):
        p=pair(frame)
        png(a.output/f'{name}-metalfx-left-native-aurora-right.png',p)
        png(a.output/f'{name}-palm-1to1.png',np.concatenate((p[6:90,198:282],p[6:90,582:666]),axis=1))
    for start,stop,name in ((360,414,'stationary54'),(414,468,'move18-stop36')):
        path=a.output/f'{name}-12fps-metalfx-left-native-aurora-right.mp4'
        cmd=['ffmpeg','-nostdin','-v','error','-f','rawvideo','-pixel_format','rgb24','-video_size','768x192','-framerate','12','-i','pipe:0',
             '-an','-c:v','libx264','-crf','12','-pix_fmt','yuv444p','-movflags','+faststart',str(path)]
        p=subprocess.Popen(cmd,stdin=subprocess.PIPE)
        try:
            for frame in range(start,stop):p.stdin.write(pair(frame).tobytes())
            p.stdin.close()
            if p.wait()!=0:raise RuntimeError('ffmpeg preview failed')
        finally:
            if p.poll() is None:p.terminate();p.wait()
    files=[{'file':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(a.output.iterdir())]
    (a.output/'manifest.json').write_text(json.dumps({'scope':'Synthetic shared perspective MASK fixture, not live scene or viewer FPS.',
        'columns':['retained MetalFX control','native Aurora component OFF'],
        'display_conversion':'linear RGB clamp[0,1], gamma1/2.2, uint8; original float outputs and metrics authoritative',
        'clips':{'playback_fps':12,'worlds_per_clip':54,'playback_seconds':4.5,'note':'12fps is deliberate playback, not renderer timing. Stationary is world361..414. Movement is415..432, stopped433..468.'},
        'files':files},indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('native-dir','metalfx-dir','output'):p.add_argument('--'+name,type=Path,required=True)
    run(p.parse_args())
