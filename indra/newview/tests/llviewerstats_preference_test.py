#!/usr/bin/env python3
"""PPD491: actual telemetry tail and helper, synthetic data, real LLSD serializers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import xml.etree.ElementTree as ET

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
    parser.add_argument('--revision', help='Consumer baseline; helper remains the reviewed current helper')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--config', default='Release')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--emit-source', type=Path)
    args = parser.parse_args()
    relative = 'indra/newview/llviewerstats.cpp'
    source = (subprocess.check_output(['git', 'show', f'{args.revision}:{relative}'], cwd=ROOT, text=True)
              if args.revision else (ROOT / relative).read_text())
    consumer = block(source, source.index('void send_viewer_stats('))
    tail = consumer[consumer.index('    // Preferences'):consumer.rindex('}')]
    add = block(source, source.index('void LLViewerStats::addToMessage('))
    control_source = (ROOT / 'indra/llxml/llcontrol.cpp').read_text()
    as_llsd = block(control_source, control_source.index('LLSD LLControlGroup::asLLSD('))
    helper = (ROOT / 'indra/newview/alsafepreferencetelemetry.h').read_text()
    fields = re.findall(r'\{"(\w+)", Type::(\w+)\}', helper)
    assert len(fields) == 24, 'Review explicit telemetry schema changes'

    # Source defaults only: validate that every admitted name is a real control
    # of the intended declared type. Never inspect account files or log values.
    declared = {}
    for filename in ['settings.xml', 'settings_alchemy.xml']:
        tree = ET.parse(ROOT / 'indra/newview/app_settings' / filename)
        nodes = list(tree.getroot().find('map'))
        for key, value in zip(nodes[::2], nodes[1::2]):
            children = list(value)
            metadata = dict(zip([node.text for node in children[::2]], children[1::2]))
            if 'Type' in metadata:
                declared[key.text] = metadata['Type'].text
    types = {'boolean': 'Boolean', 'unsigned_integer': 'U32', 'integer': 'S32', 'real': 'F32'}
    values, assertions = [], []
    for name, kind in fields:
        assert declared.get(name) == types[kind], f'Declaration mismatch: {name}'
        literal = {'boolean': 'true', 'unsigned_integer': '4', 'integer': '-1', 'real': '1.25'}[kind]
        values.append(f'    gSavedSettings.diffs["{name}"]={literal};')
        assertions.append(f'    CHECK(sinks.post["preferences"]["settings"].has("{name}"));')
        assertions.append(f'    CHECK(xml(sinks.post["preferences"]["settings"]["{name}"])==xml(LLSD({literal})));')
    generated = ((HERE / 'llviewerstats_preference_fixture.cpp').read_text()
                 .replace('// @ADD_TO_MESSAGE@', add)
                 .replace('// @AS_LLSD@', as_llsd)
                 .replace('// @PRODUCTION_TAIL@', tail)
                 .replace('// @SAFE_VALUES@', '\n'.join(values))
                 .replace('// @SAFE_ASSERTIONS@', '\n'.join(assertions)))
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(generated)
        return 0
    if not args.build_dir:
        parser.error('--build-dir is required unless --emit-source is used')
    build = args.build_dir.resolve()
    commands = json.loads((build / 'compile_commands.json').read_text())
    original = next(command for command in commands if command['file'].endswith('/tests/llsdserialize_test.cpp')
                    and f'/{args.config}/' in command['command'])
    lines = (build / f'CMakeFiles/impl-{args.config}.ninja').read_text().splitlines()
    start = next(index for index, line in enumerate(lines)
                 if line.startswith(f'build llcommon/{args.config}/llcommon_tests:'))
    fields = {}
    for line in lines[start + 1:]:
        if not line.startswith(' '):
            break
        if ' = ' in line:
            key, value = line.strip().split(' = ', 1)
            fields[key] = value

    def run(output):
        output.mkdir(parents=True, exist_ok=True)
        cpp, obj, exe = [output / name for name in ['actual_preference.cpp', 'actual_preference.o', 'actual_preference']]
        cpp.write_text(generated)
        command = shlex.split(original['command'])
        command[command.index('-o') + 1] = str(obj)
        command[command.index('-c') + 1] = str(cpp)
        command += ['-I', str(ROOT / 'indra/newview'), '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        link = ([command[0]] + shlex.split(fields.get('FLAGS', '')) + shlex.split(fields.get('LINK_FLAGS', ''))
                + [str(obj)] + shlex.split(fields['LINK_LIBRARIES'])
                + ['-fsanitize=address,undefined', '-o', str(exe)])
        receipt = dict(revision=args.revision or 'working-tree', source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                       consumer_sha256=hashlib.sha256(consumer.encode()).hexdigest(),
                       tail_sha256=hashlib.sha256(tail.encode()).hexdigest(),
                       add_to_message_sha256=hashlib.sha256(add.encode()).hexdigest(),
                       control_as_llsd_sha256=hashlib.sha256(as_llsd.encode()).hexdigest(),
                       helper_sha256=hashlib.sha256(helper.encode()).hexdigest(),
                       generated_sha256=hashlib.sha256(generated.encode()).hexdigest(),
                       boundaries='Actual consumer tail/addToMessage/helper/control exporter and real LLSD/XML; controlled storage metadata/log/file/HTTP/recording',
                       runs=[])
        result = None
        for name, argv in [('compile', command), ('link', link), ('run', [str(exe)])]:
            result = subprocess.run(argv, cwd=build, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (output / f'{name}.stdout').write_text(result.stdout)
            (output / f'{name}.stderr').write_text(result.stderr)
            receipt['runs'].append(dict(name=name, argv=argv, cwd=str(build), exit=result.returncode))
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                break
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return result.returncode

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-preference-telemetry-') as directory:
        return run(Path(directory))


if __name__ == '__main__':
    raise SystemExit(main())
