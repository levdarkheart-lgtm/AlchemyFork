#!/usr/bin/env python3
"""PPD-466: run actual failure diagnostics with controlled asset/HTTP inputs."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    texture = (ROOT / "indra/newview/lltexturefetch.cpp").read_text()
    function = texture.index("void LLTextureFetchWorker::onCompleted(")
    begin = texture.index("    bool success = true;", function)
    end = texture.index("    S32BytesImplicit data_size = callbackHttpGet", begin)
    texture_body = texture[begin:end]
    begin = texture.index("void LLTextureFetchWorker::callbackDecoded(")
    end = texture.index("\n}", begin) + 2
    decode_body = texture[begin:end].replace("LLTextureFetchWorker::callbackDecoded", "TextureOwner::decoded")
    material = (ROOT / "indra/newview/llgltfmateriallist.cpp").read_text()
    function = material.index("void LLGLTFMaterialList::onAssetLoadComplete(")
    begin = material.index("        if (ALWarningDiagnostics::enabled())", function)
    end = material.index("        asset_data->mMaterial->materialComplete(false);", begin)
    material_body = material[begin:end]
    fixture = (Path(__file__).with_name("asset_warning_inputs_fixture.cpp")).read_text()
    fixture = fixture.replace("// @TEXTURE_FAILURE_BODY@", texture_body)
    fixture = fixture.replace("// @MATERIAL_FAILURE_DIAGNOSTIC@", material_body)
    fixture = fixture.replace("// @DECODE_BODY@", decode_body)
    with tempfile.TemporaryDirectory(prefix="alchemy-warning-inputs-") as name:
        directory = Path(name)
        source, binary = directory / "actual.cpp", directory / "actual"
        source.write_text(fixture)
        subprocess.run(["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                        "-I", str(ROOT / "indra/llcommon"), str(source), "-o", str(binary)], check=True)
        for enabled in ("0", "1"):
            environment = dict(os.environ, ALCHEMY_WARNING_DIAGNOSTICS=enabled)
            subprocess.run([str(binary)], env=environment, check=True)


if __name__ == "__main__":
    main()
