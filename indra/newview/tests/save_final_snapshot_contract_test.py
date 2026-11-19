#!/usr/bin/env python3
"""PPD493: actual final-snapshot caller with controlled synthetic CPU boundaries."""
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
    parser.add_argument('--emit-source', type=Path)
    args = parser.parse_args()
    path = 'indra/newview/llappviewer.cpp'
    source = (subprocess.check_output(['git', 'show', f'{args.revision}:{path}'], cwd=ROOT, text=True)
              if args.revision else (ROOT / path).read_text())
    body = block(source, source.index('void LLAppViewer::saveFinalSnapshot('))
    generated = (HERE / 'save_final_snapshot_fixture.cpp').read_text().replace('// @PRODUCTION@', body)
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(generated)
        return 0

    def run(output):
        output.mkdir(parents=True, exist_ok=True)
        cpp, executable = output / 'actual_final_snapshot.cpp', output / 'actual_final_snapshot'
        cpp.write_text(generated)
        command = ['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                   '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g',
                   str(cpp), '-o', str(executable)]
        receipt = {'revision': args.revision or 'working-tree',
                   'body_sha256': hashlib.sha256(body.encode()).hexdigest(),
                   'generated_sha256': hashlib.sha256(generated.encode()).hexdigest(),
                   'boundaries': 'Complete actual caller body; controlled snapshot/file/settings/camera/idle owners; no viewer/GPU/account files',
                   'runs': []}
        result = None
        for name, argv in [('compile', command), ('run', [str(executable)])]:
            result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            (output / f'{name}.stdout').write_text(result.stdout)
            (output / f'{name}.stderr').write_text(result.stderr)
            receipt['runs'].append(dict(name=name, argv=argv, exit=result.returncode))
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                break
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return result.returncode

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-final-snapshot-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
