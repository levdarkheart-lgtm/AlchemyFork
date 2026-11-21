#!/usr/bin/env python3
"""PPD556: exercise actual cache/fetch/audio admission bodies under ASan/UBSan."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def block(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise ValueError(signature)


def generate():
    cache = (ROOT / 'indra/newview/lltexturecache.cpp').read_text()
    fetch = (ROOT / 'indra/newview/lltexturefetch.cpp').read_text()
    audio = (ROOT / 'indra/llaudio/llaudiodecodemgr.cpp').read_text()
    bodies = [block(cache, signature) for signature in [
        'LLTextureCache::handle_t LLTextureCache::readFromCache(const std::string&',
        'LLTextureCache::handle_t LLTextureCache::readFromCache(const LLUUID&',
        'LLTextureCache::handle_t LLTextureCache::writeToCache(']]
    write = block(fetch, '    if (mState == WRITE_TO_CACHE)')
    bodies.append('bool LLTextureFetchWorker::writeStep(S32 param)\n{\n' + write + '\nreturn false;\n}')
    bodies.append(block(audio, 'bool LLVorbisDecodeState::finishDecode()'))
    return (HERE / 'asset_admission_fixture.cpp').read_text().replace('// @PRODUCTION@', '\n\n'.join(bodies))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--emit-source', type=Path)
    args = parser.parse_args()
    source = generate()
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(source)
        return 0

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp, binary = directory / 'actual_asset_admission.cpp', directory / 'actual_asset_admission'
        cpp.write_text(source)
        commands = [(['/usr/bin/c++', '-std=c++20', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                      '-Wno-unused-parameter', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                      str(cpp), '-o', str(binary)], 'compile'), ([str(binary)], 'run')]
        receipt = dict(generated_sha256=hashlib.sha256(source.encode()).hexdigest(), runs=[])
        for command, name in commands:
            result = subprocess.run(command, text=True, capture_output=True)
            (directory / f'{name}.stdout').write_text(result.stdout)
            (directory / f'{name}.stderr').write_text(result.stderr)
            receipt['runs'].append(dict(command=command, exit=result.returncode))
            (directory / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                return result.returncode
        return 0
    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-admission-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
