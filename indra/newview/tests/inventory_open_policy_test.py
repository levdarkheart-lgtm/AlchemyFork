#!/usr/bin/env python3
"""PPD474: actual inventory tree/gallery opening bodies under ASan/UBSan."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent

def read(path, revision):
    return (subprocess.check_output(['git','show',f'{revision}:{path}'],cwd=ROOT,text=True)
            if revision else (ROOT/path).read_text())

def body(text, signature):
    start=text.index(signature)
    return text[start:text.index('\n}',start)+2]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--revision');p.add_argument('--output-dir',type=Path);p.add_argument('--skip-xml',action='store_true');p.add_argument('--emit-source',type=Path)
    a=p.parse_args()
    text=read('indra/newview/llinventorybridge.cpp',a.revision)
    functions=[body(text,x) for x in ('void LLObjectBridge::openItem()', 'void LLWearableBridge::openItem()', 'void LLObjectBridgeAction::attachOrDetach()', 'void LLWearableBridgeAction::wearOnAvatar()')]
    cpp=(HERE/'inventory_open_policy_fixture.cpp').read_text().replace('// @PRODUCTION_BODIES@','\n\n'.join(functions))
    if not a.skip_xml:
        panel=ET.fromstring(read('indra/newview/skins/default/xui/en/panel_preferences_interface.xml',a.revision))
        combo=panel.find(".//combo_box[@control_name='InventoryAddAttachmentBehavior']")
        assert combo is not None
        assert {n.get('value') for n in combo.findall('combo_box.item')}=={'0','1'}
        assert not panel.findall(".//*[@control_name='DoubleClickAttachmentAdd']")
        settings=ET.fromstring(read('indra/newview/app_settings/settings_alchemy.xml',a.revision)).find('map')
        children=list(settings);entry=next(children[i+1] for i,c in enumerate(children) if c.tag=='key' and c.text=='DoubleClickWearableAdd')
        values=dict(zip([c.text for c in list(entry)[::2]],[c.text for c in list(entry)[1::2]]))
        assert values['Type']=='Boolean' and values['Value']=='1' and values['Persist']=='1'
    if a.emit_source:
        a.emit_source.parent.mkdir(parents=True,exist_ok=True)
        a.emit_source.write_text(cpp)
        return 0
    def run(out):
        out.mkdir(parents=True,exist_ok=True);source=out/'inventory_open_actual.cpp';binary=out/'inventory_open_actual';source.write_text(cpp)
        subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer','-g',str(source),'-o',str(binary)],check=True)
        return subprocess.run([str(binary)]).returncode
    if a.output_dir:return run(a.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-inventory-policy-') as d:return run(Path(d))
if __name__=='__main__':raise SystemExit(main())
