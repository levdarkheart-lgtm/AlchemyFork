#!/usr/bin/env python3
"""Inventory OpenGL entrypoints in viewer-owned source. Strong but not exhaustive runtime gate.

Default inventory exits 0 with findings. Strict exits 1 if findings remain.
Does not scan third-party vendored libraries or dynamic symbol lookups hidden in strings.
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OWNED = ["indra/llrender", "indra/llwindow", "indra/llui", "indra/newview", "indra/alrender"]
EXTENSIONS = {".cpp", ".c", ".cc", ".mm", ".m", ".h", ".hpp"}
PATTERNS = {
    "gl_function": re.compile(r"\b(?:gl[A-Z][A-Za-z0-9_]*|wgl[A-Z][A-Za-z0-9_]*|CGL[A-Z][A-Za-z0-9_]*)\s*\("),
    "sdl_gl": re.compile(r"\bSDL_GL_[A-Za-z0-9_]+\b|SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN"),
    "gl_shader": re.compile(r"\b(?:LLGLSLShader|LLShaderMgr|LLImageGL|LLGLState)\b"),
    "gl_context": re.compile(r"\b(?:NSOpenGLContext|EAGLContext|GLFW_OPENGL|GL_CONTEXT|#\s*include\s*[<\"](?:OpenGL|GL)/)"),
}


def audit(root: Path, include_tests: bool = False):
    findings = []
    scanned = 0
    for directory in OWNED:
        subtree = root / directory
        if not subtree.exists():
            continue
        for p in sorted(subtree.rglob("*")):
            if not p.is_file() or p.suffix not in EXTENSIONS:
                continue
            rel = p.relative_to(root).as_posix()
            if not include_tests and ("/tests/" in rel or "/test/" in rel):
                continue
            if "/externals/" in rel or "/third_party/" in rel:
                continue
            scanned += 1
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for idx, line in enumerate(text.splitlines(), 1):
                code = line.split("//", 1)[0].strip()
                if not code or code.startswith("*") or code.startswith("/*"):
                    continue
                kinds = [name for name, pat in PATTERNS.items() if pat.search(code)]
                if kinds:
                    findings.append({"path": rel, "line": idx, "kinds": kinds, "snippet": code[:180]})
    return {"scanned_files": scanned, "finding_count": len(findings), "findings": findings,
            "limits": "Static source inventory; comments/preprocessor macros can affect precision; runtime GPU/linked library audit still required."}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=["inventory", "strict"], default="inventory")
    ap.add_argument("--strict", action="store_true",
                    help="fail when findings remain (alias for --mode strict)")
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--include-tests", action="store_true")
    ap.add_argument("--json", action="store_true", help="print machine-readable JSON")
    ap.add_argument("--output", type=Path, help="write full report to file (prefer ignored artifacts/metal)")
    args = ap.parse_args()
    data = audit(args.root, args.include_tests)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    if args.json:
        print(json.dumps(data, indent=2))
    else:
        print(f"Scanned {data['scanned_files']} viewer-owned files: {data['finding_count']} potential OpenGL dependencies")
        for item in data["findings"][:50]:
            print(f"{item['path']}:{item['line']} [{','.join(item['kinds'])}] {item['snippet']}")
        if data["finding_count"] > 50:
            print(f"... {data['finding_count'] - 50} additional findings (pass --output for full report)")
        print(data["limits"])
    mode = "strict" if args.strict else args.mode
    return 1 if mode == "strict" and data["finding_count"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
