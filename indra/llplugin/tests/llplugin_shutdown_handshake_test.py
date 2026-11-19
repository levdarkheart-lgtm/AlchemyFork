#!/usr/bin/env python3
"""PPD481 actual parent protocol bodies with controlled IPC/event/owner boundaries.

--emit-source is independent of compiler/build metadata for normal CMake use.
No sockets, CEF process, viewer, active account, or network are used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def body(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    # All selected production functions have balanced braces in their comments/strings.
    for pos in range(brace, len(text)):
        if text[pos] == '{':
            depth += 1
        elif text[pos] == '}':
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    raise ValueError(signature)


def generate(root, revision=None):
    rel = 'indra/llplugin/llpluginprocessparent.cpp'
    header_rel = 'indra/llplugin/llpluginprocessparent.h'
    def read(path):
        if revision:
            return subprocess.check_output(['git', '-C', str(root), 'show', f'{revision}:{path}'], text=True)
        return (root / path).read_text()
    source = read(rel)
    header = read(header_rel)
    blocks = {}
    for name, signature in {
        'requestShutdown': 'void LLPluginProcessParent::requestShutdown()',
        'receiveMessageEarly': 'void LLPluginProcessParent::receiveMessageEarly(',
        'receiveMessage': 'void LLPluginProcessParent::receiveMessage(',
        'errorState': 'void LLPluginProcessParent::errorState(',
    }.items():
        blocks[name] = body(source, signature)
    enum_start = header.index('    enum EState')
    enum_end = header.index('    EState mState;', enum_start)
    blocks['states'] = header[enum_start:enum_end]
    idle = body(source, 'void LLPluginProcessParent::idle(void)')
    blocks['queue_drain'] = idle[idle.index('        // process queued messages'):idle.index('        // Give time to network processing')]
    blocks['parent_cases'] = idle[idle.index('            case STATE_LAUNCHED:'):idle.index('\n        }\n\n    } while')]
    child = read('indra/llplugin/llpluginprocesschild.cpp')
    child_header = read('indra/llplugin/llpluginprocesschild.h')
    blocks['child_idle'] = body(child, 'void LLPluginProcessChild::idle(void)')
    child_enum = child_header.index('    enum EState')
    blocks['child_states'] = child_header[child_enum:child_header.index('    void setState(', child_enum)]
    # The actual socket dispatcher branch establishes the state before the whole
    # child idle body executes; serialization/network I/O is a controlled boundary.
    shutdown = child.index('else if (message_name == "shutdown_plugin")')
    blocks['child_shutdown_dispatch'] = body(child, 'else if (message_name == "shutdown_plugin")')[body(child, 'else if (message_name == "shutdown_plugin")').index('{'):]
    if shutdown < child.index('void LLPluginProcessChild::receiveMessageRaw('):
        raise ValueError('wrong child dispatcher')
    # These exact cases contain platform-independent state transitions and controlled
    # LLProcess/socket/liveness boundary calls. Do not substitute a copied policy.
    template = (root / 'indra/llplugin/tests/llplugin_shutdown_handshake_fixture.cpp').read_text()
    rendered = template
    for name, block in blocks.items():
        rendered = rendered.replace('@' + name + '@', block)
    if '@' in rendered:
        raise ValueError('unreplaced fixture token')
    return rendered, {key: hashlib.sha256(value.encode()).hexdigest() for key, value in blocks.items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--revision')
    parser.add_argument('--emit-source', type=Path)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--compiler', default='clang++')
    args = parser.parse_args()
    generated, hashes = generate(args.source_root, args.revision)
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(generated)
        return 0
    out = args.output_dir or Path(tempfile.mkdtemp(prefix='alchemy-ppd481-'))
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / 'actual.cpp'
    cpp.write_text(generated)
    binary = out / 'actual-test'
    command = [args.compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(cpp), '-o', str(binary)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (out / 'compile.log').write_text(compiled.stdout + compiled.stderr)
    if compiled.returncode:
        print(compiled.stderr)
        return compiled.returncode
    tested = subprocess.run([str(binary)], capture_output=True, text=True)
    (out / 'test.log').write_text(tested.stdout + tested.stderr)
    receipt = {'source_root': str(args.source_root), 'revision': args.revision or 'working tree', 'body_sha256': hashes, 'generated_sha256': hashlib.sha256(generated.encode()).hexdigest(), 'compile_command': command, 'compile_exit': compiled.returncode, 'test_exit': tested.returncode, 'stdout': tested.stdout, 'stderr': tested.stderr, 'boundaries': 'controlled owner, process/socket/liveness, message storage, mutex and event listener; no actual IPC/CEF lifetime'}
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(tested.stdout + tested.stderr, end='')
    return tested.returncode


if __name__ == '__main__':
    raise SystemExit(main())
