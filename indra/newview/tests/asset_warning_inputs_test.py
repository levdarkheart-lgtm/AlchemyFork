#!/usr/bin/env python3
"""PPD-466/472: run actual diagnostics with controlled asset/HTTP inputs."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def asset_fixture(root):
    texture = (root / "indra/newview/lltexturefetch.cpp").read_text()
    function = texture.index("void LLTextureFetchWorker::onCompleted(")
    begin = texture.index("    bool success = true;", function)
    end = texture.index("    S32BytesImplicit data_size = callbackHttpGet", begin)
    texture_body = texture[begin:end]
    begin = texture.index("void LLTextureFetchWorker::callbackDecoded(")
    end = texture.index("\n}", begin) + 2
    decode_body = texture[begin:end].replace("LLTextureFetchWorker::callbackDecoded", "TextureOwner::decoded")
    material = (root / "indra/newview/llgltfmateriallist.cpp").read_text()
    function = material.index("void LLGLTFMaterialList::onAssetLoadComplete(")
    begin = material.index("        if (ALWarningDiagnostics::enabled())", function)
    end = material.index("        asset_data->mMaterial->materialComplete(false);", begin)
    material_body = material[begin:end]
    fixture = (Path(__file__).with_name("asset_warning_inputs_fixture.cpp")).read_text()
    fixture = fixture.replace("// @TEXTURE_FAILURE_BODY@", texture_body)
    fixture = fixture.replace("// @MATERIAL_FAILURE_DIAGNOSTIC@", material_body)
    fixture = fixture.replace("// @DECODE_BODY@", decode_body)
    return fixture


def http_fixture(root):
    policy = (root / "indra/llcorehttp/_httppolicy.cpp").read_text()
    function = policy.index("void diagnose_http_failure(")
    begin = policy.index("    if (!ALWarningDiagnostics::enabled())", function)
    end = policy.index("        const LLURI uri(", begin)
    fixture = Path(__file__).with_name("http_terminal_warning_fixture.cpp").read_text()
    return fixture.replace("// @HTTP_ADMISSION_PREFIX@", policy[begin:end])


def run(directory, root, case):
    for name, factory in (("asset", asset_fixture), ("http", http_fixture)):
        if case not in ("all", name):
            continue
        source, binary = directory / (name + ".cpp"), directory / name
        source.write_text(factory(root))
        subprocess.run(["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                        "-I", str(ROOT / "indra/llcommon"), str(source), "-o", str(binary)], check=True)
        for enabled in ("0", "1"):
            environment = dict(os.environ, ALCHEMY_WARNING_DIAGNOSTICS=enabled)
            subprocess.run([str(binary)], env=environment, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="production source revision to extract (defaults to this checkout)")
    parser.add_argument("--case", choices=("all", "asset", "http"), default="all")
    parser.add_argument("--keep-directory", type=Path,
                        help="retain generated actual bodies and sanitizer binaries")
    args = parser.parse_args()
    if args.keep_directory:
        args.keep_directory.mkdir(parents=True, exist_ok=True)
        run(args.keep_directory, args.source_root, args.case)
    else:
        with tempfile.TemporaryDirectory(prefix="alchemy-warning-inputs-") as name:
            run(Path(name), args.source_root, args.case)


if __name__ == "__main__":
    main()
