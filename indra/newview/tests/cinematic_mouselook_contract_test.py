#!/usr/bin/env python3
"""PPD483: actual camera transitions and cinematic/floater visibility owners."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def function(source, signature):
    start = source.index(signature)
    # These selected bodies have no unmatched braces in strings/comments.
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--emit-source', type=Path)
    parser.add_argument('--include-dir', type=Path, action='append', default=[])
    parser.add_argument('--agent-revision', help='Read-only baseline agent-body control from Git')
    args = parser.parse_args()
    view = ROOT / 'indra/newview'
    ui = ROOT / 'indra/llui'
    agent = ((view / 'llagent.cpp').read_text() if not args.agent_revision else
             subprocess.check_output(['git', 'show', args.agent_revision + ':indra/newview/llagent.cpp'],
                                     cwd=ROOT, text=True))
    sections = {
        'MODE': (view / 'alcinematicmode.cpp').read_text().split('bool ALCinematicMode::_enabled', 1)[1],
        'AGENT': function(agent, 'void LLAgent::endAnimationUpdateUI()'),
        'CAMERA': function((view / 'llagentcamera.cpp').read_text(), 'void LLAgentCamera::changeCameraToThirdPerson(bool animate)'),
        'WINDOW': function((view / 'llviewerwindow.cpp').read_text(), 'void LLViewerWindow::setUIVisibility(bool visible)'),
    }
    sections['MODE'] = 'bool ALCinematicMode::_enabled' + sections['MODE']
    floater = (ui / 'llfloater.cpp').read_text()
    for name, signature in [
        ('HIDDEN_CLOSED', 'void LLFloaterView::hiddenFloaterClosed(LLFloater* floater)'),
        ('HIDE', 'void LLFloaterView::hideAllFloaters()'),
        ('SHOW', 'void LLFloaterView::showHiddenFloaters()'),
        ('PUSH_ALL', 'void LLFloaterView::pushVisibleAll(bool visible, const skip_list_t& skip_list)'),
        ('POP_ALL', 'void LLFloaterView::popVisibleAll(const skip_list_t& skip_list)'),
    ]:
        sections[name] = function(floater, signature)
    header = (ui / 'llview.h').read_text()
    sections['PUSH'] = function(header, 'void            pushVisible(bool visible)')
    sections['POP'] = function(header, 'void            popVisible()')
    start = header.index('enum ESavedVisible : U8')
    sections['SAVED'] = header[start:header.index(';', header.index('ESavedVisible mSavedVisible', start)) + 1]
    source = (HERE / 'cinematic_mouselook_contract_fixture.cpp').read_text()
    for name, body in sections.items():
        marker = '// @ACTUAL_' + name + '@'
        assert source.count(marker) == 1, marker
        source = source.replace(marker, body)
    assert '// @ACTUAL_' not in source
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(source)
        return 0

    def run(output):
        output.mkdir(parents=True, exist_ok=True)
        cpp = output / 'cinematic_mouselook_actual.cpp'
        exe = output / 'cinematic_mouselook_actual'
        cpp.write_text(source)
        argv = ['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                '-fno-omit-frame-pointer', '-g', '-I', str(view)]
        for include in args.include_dir:
            argv += ['-isystem', str(include)]
        argv += [str(cpp), '-o', str(exe)]
        results = []
        for command in [argv, [str(exe)]]:
            result = subprocess.run(command, text=True, capture_output=True)
            results.append(dict(argv=command, exit=result.returncode,
                                stdout=result.stdout, stderr=result.stderr))
            print(result.stdout, end='')
            print(result.stderr, end='')
            if result.returncode:
                break
        receipt = dict(sections_sha256={k: hashlib.sha256(v.encode()).hexdigest()
                                      for k, v in sections.items()}, commands=results)
        (output / 'commands.json').write_text(json.dumps(receipt, indent=2) + '\n')
        return results[-1]['exit']
    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-cinematic-mouselook-') as output:
        return run(Path(output))


if __name__ == '__main__':
    raise SystemExit(main())
