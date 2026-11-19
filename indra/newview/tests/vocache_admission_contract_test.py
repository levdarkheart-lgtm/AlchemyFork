#!/usr/bin/env python3
"""PPD475: execute production cache culls and occlusion decisions under sanitizers.

Frustum/projection and query-result providers are controlled. This tests admission
liveness and state semantics, not a live region, drawables, textures, or Metal GPU.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
BODIES = {
    "indra/newview/llvieweroctree.cpp": [
        "class LLSpatialSetOcclusionState :", "class LLSpatialSetOcclusionStateDiff :",
        "void LLOcclusionCullingGroup::setOcclusionState(",
        "class LLSpatialClearOcclusionState :", "class LLSpatialClearOcclusionStateDiff :",
        "class LLSpatialDiscardStaleQueries :", "void LLOcclusionCullingGroup::discardStaleQueriesBelow()",
        "void LLOcclusionCullingGroup::clearOcclusionState(",
        "bool LLOcclusionCullingGroup::needsUpdate()", "bool LLOcclusionCullingGroup::isOcclusionRetestDue()",
        "bool LLOcclusionCullingGroup::isRecentlyVisible() const", "bool LLOcclusionCullingGroup::earlyFail(",
        "bool LLOcclusionCullingGroup::checkOcclusion()", "void LLOcclusionCullingGroup::doOcclusion(",
    ],
    "indra/newview/llvocache.cpp": [
        "class LLVOCacheOctreeCull :", "class LLVOCacheOctreeBackCull :",
        "void LLVOCachePartition::selectBackObjects(", "S32 LLVOCachePartition::cull(",
        "void LLVOCachePartition::addOccluders(", "void LLVOCachePartition::processOccluders(",
    ],
}


def extract(source, signature):
    begin = source.index(signature)
    end = source.index("\n}", begin) + 2
    return source[begin:end] + (";" if signature.startswith("class ") else "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", help="Retained production source for a negative control")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    parts = []
    for path, signatures in BODIES.items():
        source = (subprocess.check_output(["git", "show", f"{args.revision}:{path}"], cwd=ROOT, text=True)
                  if args.revision else (ROOT / path).read_text())
        parts.extend(extract(source, signature) for signature in signatures)
    fixture = (HERE / "vocache_admission_contract_fixture.cpp").read_text()
    fixture = fixture.replace("// @PRODUCTION_BODIES@", "\n\n".join(parts))

    def run(directory):
        directory.mkdir(parents=True, exist_ok=True)
        cpp = directory / "vocache_admission_actual.cpp"
        binary = directory / "vocache_admission_actual"
        cpp.write_text(fixture)
        subprocess.run(["clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        "-Wno-reorder-ctor", "-Wno-unused-parameter", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=undefined", "-fno-omit-frame-pointer", "-g",
                        str(cpp), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)]).returncode
    if args.output_dir:
        return run(args.output_dir.resolve())
    with tempfile.TemporaryDirectory(prefix="alchemy-vocache-contract-") as directory:
        return run(Path(directory))


if __name__ == "__main__":
    raise SystemExit(main())
