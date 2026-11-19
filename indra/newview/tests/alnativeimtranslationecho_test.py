#!/usr/bin/env python3
"""Compile the actual IM send body against packet and local-model captures."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', help='Historical send body; current formatting helper remains under test')
    parser.add_argument('--emit-source', type=Path)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()

    def source(path):
        if args.revision:
            return subprocess.check_output(['git', 'show', f'{args.revision}:{path}'], cwd=ROOT, text=True)
        return (ROOT / path).read_text()

    model = source('indra/newview/llimview.cpp')
    start = model.index('void LLIMModel::sendMessage(')
    # Production member definitions terminate at column zero. This includes
    # nested blocks and string/comment braces without implementing a C++ parser.
    end = model.index('\n}', start) + 2
    method = model[start:end]
    compatibility = False
    if 'local_original' not in method[:method.index('{')]:
        # A baseline can expose the new optional argument without altering its
        # old body, producing behavioral failures rather than a signature error.
        method = method.replace('EInstantMessage dialog)',
                                'EInstantMessage dialog, const std::string& /*local_original*/)', 1)
        compatibility = True
    limits = source('indra/llmessage/lldbstrings.h')
    limit = re.search(r'^const S32 MAX_MSG_STR_LEN\s*=\s*\d+;', limits, re.MULTILINE)
    if not limit:
        raise ValueError('Production IM packet limit was not found')
    fixture = (HERE / 'alnativeimtranslationecho_test.cpp').read_text()
    generated = fixture.replace('// @PRODUCTION_LIMITS@', limit.group()).replace('// @PRODUCTION_SEND_BODY@', method)
    receipt = {
        'revision': args.revision or 'working_tree',
        'source_sha': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'model_sha256': hashlib.sha256(model.encode()).hexdigest(),
        'method_sha256': hashlib.sha256(method.encode()).hexdigest(),
        'helper_sha256': hashlib.sha256((ROOT / 'indra/newview/alnativeimtranslationpolicy.h').read_bytes()).hexdigest(),
        'baseline_signature_adapter_only': compatibility,
        'splitter': 'controlled UTF-8 boundary; actual send-body input and packet output captured',
    }

    def emit(path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(generated)
        path.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')

    if args.emit_source:
        emit(args.emit_source.resolve())
        return 0

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp = directory / 'native_im_echo_actual.cpp'
        binary = directory / 'native_im_echo_actual'
        emit(cpp)
        command = ['clang++', '-std=c++23', '-Wall', '-Wextra', '-Werror',
                   '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                   '-fno-omit-frame-pointer', '-g', '-I', str(ROOT / 'indra/newview'),
                   str(cpp), '-o', str(binary)]
        receipt['compile_command'] = command
        result = subprocess.run(command)
        receipt['compile_exit'] = result.returncode
        if result.returncode:
            cpp.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
            return result.returncode
        result = subprocess.run([str(binary)])
        receipt['run_command'] = [str(binary)]
        receipt['run_exit'] = result.returncode
        cpp.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return result.returncode

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-native-im-echo-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
