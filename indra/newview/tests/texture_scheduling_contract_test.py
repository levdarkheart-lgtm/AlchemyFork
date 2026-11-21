#!/usr/bin/env python3
"""Exercise production cache handoffs/slots and fetch-ready generations with ASan/UBSan."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def block(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == '{':
            depth += 1
        elif source[position] == '}':
            depth -= 1
            if depth == 0:
                return source[start:position + 1]
    raise ValueError(f'Unclosed production body: {signature}')


def generate():
    cache = (ROOT / 'indra/newview/lltexturecache.cpp').read_text()
    fetch = (ROOT / 'indra/newview/lltexturefetch.cpp').read_text()
    texture_list = (ROOT / 'indra/newview/llviewertexturelist.cpp').read_text()
    fixture = (HERE / 'texture_scheduling_fixture.cpp').read_text()
    responders = '\n'.join(block(fetch, 'class ' + name) + ';' for name in ['CacheReadResponder', 'CacheWriteResponder'])
    bodies = [block(cache, signature) for signature in [
        'LLTextureCacheFastResult::LLTextureCacheFastResult(',
        'LLTextureCacheFastResult::~LLTextureCacheFastResult(',
        'std::shared_ptr<LLTextureCacheFastResult> LLTextureCache::readFromFastCacheAsync(',
        'void LLTextureCache::prioritizeWrite(', 'void LLTextureCache::addCompleted(']]
    bodies += [block(fetch, signature) for signature in ['void LLTextureFetch::notifyReady(', 'bool LLTextureFetch::popReadyTexture(', 'bool LLTextureFetchWorker::deleteOK(',
        'void LLTextureFetchWorker::callbackCacheRead(', 'void LLTextureFetchWorker::callbackCacheWrite(']]
    bodies.append(block(texture_list, 'F32 LLViewerTextureList::updateImagesLoadingFastCache('))
    return fixture.replace('// @RESPONDERS@', responders).replace('// @PRODUCTION@', '\n\n'.join(bodies))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--emit-source', type=Path)
    args = parser.parse_args()
    if args.emit_source:
        args.emit_source.write_text(generate())
        return 0

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        source = directory / 'texture_scheduling_actual.cpp'
        binary = directory / 'texture_scheduling_actual'
        source.write_text(generate())
        commands = [(['/usr/bin/c++', '-std=c++20', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                      '-Wno-unused-parameter', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(source), '-o', str(binary)], 'compile'),
                    ([str(binary)], 'run')]
        for command, name in commands:
            result = subprocess.run(command, text=True, capture_output=True)
            (directory / f'{name}.stdout').write_text(result.stdout)
            (directory / f'{name}.stderr').write_text(result.stderr)
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                return result.returncode
        return 0
    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-texture-scheduling-') as temporary:
        return run(Path(temporary))


if __name__ == '__main__':
    raise SystemExit(main())
