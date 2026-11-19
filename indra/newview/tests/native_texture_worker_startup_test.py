#!/usr/bin/env python3
"""PPD487: execute the real window startup call and native worker admission bodies."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def block(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if not depth:
                return source[start:index + 1]
    raise ValueError('unclosed source body')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--emit-source', type=Path)
    args = parser.parse_args()

    def read(name):
        if args.revision:
            return subprocess.check_output(['git', 'show', f'{args.revision}:{name}'], cwd=ROOT, text=True)
        return (ROOT / name).read_text()

    window = read('indra/newview/llviewerwindow.cpp')
    begin = window.index('LLImageGL::initClass(')
    call = window[begin:window.index(';', begin) + 1]
    image = read('indra/llrender/llimagegl.cpp')
    initializer = block(image, 'void LLImageGL::initClass(')
    worker = block(image, 'LLImageGLThread::LLImageGLThread(')
    pool = block(read('indra/llcommon/threadpool.cpp'), 'LL::ThreadPoolBase::ThreadPoolBase(')
    width = re.search(r'mThreadCount\(([^\n]+)\),', pool).group(1)
    defaults = {}
    for name in ['settings.xml', 'settings_alchemy.xml']:
        root = ET.fromstring(read('indra/newview/app_settings/' + name)).find('map')
        entries = list(root)
        for key, value in zip(entries[::2], entries[1::2]):
            if key.text not in {'RenderMetalMultiThreadedTextures', 'RenderGLMultiThreadedTextures',
                                'RenderGLMultiThreadedMedia'} or value.tag != 'map':
                continue
            fields = list(value)
            fields = {k.text: v for k, v in zip(fields[::2], fields[1::2])}
            if fields.get('Type') is not None and fields['Type'].text == 'Boolean':
                defaults[key.text] = int(fields['Value'].text) != 0
    literals = ',\n'.join('{' + json.dumps(name) + ',' + ('true' if value else 'false') + '}'
                          for name, value in sorted(defaults.items()))
    fixture = (HERE / 'native_texture_worker_startup_fixture.cpp').read_text()
    generated = (fixture.replace('// @DEFAULTS@', literals).replace('// @WINDOW_CALL@', call)
                 .replace('// @INITIALIZER@', initializer).replace('// @WORKER@', worker)
                 .replace('/* @WIDTH@ */', width))
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(generated)
        return 0

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp, binary = directory / 'startup.cpp', directory / 'startup'
        cpp.write_text(generated)
        receipt = {'revision': args.revision or 'working-tree',
                   'source_sha256': hashlib.sha256(generated.encode()).hexdigest(), 'runs': []}
        code = 0
        for name, argv in [('compile', ['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                                        '-Wno-unused-parameter',
                                        '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                                        '-fno-omit-frame-pointer', str(cpp), '-o', str(binary)]),
                           ('run', [str(binary)])]:
            result = subprocess.run(argv, capture_output=True, text=True)
            (directory / (name + '.stdout')).write_text(result.stdout)
            (directory / (name + '.stderr')).write_text(result.stderr)
            receipt['runs'].append({'name': name, 'argv': argv, 'exit': result.returncode})
            print(result.stdout, end='')
            print(result.stderr, end='')
            code = result.returncode
            if code:
                break
        (directory / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return code

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-native-worker-startup-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
