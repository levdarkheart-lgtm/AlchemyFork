#!/usr/bin/env python3
"""Compile and fault-test the current production upload/publication bodies (PPD-467).

No viewer, login, GPU or dependency build is needed. Queue and storage boundaries
are controlled inputs; the worker/continuation, finalization and publication
state machine are extracted verbatim from the selected source revision.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def production(path, signature, revision, texture_source=None):
    text = (texture_source.read_text() if texture_source and path == "indra/newview/llviewertexture.cpp" else
            subprocess.check_output(["git", "show", f"{revision}:{path}"],
                                   cwd=ROOT, text=True)
            if revision else (ROOT / path).read_text())
    begin = text.index(signature)
    end = text.index("\n}", begin) + 2
    return text[begin:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", help="Test retained Git bodies, e.g. the pre-fix negative control")
    parser.add_argument("--output-dir", type=Path, help="Retain generated source and binary")
    parser.add_argument("--texture-source", type=Path,
                        help="Use a preserved viewertexture source for a focused negative control")
    parser.add_argument("--thread-check", choices=("0", "1"), default="0",
                        help="Compile the real debug input-copy guard as well")
    args = parser.parse_args()
    selected = [
        ("indra/newview/llviewertexture.cpp", "bool LLViewerFetchedTexture::createTexture(S32 usename, S32 category)"),
        ("indra/newview/llviewertexture.cpp", "void LLViewerFetchedTexture::postCreateTexture()"),
        ("indra/newview/llviewertexture.cpp", "bool LLViewerFetchedTexture::postNativeTextureUpload("),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::beginUpload()"),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::beginUpload(S8 components)"),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::cancelUpload()"),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::endUpload()"),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::queuePublish("),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::finishPublish("),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::afterPublish(std::function<void()>"),
        ("indra/llrender/llimagegl.cpp", "void LLImageGL::afterPublish(std::function<void(bool)>"),
    ]
    bodies = "\n\n".join(production(path, name, args.revision, args.texture_source) for path, name in selected)
    create_list = production("indra/newview/llviewertexturelist.cpp",
                             "F32 LLViewerTextureList::updateImagesCreateTextures(", args.revision)
    branch_begin = create_list.index("            if (admission && imagep->postNativeTextureUpload(admission.credit))")
    branch_end = create_list.index("\n            }", branch_begin) + len("\n            }")
    post_branch = create_list[branch_begin:branch_end]
    fixture = (HERE / "llviewertexture_upload_recovery_fixture.cpp").read_text()
    source = fixture.replace("// @PRODUCTION_BODIES@", bodies).replace("// @POST_BRANCH@", post_branch)
    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp = directory / "upload_recovery_actual.cpp"
        binary = directory / "upload_recovery_actual"
        cpp.write_text(source)
        subprocess.run(["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        f"-DLL_IMAGEGL_THREAD_CHECK={args.thread_check}",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                        str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)]).returncode
    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix="alchemy-upload-recovery-") as temporary:
        return run(Path(temporary))


if __name__ == "__main__":
    raise SystemExit(main())
