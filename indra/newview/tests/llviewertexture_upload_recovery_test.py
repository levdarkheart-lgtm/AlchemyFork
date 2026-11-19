#!/usr/bin/env python3
"""Compile and fault-test the current production upload/publication bodies (PPD-467).

No viewer, login, GPU or dependency build is needed. Queue and storage boundaries
are controlled inputs; the worker/continuation, finalization and publication
state machine are extracted verbatim from the selected source revision.
"""
import argparse
import hashlib
import json
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
    parser.add_argument("--emit-source", type=Path)
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
    selected.append(("indra/llrender/llimagegl.cpp", "void LLImageGL::cleanupClass()"))
    for signature in ("U64 LLImageGL::retainUploadOwner(", "bool LLImageGL::ownsUploadOwner(", "void LLImageGL::releaseUploadOwner("):
        try: production("indra/llrender/llimagegl.cpp",signature,args.revision)
        except ValueError: pass  # Baseline has no owner registry; its actual paths remain the negative control.
        else: selected.append(("indra/llrender/llimagegl.cpp",signature))
    bodies = "\n\n".join(production(path, name, args.revision, args.texture_source) for path, name in selected)
    create_list = production("indra/newview/llviewertexturelist.cpp",
                             "F32 LLViewerTextureList::updateImagesCreateTextures(", args.revision)
    branch_begin = create_list.index("            if (admission && imagep->postNativeTextureUpload(admission.credit))")
    branch_end = create_list.index("\n            }", branch_begin) + len("\n            }")
    post_branch = create_list[branch_begin:branch_end]
    fixture = (HERE / "llviewertexture_upload_recovery_fixture.cpp").read_text()
    queue_text = (subprocess.check_output(["git","show",f"{args.revision}:indra/llcommon/workqueue.h"],cwd=ROOT,text=True) if args.revision else (ROOT/"indra/llcommon/workqueue.h").read_text())
    queue_start=queue_text.rindex("    /// general case: arbitrary C++ return type",0,queue_text.index("    struct WorkQueueBase::MakeReplyLambda"))
    queue_end=queue_text.index("    /// general case: arbitrary C++ return type",queue_start+1)
    queue_bodies=queue_text[queue_start:queue_end]
    fixture=fixture.replace("// @ACTUAL_POST_TO@",queue_bodies)
    source = fixture.replace("// @PRODUCTION_BODIES@", bodies).replace("// @POST_BRANCH@", post_branch)
    if args.emit_source:
        args.emit_source.parent.mkdir(parents=True,exist_ok=True)
        args.emit_source.write_text(source)
        return 0
    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp = directory / "upload_recovery_actual.cpp"
        binary = directory / "upload_recovery_actual"
        cpp.write_text(source)
        command=["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 f"-DLL_IMAGEGL_THREAD_CHECK={args.thread_check}",
                 "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g", str(cpp), "-o", str(binary)]
        compiled=subprocess.run(command,capture_output=True,text=True)
        (directory/"compile.log").write_text(compiled.stdout+compiled.stderr)
        tested=None
        if compiled.returncode==0:
            tested=subprocess.run([str(binary)],capture_output=True,text=True)
            (directory/"test.log").write_text(tested.stdout+tested.stderr)
            print(tested.stdout+tested.stderr,end="")
        else: print(compiled.stdout+compiled.stderr,end="")
        receipt={"revision":args.revision or "working-tree", "thread_check":args.thread_check,
            "generated_sha256":hashlib.sha256(source.encode()).hexdigest(),
            "production_body_sha256":{signature:hashlib.sha256(production(path,signature,args.revision,args.texture_source).encode()).hexdigest() for path,signature in selected},
            "postTo_templates_sha256":hashlib.sha256(queue_bodies.encode()).hexdigest(),
            "compile_command":command,"compile_exit":compiled.returncode,"test_exit":tested.returncode if tested else None,
            "boundary":"Actual postTo/makeReplyLambda/postMaybe plus upload/publication/cleanup bodies. Controlled queues/storage/mutex-free single-thread scheduling and counted fetched refs; real Metal/actual fetched class destruction not executed here."}
        (directory/"receipt.json").write_text(json.dumps(receipt,indent=2)+"\n")
        return compiled.returncode if compiled.returncode else tested.returncode

    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix="alchemy-upload-recovery-") as temporary:
        return run(Path(temporary))


if __name__ == "__main__":
    raise SystemExit(main())
