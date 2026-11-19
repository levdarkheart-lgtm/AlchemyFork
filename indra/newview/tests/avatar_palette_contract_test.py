#!/usr/bin/env python3
"""PPD313: execute the current avatar palette packing/cache body under sanitizers.

Only the pose/palette producer boundaries are controlled; the production method
and joint-count body are copied verbatim. This does not instantiate a live avatar
or qualify skinning numerical causes, GPU output, assets or region appearance.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def body(path, signature, revision):
    source = (subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT, text=True)
              if revision else (ROOT / path).read_text())
    begin = source.index(signature)
    return source[begin:source.index("\n}", begin) + 2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", help="Test retained production body, e.g. the pre-fix negative control")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--case", choices=("zero", "valid", "all"), default="all")
    args = parser.parse_args()
    function = body("indra/newview/llvoavatar.cpp",
                    "const LLVOAvatar::MatrixPaletteCache& LLVOAvatar::updateSkinInfoMatrixPalette(", args.revision)
    count = body("indra/newview/llskinningutil.cpp", "U32 LLSkinningUtil::getMeshJointCount(", args.revision)
    source = (HERE / "avatar_palette_contract_fixture.cpp").read_text().replace("// @PRODUCTION_BODY@", function).replace("// @PRODUCTION_COUNT@", count)

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp = directory / "avatar_palette_actual.cpp"
        binary = directory / "avatar_palette_actual"
        cpp.write_text(source)
        subprocess.run(["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                        "-fno-omit-frame-pointer", "-g", str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary), args.case]).returncode
    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix="alchemy-palette-contract-") as directory:
        return run(Path(directory))


if __name__ == "__main__":
    raise SystemExit(main())
