#!/usr/bin/env python3
"""PPD474 actual cinematic transition and hover-text consumer with UI-owner boundaries."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emit-source',type=Path);p.add_argument('--output-dir',type=Path);p.add_argument('--test-mode', choices=['all', 'movement'], default='all');a=p.parse_args()
    view=ROOT/'indra/newview'
    mode=(view/'alcinematicmode.cpp').read_text();mode=mode[mode.index('bool ALCinematicMode::_enabled'):]
    hover=(view/'llhudtext.cpp').read_text();start=hover.index('void LLHUDText::render()');hover=hover[start:hover.index('\n}',start)+2]
    source=(HERE/'cinematic_mode_contract_fixture.cpp').read_text().replace('// @ACTUAL_MODE@',mode).replace('// @ACTUAL_HOVER@',hover)
    movement=(view/'llmoveview.cpp').read_text()
    for name,signature in [('MOVEMENT_MODE','void LLPanelStandStopFlying::setStandStopFlyingMode(EStandStopFlyingMode mode)'),('MOVEMENT_VISIBLE','void LLPanelStandStopFlying::setVisible(bool visible)')]:
        start=movement.index(signature);end=movement.index('\n}',start)+2
        source=source.replace('// @ACTUAL_'+name+'@',movement[start:end])
    panel=ET.parse(view/'skins/default/xui/en/panel_preferences_interface.xml').getroot()
    binding=panel.find(".//check_box[@control_name='AlchemyCinematicModeHideHoverText']");assert binding is not None and binding.get('initial_value')=='false'
    menu=ET.parse(view/'skins/default/xui/en/menu_viewer.xml').getroot();entry=menu.find(".//menu_item_call[@name='CinematicMode']")
    assert entry is not None and entry.get('shortcut')=='alt|shift|c'
    assert entry.find('menu_item_call.on_click').get('function')=='View.ToggleCinematicMode'
    notifications=ET.parse(view/'skins/default/xui/en/notifications.xml').getroot();assert notifications.find("notification[@name='CinematicConfirmHideUI']") is not None
    settings=ET.parse(view/'app_settings/settings_alchemy.xml').getroot().find('map');nodes=list(settings)
    entry=next(nodes[i+1] for i,x in enumerate(nodes) if x.tag=='key' and x.text=='AlchemyCinematicModeHideHoverText')
    data=dict(zip([x.text for x in list(entry)[::2]],[x.text for x in list(entry)[1::2]]));assert data['Type']=='Boolean' and data['Persist']=='1' and data['Value']=='0'
    if a.emit_source:
        a.emit_source.parent.mkdir(parents=True,exist_ok=True);a.emit_source.write_text(source);return 0
    def run(out):
        out.mkdir(parents=True,exist_ok=True);cpp=out/'cinematic_actual.cpp';exe=out/'cinematic_actual';cpp.write_text(source)
        subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer','-g','-I',str(view),str(cpp),'-o',str(exe)],check=True)
        return subprocess.run([str(exe)]+(['movement'] if a.test_mode=='movement' else [])).returncode
    if a.output_dir:return run(a.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix='alchemy-cinematic-') as d:return run(Path(d))
if __name__=='__main__':raise SystemExit(main())
