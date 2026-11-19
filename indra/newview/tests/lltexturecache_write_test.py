#!/usr/bin/env python3
"""PPD-340/477: fault-test actual cache producer bodies using synthetic files and real LLFile."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
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
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--config', default='Release')
    parser.add_argument('--suite', choices=['body', 'completion', 'all'], default='all')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    source = (subprocess.check_output(['git', 'show', f'{args.revision}:indra/newview/lltexturecache.cpp'], cwd=ROOT, text=True)
              if args.revision else (ROOT / 'indra/newview/lltexturecache.cpp').read_text())
    signatures = ['bool LLTextureCacheRemoteWorker::doWrite()',
                  'void LLTextureCacheWorker::finishWork(', 'void LLTextureCacheWorker::endWork(']
    bodies = [block(source, source.index(signature)) for signature in signatures]
    generated = (HERE / 'lltexturecache_write_fixture.cpp').read_text().replace('// @PRODUCTION@', '\n\n'.join(bodies))
    commands = json.loads((build / 'compile_commands.json').read_text())
    original = next(command for command in commands if command['file'].endswith('/tests/llfile_test.cpp') and f'/{args.config}/' in command['command'])
    lines = (build / f'CMakeFiles/impl-{args.config}.ninja').read_text().splitlines()
    start = next(index for index, line in enumerate(lines) if line.startswith(f'build llcommon/{args.config}/llcommon_tests:'))
    fields = {}
    for line in lines[start + 1:]:
        if not line.startswith(' '):
            break
        if ' = ' in line:
            key, value = line.strip().split(' = ', 1)
            fields[key] = value

    def run(output):
        output.mkdir(parents=True, exist_ok=True)
        cpp, obj, exe = (output / name for name in ['actual_cache_write.cpp', 'actual_cache_write.o', 'actual_cache_write'])
        cpp.write_text(generated)
        compile_command = shlex.split(original['command'])
        compile_command[compile_command.index('-o') + 1] = str(obj)
        compile_command[compile_command.index('-c') + 1] = str(cpp)
        compile_command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        link_command = [compile_command[0]] + shlex.split(fields.get('FLAGS', '')) + shlex.split(fields.get('LINK_FLAGS', ''))
        link_command += [str(obj)] + shlex.split(fields['LINK_LIBRARIES']) + ['-fsanitize=address,undefined', '-o', str(exe)]
        receipt = dict(revision=args.revision or 'working-tree', source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                       functions=[dict(signature=s, sha256=hashlib.sha256(b.encode()).hexdigest()) for s, b in zip(signatures, bodies)],
                       generated_sha256=hashlib.sha256(generated.encode()).hexdigest(), runs=[])
        result = None
        for name, command in [('compile', compile_command), ('link', link_command), ('run', [str(exe), args.suite])]:
            result = subprocess.run(command, cwd=build, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (output / f'{name}.stdout').write_text(result.stdout)
            (output / f'{name}.stderr').write_text(result.stderr)
            receipt['runs'].append(dict(name=name, command=command, cwd=str(build), exit=result.returncode))
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                break
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return result.returncode

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-cache-write-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
