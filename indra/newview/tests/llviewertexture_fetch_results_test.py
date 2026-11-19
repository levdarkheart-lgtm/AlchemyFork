#!/usr/bin/env python3
"""PPD-476: execute whole fetch-result/upload-queue bodies with controlled CPU boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def block(text, start):
    brace = text.index('{', start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise ValueError('Unclosed source block')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    path = 'indra/newview/llviewertexture.cpp'
    viewer = (subprocess.check_output(['git', 'show', f'{args.revision}:{path}'], cwd=ROOT, text=True)
              if args.revision else (ROOT / path).read_text())
    signatures = [
        'void LLViewerFetchedTexture::clearCallbackEntryList()',
        'LLLoadedCallbackEntry::LLLoadedCallbackEntry(',
        'void LLViewerFetchedTexture::setLoadedCallback(',
        'void LLViewerFetchedTexture::updateAuxDemand()',
        'void LLViewerFetchedTexture::pauseLoadedCallbacks(',
        'void LLViewerFetchedTexture::unpauseLoadedCallbacks(',
        'void LLViewerFetchedTexture::postCreateTexture()',
        'bool LLViewerFetchedTexture::needsToSaveRawImage()',
        'void LLViewerFetchedTexture::destroyRawImage()',
        'bool LLViewerFetchedTexture::doLoadedCallbacks()',
        'void LLViewerFetchedTexture::forceToRefetchTexture(',
        'S32 LLViewerFetchedTexture::getCurrentDiscardLevelForFetching()',
        'bool LLViewerFetchedTexture::processFetchResults(',
        'void LLViewerFetchedTexture::addToCreateTexture()',
        'void LLViewerFetchedTexture::scheduleCreateTexture()',
        'void LLViewerFetchedTexture::setIsMissingAsset(',
    ]
    bodies = [block(viewer, viewer.index(signature)) for signature in signatures]
    constructor = next(body for body in bodies if body.startswith('LLLoadedCallbackEntry::'))
    declaration = constructor[:constructor.index(')') + 1].replace('LLLoadedCallbackEntry::', '') + ';'
    generated = ((HERE / 'llviewertexture_fetch_results_fixture.cpp').read_text()
                 .replace('// @ENTRY_DECL@', declaration)
                 .replace('// @PRODUCTION@', '\n\n'.join(bodies)))

    def run(output):
        output.mkdir(parents=True, exist_ok=True)
        source = output / 'actual_fetch_results.cpp'
        executable = output / 'actual_fetch_results'
        source.write_text(generated)
        compile_command = ['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                           '-Wno-reorder-ctor', '-Wno-unused-parameter',
                           '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g',
                           str(source), '-o', str(executable)]
        receipt = {'revision': args.revision or 'working-tree',
                   'source_sha256': hashlib.sha256(viewer.encode()).hexdigest(),
                   'whole_functions': [dict(signature=signature, sha256=hashlib.sha256(body.encode()).hexdigest())
                                       for signature, body in zip(signatures, bodies)],
                   'generated_sha256': hashlib.sha256(generated.encode()).hexdigest(), 'runs': []}
        result = None
        for name, command in [('compile', compile_command), ('run', [str(executable)])]:
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (output / f'{name}.stdout').write_text(result.stdout)
            (output / f'{name}.stderr').write_text(result.stderr)
            receipt['runs'].append(dict(name=name, command=command, exit=result.returncode))
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                break
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return result.returncode

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-fetch-results-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
