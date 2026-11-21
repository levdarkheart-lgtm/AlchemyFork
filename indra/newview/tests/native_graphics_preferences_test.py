#!/usr/bin/env python3
"""PPD527/536/541: packaged quality, native availability and callback lifetimes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OBSOLETE = {
    "RenderVBOEnable", "RenderVBOMappingDisable", "RenderUseStreamVBO",
    "RenderDownScaleMethod", "RenderGLMultiThreadedTextures", "RenderGLContextCoreProfile",
    "RenderMaxOpenGLVersion", "RenderGLContext10bitSDR", "RenderNsightDebugSupport",
    "RenderUseVAO", "RenderPreferStreamDraw",
}


def block(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise ValueError("unclosed production body")


def llsd_map(element):
    entries = list(element)
    assert len(entries) % 2 == 0, "malformed LLSD map"
    result = {}
    for key, value in zip(entries[::2], entries[1::2]):
        assert key.tag == "key" and key.text not in result, "duplicate or invalid key"
        result[key.text] = value
    return result


def statement(source, beginning):
    start = source.index(beginning)
    depth = 0
    for index in range(start, len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if source[index] == ";" and depth == 0:
            return source[start:index + 1]
    raise ValueError("unclosed production statement")


def verify_packaging(read, advanced):
    settings = {}
    for name in ("settings.xml", "settings_alchemy.xml"):
        settings.update(llsd_map(ET.fromstring(read("indra/newview/app_settings/" + name)).find("map")))
    assert not OBSOLETE.intersection(settings), "obsolete backend settings still exposed"
    # GL-named settings with real native consumers must not be removed by a name sweep.
    assert {"RenderMaxVBOSize", "RenderGLMultiThreadedMedia", "RenderMetalMultiThreadedTextures",
            "RenderDepthOfField", "RenderDeferredSSAO", "AlchemyRenderWaterWaves"} <= settings.keys()
    for filename in ("panel_preferences_graphics1.xml", "floater_preferences_graphics_advanced.xml",
                     "panel_performance_preferences.xml", "panel_lightbox_scene.xml"):
        ui = ET.fromstring(read("indra/newview/skins/default/xui/en/" + filename))
        for node in ui.iter():
            name = node.get("control_name")
            assert not name or name in settings, f"{filename}: unknown setting {name}"
            assert node.get("name") not in {"advanced_lighting_model", "UseLightShaders"}
    controls = {node.get("name"): node for node in advanced.iter() if node.get("name")}
    for name in ("UseSSAO", "UseDoF", "ShadowDetail", "fsaa", "fsaa quality", "anisotropic_filter",
                 "ReflectionDetail", "ReflectionLevel", "ProbeCount", "Mirrors", "MirrorResolution"):
        assert name in controls, f"native quality control lost: {name}"
    assert {node.get("value") for node in controls["fsaa"]} == {"0", "1", "2"}, "native AA choices lost"
    for language in ROOT.joinpath("indra/newview/skins/default/xui").iterdir():
        name = f"indra/newview/skins/default/xui/{language.name}/floater_preferences_graphics_advanced.xml"
        if not ROOT.joinpath(name).is_file():
            continue
        translated = ET.fromstring(read(name))
        for node in translated.iter():
            assert node.get("name") not in {"vbo", "UseLightShaders", "antialiasing restart"}
            assert "OpenGL" not in node.get("tool_tip", ""), f"obsolete tooltip in {language.name}"
    waves = llsd_map(settings["AlchemyRenderWaterWaves"])
    assert "OpenGL" not in waves["Comment"].text and waves["Value"].text == "1"
    for filename in ("featuretable.txt", "featuretable_mac.txt", "featuretable_linux.txt"):
        for line in read("indra/newview/" + filename).splitlines():
            fields = line.split()
            assert not fields or fields[0] not in OBSOLETE, f"dead feature in {filename}"
    presets = block(read("indra/newview/llpresetsmanager.cpp"), "void LLPresetsManager::getGraphicsControlNames(")
    names = set(re.findall(r'"([A-Za-z0-9_]+)"', presets))
    assert names <= settings.keys(), "preset refers to a removed setting"
    assert {"RenderFSAAType", "RenderFSAASamples", "RenderDepthOfField", "RenderDeferredSSAO",
            "RenderShadowDetail", "RenderMirrors", "RenderReflectionProbeCount"} <= names
    print("native graphics packaging: settings, bindings, translations, feature tables and presets passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision")
    parser.add_argument("--runtime-only", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--emit-source", type=Path)
    parser.add_argument("--include-dir", type=Path, action="append", default=[])
    args = parser.parse_args()

    def read(name):
        if args.revision:
            return subprocess.check_output(["git", "show", f"{args.revision}:{name}"], cwd=ROOT, text=True)
        return ROOT.joinpath(name).read_text()

    source = read("indra/newview/llfloaterpreferencesgraphicsadvanced.cpp")
    advanced = ET.fromstring(read("indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml"))
    if not args.runtime_only:
        verify_packaging(read, advanced)
    methods = block(source, "void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledState()")
    methods += "\n" + block(source, "LLFloaterPreferenceGraphicsAdvanced::~LLFloaterPreferenceGraphicsAdvanced()")
    if "void LLFloaterPreferenceGraphicsAdvanced::disableUnavailableSettings()" in source:
        methods += "\n" + block(source, "void LLFloaterPreferenceGraphicsAdvanced::disableUnavailableSettings()")
    performance = read("indra/newview/llfloaterperformance.cpp")
    if "bool is_ALM_available()" in performance:
        methods += "\n" + block(performance, "bool is_ALM_available()")
    methods += "\n" + block(performance, "void LLFloaterPerformance::onClickShadows()")
    names = sorted({node.get("name") for node in advanced.iter() if node.get("name")})
    literals = ",\n".join("{" + json.dumps(name) + ", {}}" for name in names)
    generated = (HERE.joinpath("native_graphics_preferences_fixture.cpp").read_text()
                 .replace("// @PACKAGED_NAMES@", literals).replace("// @PRODUCTION_METHODS@", methods))
    hookups = "\n".join(statement(source, name + " =") for name in
                         ("mImpostorsChangedSignal", "mNumImpostorsChangedSignal"))
    generated = generated.replace("// @PRODUCTION_HOOKUPS@", hookups)
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True, exist_ok=True)
        args.emit_source.write_text(generated)
        return 0

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp, binary = directory / "preferences.cpp", directory / "preferences"
        cpp.write_text(generated)
        receipt = {"revision": args.revision or "working-tree",
                   "source_sha256": hashlib.sha256(generated.encode()).hexdigest(), "runs": []}
        code = 0
        includes = [flag for path in args.include_dir for flag in ("-isystem", str(path))]
        for name, argv in [("compile", ["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                                        "-Wno-unused-parameter", *includes,
                                        "-fsanitize=address,undefined",
                                        "-fno-sanitize-recover=undefined", "-fno-omit-frame-pointer",
                                        str(cpp), "-o", str(binary)]), ("run", [str(binary)])]:
            result = subprocess.run(argv, capture_output=True, text=True)
            directory.joinpath(name + ".stdout").write_text(result.stdout)
            directory.joinpath(name + ".stderr").write_text(result.stderr)
            receipt["runs"].append({"name": name, "argv": argv, "exit": result.returncode})
            print(result.stdout, end="")
            print(result.stderr, end="")
            code = result.returncode
            if code:
                break
        directory.joinpath("receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        return code

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix="alchemy-native-preferences-") as directory:
        return run(Path(directory))


if __name__ == "__main__":
    raise SystemExit(main())
