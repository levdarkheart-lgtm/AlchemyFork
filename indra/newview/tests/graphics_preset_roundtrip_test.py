#!/usr/bin/env python3
"""Generate actual preset save/load/watch bodies over real control/XML services."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def block(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for index in range(opening, len(source)):
        depth += (source[index] == '{') - (source[index] == '}')
        if depth == 0:
            return source[start:index + 1]
    raise ValueError('unclosed production body: ' + signature)


def generate(revision=None):
    def read(name):
        if revision:
            return subprocess.check_output(['git', 'show', f'{revision}:{name}'], cwd=ROOT, text=True)
        return (ROOT / name).read_text()
    source = read('indra/newview/llpresetsmanager.cpp')
    signatures = (
        'std::string LLPresetsManager::getPresetsDir(',
        'void LLPresetsManager::getGraphicsControlNames(',
        'void LLPresetsManager::startWatching(',
        'void LLPresetsManager::graphicsSettingChanged(',
        'bool LLPresetsManager::savePreset(',
        'void LLPresetsManager::loadPreset(',
        'bool LLPresetsManager::isDefaultCameraPreset(',
        'bool LLPresetsManager::isTemplateCameraPreset(',
        'LLPresetsManager::~LLPresetsManager()',
    )
    methods = '\n\n'.join(block(source, name) for name in signatures)
    header = read('indra/newview/llpresetsmanager.h')
    constants = '\n'.join(re.findall(r'^static const std::string [A-Z_]+ = .*?;', header, re.MULTILINE))
    camera_enum = block(read('indra/newview/llagentcamera.h'), 'enum ECameraPreset') + ';'
    fixture = (HERE / 'graphics_preset_roundtrip_fixture.cpp').read_text()
    generated = fixture.replace('// @PRESET_CONSTANTS@', constants)
    generated = generated.replace('// @CAMERA_PRESET_ENUM@', camera_enum)
    generated = generated.replace('// @PRODUCTION_METHODS@', methods)
    return generated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emit-source', required=True, type=Path)
    parser.add_argument('--revision', help='Optional historical source for discrimination')
    args = parser.parse_args()
    generated = generate(args.revision)
    args.emit_source.parent.mkdir(parents=True, exist_ok=True)
    args.emit_source.write_text(generated)
    print(json.dumps({'source': str(args.emit_source), 'revision': args.revision or 'working-tree',
        'sha256': hashlib.sha256(generated.encode()).hexdigest(),
        'services': 'real LLControlGroup/LLSD serialization and actual preset-manager method bodies'}))


if __name__ == '__main__':
    main()
