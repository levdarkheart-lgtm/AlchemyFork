#!/usr/bin/env python3
"""PPD-471: fault-test actual callback/fetch control bodies with controlled storage/queue inputs."""
import argparse
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent

def block(text,start):
    brace=text.index('{',start);depth=0
    for i in range(brace,len(text)):
        if text[i]=='{':depth+=1
        elif text[i]=='}':
            depth-=1
            if depth==0:return text[start:i+1]
    raise ValueError('Unclosed source block')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--revision');ap.add_argument('--output-dir',type=Path);a=ap.parse_args()
    def source(path):return subprocess.check_output(['git','show',f'{a.revision}:{path}'],cwd=ROOT,text=True) if a.revision else (ROOT/path).read_text()
    viewer=source('indra/newview/llviewertexture.cpp');fetch=source('indra/newview/lltexturefetch.cpp')
    symbols=['void LLViewerFetchedTexture::clearCallbackEntryList()','LLLoadedCallbackEntry::LLLoadedCallbackEntry(','void LLViewerFetchedTexture::setLoadedCallback(','void LLViewerFetchedTexture::updateAuxDemand()','void LLViewerFetchedTexture::pauseLoadedCallbacks(','void LLViewerFetchedTexture::unpauseLoadedCallbacks(','void LLViewerFetchedTexture::postCreateTexture()','bool LLViewerFetchedTexture::needsToSaveRawImage()','void LLViewerFetchedTexture::destroyRawImage()','bool LLViewerFetchedTexture::doLoadedCallbacks()','void LLViewerFetchedTexture::forceToRefetchTexture(','S32 LLViewerFetchedTexture::getCurrentDiscardLevelForFetching()']
    bodies=[]
    for s in symbols:
        bodies.append(block(viewer,viewer.index(s)) if s in viewer else 'void LLViewerFetchedTexture::updateAuxDemand() {}')
    for s in ['void LLTextureFetchWorker::resetFormattedData()','void LLTextureFetchWorker::getStatus(','bool LLTextureFetchWorker::requiresAuxDecode()','void LLTextureFetchWorker::startWork(','void LLTextureFetchWorker::endWork(','void LLTextureFetchWorker::callbackDecoded(']:
        bodies.append(block(fetch,fetch.index(s)) if s in fetch else 'bool LLTextureFetchWorker::requiresAuxDecode() const {return false;}')
    ctor=next(x for x in bodies if x.startswith('LLLoadedCallbackEntry::LLLoadedCallbackEntry('));decl=ctor[:ctor.index(')')+1].replace('LLLoadedCallbackEntry::','')+';'
    url=block(fetch,fetch.index('    void setUrl(')).strip().replace('void setUrl(', 'void LLTextureFetchWorker::setUrl(');bodies.append(url)
    create=fetch.index('S32 LLTextureFetch::createRequest(');admit=block(fetch,fetch.index('    if (worker)\n    {\n        if (worker->wasAborted())',create))
    finished=fetch.index('bool LLTextureFetch::getRequestFinished(');mark='            if (worker->requiresAuxDecode())'
    complete=block(fetch,fetch.index(mark,finished)) if mark in fetch[finished:] else ''
    work=block(fetch,fetch.index('bool LLTextureFetchWorker::doWork('))
    prep_begin=work.index('        mDecoded  = false;',work.index('if (mState == DECODE_IMAGE)'))
    prep_end=work.index('        setState(DECODE_IMAGE_UPDATE);',prep_begin)+len('        setState(DECODE_IMAGE_UPDATE);')
    preparation=work[prep_begin:prep_end]
    upgrade_mark='            if (requiresAuxDecode())'
    upgrade=block(work,work.index(upgrade_mark)) if upgrade_mark in work else ''
    generated=(HERE/'llviewertexture_aux_recovery_fixture.cpp').read_text().replace('// @PRODUCTION@','\n\n'.join(bodies)).replace('// @ENTRY_DECL@',decl).replace('// @ADMIT@',admit).replace('// @COMPLETE@',complete).replace('// @PREPARE@',preparation).replace('// @ADVANCE@',upgrade)
    def run(out):
        out.mkdir(parents=True,exist_ok=True);cpp=out/'actual_aux_recovery.cpp';exe=out/'actual_aux_recovery';cpp.write_text(generated)
        subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-reorder-ctor','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',str(cpp),'-o',str(exe)],check=True)
        return subprocess.run([str(exe)]).returncode
    if a.output_dir:return run(a.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-aux-recovery-') as out:return run(Path(out))
if __name__=='__main__':raise SystemExit(main())
