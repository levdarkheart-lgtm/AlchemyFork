#!/usr/bin/env python3
"""PPD463 actual HUD-demand branch/metadata body with CPU texture boundaries."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]

def brace_end(source, start):
    brace = source.index('{', start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == '{': depth += 1
        elif source[pos] == '}':
            depth -= 1
            if depth == 0: return pos + 1
    raise ValueError('unbalanced actual body')

def generate(root, revision=None, observer_revision=None):
    path = 'indra/newview/llvovolume.cpp'
    source = subprocess.check_output(['git', '-C', str(root), 'show', f'{revision}:{path}'], text=True) if revision else (root/path).read_text()
    function = source.index('void LLVOVolume::updateTextureVirtualSize(bool forced)')
    begin = source.index('        if (isHUDAttachment())', function)
    hud_end = brace_end(source, begin)
    else_begin = source.index('        else', hud_end)
    branch = source[begin:brace_end(source, else_begin)]
    observer_path = 'indra/newview/llvovolume_hud_texture_inputs.inl'
    observation = subprocess.check_output(['git','-C',str(root),'show',f'{observer_revision}:{observer_path}'],text=True) if observer_revision else (root/observer_path).read_text()
    header = (root/'indra/newview/llviewertexture.h').read_text()
    getters = '\n'.join(header[(start:=header.index(signature)):brace_end(header,start)] for signature in ('S32 getFetchState() const', 'U32 getFetchPriority() const'))
    fixture = (Path(__file__).with_name('hud_texture_inputs_fixture.cpp')).read_text()
    generated = fixture.replace('// @ACTUAL_OBSERVATION@', observation).replace('// @ACTUAL_DEMAND_BRANCH@', branch).replace('// @ACTUAL_FETCH_METADATA_GETTERS@',getters)
    return generated, {'demand_branch':hashlib.sha256(branch.encode()).hexdigest(), 'observation':hashlib.sha256(observation.encode()).hexdigest(), 'fetch_metadata_getters':hashlib.sha256(getters.encode()).hexdigest()}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,default=ROOT);p.add_argument('--revision');p.add_argument('--observer-revision');p.add_argument('--emit-source',type=Path);p.add_argument('--output-dir',type=Path)
    a=p.parse_args();source,hashes=generate(a.source_root,a.revision,a.observer_revision)
    if a.emit_source:
        a.emit_source.parent.mkdir(parents=True,exist_ok=True);a.emit_source.write_text(source);return 0
    out=a.output_dir or Path(tempfile.mkdtemp(prefix='alchemy-ppd463-'));out.mkdir(parents=True,exist_ok=True)
    cpp=out/'actual.cpp';exe=out/'actual';cpp.write_text(source)
    command=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer','-g','-I',str(a.source_root/'indra/newview'),'-I',str(a.source_root/'indra/llcommon'),str(cpp),'-o',str(exe)]
    result=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(result.stdout+result.stderr)
    if result.returncode: print(result.stderr);return result.returncode
    results=[]
    for mode in ('disabled','enabled','exceptions','cap'):
        env=dict(os.environ,ALCHEMY_WARNING_DIAGNOSTICS='0' if mode=='disabled' else '1')
        result=subprocess.run([str(exe),mode],capture_output=True,text=True,env=env)
        (out/(mode+'.log')).write_text(result.stdout+result.stderr)
        results.append({'mode':mode,'exit':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        print(result.stdout+result.stderr,end='')
    receipt={'revision':a.revision or 'working tree','observer_revision':a.observer_revision or 'working tree','source_root':str(a.source_root),'generated_sha256':hashlib.sha256(source.encode()).hexdigest(),'actual_body_sha256':hashes,'compile_command':command,'results':results,'boundaries':'actual HUD branch, observation body and public cached metadata getters; CPU texture/face/boost ownership and logger controlled; no viewer/GPU/network'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 1 if any(item['exit'] for item in results) else 0

if __name__=='__main__': raise SystemExit(main())
