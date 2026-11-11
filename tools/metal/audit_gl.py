#!/usr/bin/env python3
"""Inventory GL vocabulary and gate API dependencies in first-party indra source.

Inventory exits 0 even with findings. Strict fails API/context/loader/header
dependencies or an incomplete scan, not native classes retaining GL-era names.
This conservative source gate does not establish runtime or linked-library safety.
"""
import argparse
from bisect import bisect_right
import configparser
from itertools import islice
import json
import os
import re
from pathlib import Path
from typing import NamedTuple

ROOT = Path(__file__).resolve().parents[2]
OWNED = ["indra"]
EXTENSIONS = {".cpp", ".c", ".cc", ".cxx", ".mm", ".m", ".h", ".hpp", ".hh", ".hxx", ".inl", ".ipp"}
VENDORED = {"externals", "third_party", "vendor", "vendored"}
TEST_DIRS = {"test", "tests", "integration_tests"}
COMPAT_CLASSES = {"LLGLSLShader", "LLShaderMgr", "LLImageGL", "LLGLState"}
# Original 1eb44: these divide UI coordinates by scale or call that conversion.
# Exempt only these identifiers at their actual declaration/definition paths,
# never a file or the body of a helper/native compatibility class.
CPU_HELPERS = {
    "indra/llui/llui.cpp": {"glPointToScreen", "glRectToScreen"},
    "indra/llui/llui.h": {"glPointToScreen", "glRectToScreen"},
}
API_NAME = re.compile(r"(?:gl|wgl|egl|CGL)[A-Z][A-Za-z0-9_]*\Z")
API_TYPE = re.compile(r"PFN_?(?:GL|WGL|EGL|CGL)[A-Za-z0-9_]*\Z", re.IGNORECASE)
CONTEXT = re.compile(
    r"(?:NSOpenGL\w*|EAGL\w*|CGLContextObj|CGLPixelFormatObj|HGLRC|"
    r"GLX\w*|EGL\w*|WGL_\w*|kCGL\w*|GLFW_OPENGL\w*|GL_CONTEXT\w*)\Z")
GL_HEADER = re.compile(
    r"^(?:(?:OpenGL|OpenGLES|GL|EGL|GLES\d*|GLFW|glad)/|"
    r"epoxy/gl|(?:SDL\d*/)?SDL_opengl|"
    r"(?:gl|glew|glad|glcorearb|glext|wglext|glx|glxext|egl|eglext)\.h\Z)",
    re.IGNORECASE)
GL_LIBRARY = re.compile(
    r"(?:^|[/\\])(?:opengl32(?:\.(?:dll|lib))?|"
    r"lib(?:OpenGL|GL|GLX|EGL|GLES\w*)(?:[.-][^/\\]*)?|"
    r"(?:OpenGL|OpenGLES)\.framework)(?:[/\\]|$)|^com\.apple\.OpenGL$",
    re.IGNORECASE)
LOADERS = {
    "dlsym", "dlopen", "GetProcAddress", "LoadLibrary", "LoadLibraryA",
    "LoadLibraryW", "LoadLibraryEx", "LoadLibraryExA", "LoadLibraryExW", "SDL_LoadFunction",
    "SDL_LoadObject", "CFBundleGetFunctionPointerForName",
    "CFBundleGetDataPointerForName", "CFBundleGetBundleWithIdentifier", "NSLookupSymbolInImage",
    "NSLookupAndBindSymbol", "NSIsSymbolNameDefined",
}
GL_LOADERS = {"glfwGetProcAddress", "gladLoadGL", "gladLoadGLLoader",
              "gladLoadGLES2", "glewInit", "epoxy_get_proc_address"}
LITERAL_WRAPPERS = {"TEXT", "_T", "CFSTR"}
TOKEN = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*")
NUMBER = re.compile(r"(?:[0-9]|\.[0-9])(?:[A-Za-z0-9_.]|'(?=[A-Za-z0-9_]))*")
RAW_STRING = re.compile(r'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')
QUOTED = re.compile(r'''(?:u8|u|U|L)?(["'])''')
OPERATORS = re.compile(r"##|::|->\*|->|<=>|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||"
                       r"\+\+|--|\+=|-=|\*=|/=|%=|&=|\|=|\^=|\.\.\.")


class Token(NamedTuple):
    kind: str
    text: str
    line: int
    logical_line: int
    value: str = ""


def decode_literal(value):
    """Decode C/C++ escapes for literal symbol/library lookup arguments."""
    escapes = {"a": "\a", "b": "\b", "f": "\f", "n": "\n", "r": "\r",
               "t": "\t", "v": "\v", "\\": "\\", "'": "'", '"': '"', "?": "?"}

    def replace(match):
        part = match.group()[1:]
        if part in escapes:
            return escapes[part]
        if part.startswith(("x", "u", "U")):
            return chr(int(part[1:], 16))
        if part[0] in "01234567":
            return chr(int(part, 8))
        return part

    return re.sub(r"\\(?:x[0-9A-Fa-f]+|u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8}|[0-7]{1,3}|.)",
                  replace, value)


def tokenize(text):
    """Remove comments/literal bodies without mistaking URLs or raw GLSL for code.

    C/C++ line splicing happens before comments and tokens. Keep the original
    physical line for diagnostics, including tokens assembled across a splice.
    This is lexical analysis, not a preprocessor or an interprocedural resolver.
    """
    splices = []
    removed = 0
    for match in re.finditer(r"\\\r?\n", text):
        splices.append(match.start() - removed)
        removed += len(match.group())
    code = re.sub(r"\\\r?\n", "", text)
    starts = [0] + [m.end() for m in re.finditer("\n", code)]
    tokens, errors = [], []
    pos = 0

    def add(kind, start, end, value=""):
        logical = bisect_right(starts, start)
        tokens.append(Token(kind, code[start:end],
                            logical + bisect_right(splices, start), logical, value))

    while pos < len(code):
        start = pos
        if code[pos].isspace():
            pos += 1
        elif code.startswith("//", pos):
            end = code.find("\n", pos)
            pos = end if end >= 0 else len(code)
        elif code.startswith("/*", pos):
            end = code.find("*/", pos + 2)
            if end < 0:
                errors.append("Unterminated block comment")
                break
            pos = end + 2
        elif match := RAW_STRING.match(code, pos):
            closing = ")" + match.group(1) + '"'
            end = code.find(closing, match.end())
            if end < 0:
                errors.append("Unterminated raw string")
                break
            pos = end + len(closing)
            add("string", start, pos, code[match.end():end])
        elif match := QUOTED.match(code, pos):
            quote, pos = match.group(1), match.end()
            content = pos
            while pos < len(code) and code[pos] != quote and code[pos] != "\n":
                pos += 2 if code[pos] == "\\" else 1
            if pos >= len(code) or code[pos] != quote:
                errors.append("Unterminated quoted literal")
                break
            try:
                value = decode_literal(code[content:pos])
            except (ValueError, OverflowError):
                errors.append("Invalid quoted literal escape")
                value = ""
            pos += 1
            add("string" if quote == '"' else "char", start, pos, value)
        elif match := NUMBER.match(code, pos):
            pos = match.end()
            add("number", start, pos)
        elif match := TOKEN.match(code, pos):
            pos = match.end()
            add("identifier", start, pos)
        elif match := OPERATORS.match(code, pos):
            pos = match.end()
            add("punctuation", start, pos)
        else:
            pos += 1
            add("punctuation", start, pos)
    return tokens, errors


def string_value(tokens, bindings, seen=frozenset()):
    """Resolve only adjacent literals and simple local/object-macro aliases."""
    parts = []
    for token in tokens:
        if token.kind == "string":
            parts.append(token.value)
        elif token.text in {"(", ")", "+", "@"}:
            continue
        elif token.text in LITERAL_WRAPPERS:
            continue
        elif token.kind == "identifier" and token.text in bindings and token.text not in seen:
            value = string_value(bindings[token.text], bindings, seen | {token.text})
            if value is None:
                return None
            parts.append(value)
        else:
            return None
    return "".join(parts) if parts else None


def header_value(tokens, bindings, seen=frozenset()):
    value = string_value(tokens, bindings, seen)
    if value is not None:
        return value
    if tokens and tokens[0].text == "<" and tokens[-1].text == ">":
        return "".join(token.text for token in tokens[1:-1])
    if len(tokens) == 1 and tokens[0].text in bindings and tokens[0].text not in seen:
        name = tokens[0].text
        return header_value(bindings[name], bindings, seen | {name})
    return None


def arguments(tokens, opening):
    args, part, depth = [], [], 0
    for token in islice(tokens, opening + 1, None):
        if token.text == ")" and depth == 0:
            return args + [part]
        if token.text == "," and depth == 0:
            args.append(part)
            part = []
            continue
        if token.text in {"(", "[", "{"}:
            depth += 1
        elif token.text in {")", "]", "}"}:
            depth -= 1
        part.append(token)
    return []


def literal_fragments(tokens, bindings, seen=frozenset()):
    """Expose provable literal fragments inside unresolved lookup expressions.

    For example dlsym(handle, "gl" #name) is already a GL lookup dependency,
    as is dlsym(handle, std::string("glClear").c_str()). This does not resolve
    arbitrary wrappers or expand a function macro's parameter.
    """
    prefix = ""
    for token in tokens:
        if token.kind == "string":
            prefix += token.value
        elif token.text in bindings and token.text not in seen:
            if prefix:
                yield prefix
                prefix = ""
            yield from literal_fragments(bindings[token.text], bindings, seen | {token.text})
        elif token.text not in {"(", ")", "+", "@"} | LITERAL_WRAPPERS:
            if prefix:
                yield prefix
            prefix = ""
    if prefix:
        yield prefix


def header_alternatives(tokens, bindings, history, seen=frozenset()):
    value = header_value(tokens, bindings, seen)
    if value is not None:
        yield value
    if len(tokens) == 1 and tokens[0].text in history and tokens[0].text not in seen:
        name = tokens[0].text
        for body in history[name]:
            yield from header_alternatives(body, bindings, history, seen | {name})


def inspect_source(text, rel):
    tokens, errors = tokenize(text)
    rows = {}
    lines = text.split("\n")

    def finding(token, kind, blocking=True, symbol=None):
        row = rows.setdefault(token.line, {
            "path": rel, "line": token.line, "kinds": set(),
            "blocking_kinds": set(), "informational_kinds": set(), "symbols": set(),
            "snippet": lines[token.line - 1].strip()[:180],
        })
        row["kinds"].add(kind)
        row["blocking_kinds" if blocking else "informational_kinds"].add(kind)
        row["symbols"].add(symbol or token.text)

    definitions = {}
    for index, token in enumerate(tokens):
        following = tokens[index + 1:index + 3]
        if token.text == "#" and len(following) == 2 and following[0].text == "define":
            body = []
            for item in islice(tokens, index + 3, None):
                if item.logical_line != token.logical_line:
                    break
                body.append(item)
            definitions[index] = (following[1].text, body, True)
        assignment = index + 1
        if following and following[0].text == "[":
            # A local char symbol[]/symbol[N] has the same literal semantics
            # as a pointer declaration for the loader's C-string argument.
            while assignment < len(tokens) and tokens[assignment].text != "]":
                assignment += 1
            assignment += 1
        if (token.kind == "identifier" and assignment < len(tokens) and
                tokens[assignment].text == "="):
            expression = []
            for item in islice(tokens, assignment + 1, None):
                if item.text == ";":
                    break
                expression.append(item)
            definitions[index] = (token.text, expression, False)

    bindings, loader_candidates, history, literal_history = {}, {}, {}, {}
    loaders = set(LOADERS)
    for index, token in enumerate(tokens):
        if index in definitions:
            name, body, macro = definitions[index]
            # Without control-flow/scope/preprocessor evaluation a later
            # assignment cannot prove that an earlier GL candidate is dead.
            # Preserve all candidates conservatively for lookup/include use.
            history.setdefault(name, []).append(body)
            literal_history.setdefault(name, []).extend(
                body + [token._replace(kind="punctuation", text=";")])
            if macro:
                bindings[name] = body
            else:
                # A copied local string keeps its value when another alias is
                # later assigned. Macro replacement instead happens at use.
                value = string_value(body, bindings)
                bindings[name] = [token._replace(kind="string", value=value)] if value is not None else body
            alias = [part for part in body if part.text not in {"(", ")", "&", "*"}]
            if macro or (len(alias) == 1 and alias[0].kind == "identifier"):
                loader_candidates[name] = body
            else:
                loader_candidates.pop(name, None)
            # Object aliases and function-like lookup macros are local only.
            # Arbitrary wrappers/computed names still need runtime auditing.
            while True:
                aliases = {alias for alias, expression in loader_candidates.items()
                           if any(t.text in loaders for t in expression)}
                if aliases <= loaders:
                    break
                loaders.update(aliases)
        name = token.text
        if token.kind == "identifier":
            if name in COMPAT_CLASSES:
                finding(token, "gl_shader", False)
            if API_NAME.fullmatch(name):
                helper = name in CPU_HELPERS.get(rel, ())
                finding(token, "cpu_coordinate_helper" if helper else "gl_function", not helper)
            if API_TYPE.fullmatch(name):
                finding(token, "gl_prototype")
            if name in GL_LOADERS:
                finding(token, "gl_loader")
            if name.startswith(("SDL_GL_", "SDL_EGL_")) or name in {
                    "SDL_WINDOW_OPENGL"} or (name.startswith("SDL_PROP_") and
                                             ("OPENGL" in name or "_EGL_" in name)):
                finding(token, "sdl_gl")
            if CONTEXT.fullmatch(name):
                finding(token, "gl_context")
        if name == "##" and index > 0 and index + 1 < len(tokens):
            begin = index - 1
            while begin >= 2 and tokens[begin - 1].text == "##":
                begin -= 2
            pieces = [tokens[begin].text]
            cursor = begin + 1
            while cursor + 1 < len(tokens) and tokens[cursor].text == "##":
                pieces.append(tokens[cursor + 1].text)
                cursor += 2
            pasted = "".join(pieces)
            prefixes = {"gl", "glX", "wgl", "egl", "CGL", "SDL_GL_", "SDL_EGL_"}
            if API_NAME.fullmatch(pasted) or any("".join(pieces[:end]) in prefixes for end in range(1, len(pieces))):
                finding(token, "gl_token_paste", symbol=pasted)
        if name == "#" and index + 2 < len(tokens) and tokens[index + 1].text in {"include", "import"}:
            header = tokens[index + 2]
            if header.text == "<":
                pieces = []
                for part in islice(tokens, index + 3, None):
                    if part.text == ">" or part.logical_line != token.logical_line:
                        break
                    pieces.append(part.text)
                values = ["".join(pieces)]
            else:
                values = header_alternatives([header], bindings, history)
            for value in values:
                if value and GL_HEADER.search(value):
                    finding(token, "gl_header", symbol=value)
        if name == "import" and index + 1 < len(tokens) and tokens[index + 1].text in {"OpenGL", "OpenGLES"}:
            finding(token, "gl_header", symbol=tokens[index + 1].text)
        if name in loaders and index + 1 < len(tokens) and tokens[index + 1].text == "(":
            for arg in arguments(tokens, index + 1):
                value = string_value(arg, bindings)
                values = ([value] if value is not None else [])
                values.extend(literal_fragments(arg, literal_history))
                for literal in values:
                    # The C loader APIs consume NUL-terminated names.
                    literal = literal.split("\0", 1)[0]
                    if (API_NAME.fullmatch(literal.lstrip("_")) or
                            literal.startswith(("SDL_GL_", "SDL_EGL_")) or
                            literal in {"gl", "glX", "wgl", "egl", "CGL"}):
                        finding(token, "gl_dynamic_symbol", symbol=literal)
                    if GL_LIBRARY.search(literal):
                        finding(token, "gl_dynamic_library", symbol=literal)
        if name == "#" and index + 2 < len(tokens) and tokens[index + 1].text == "pragma":
            for part in islice(tokens, index + 2, None):
                if part.logical_line != token.logical_line:
                    break
                if part.kind == "string" and GL_LIBRARY.search(part.value):
                    finding(token, "gl_import", symbol=part.value)

    for row in rows.values():
        for key in ("kinds", "blocking_kinds", "informational_kinds", "symbols"):
            row[key] = sorted(row[key])
        row["blocking"] = bool(row["blocking_kinds"])
    return [rows[line] for line in sorted(rows)], errors


def submodule_paths(root):
    config = configparser.ConfigParser(interpolation=None)
    path = root / ".gitmodules"
    if path.exists():
        config.read_string(path.read_text(encoding="utf-8"), source=str(path))
    return {config.get(section, "path").strip("/") for section in config.sections()
            if config.has_option(section, "path")}


def audit(root: Path, include_tests: bool = False):
    findings, errors, excluded = [], [], []
    scanned = 0
    root = root.resolve()
    try:
        submodules = submodule_paths(root)
    except (OSError, UnicodeError, configparser.Error) as error:
        submodules = set()
        errors.append({"path": ".gitmodules", "error": str(error)})
    for directory in OWNED:
        subtree = root / directory
        if not subtree.exists():
            continue
        def walk_error(error):
            errors.append({"path": str(error.filename), "error": str(error)})

        for current, directories, files in os.walk(subtree, onerror=walk_error):
            for name in sorted(directories):
                path = Path(current) / name
                rel = path.relative_to(root).as_posix()
                reason = ("submodule" if rel in submodules else
                          "vendored" if name in VENDORED else
                          "tests" if not include_tests and name in TEST_DIRS else
                          "directory_symlink" if path.is_symlink() else None)
                if reason:
                    directories.remove(name)
                    excluded.append({"path": rel, "reason": reason})
                    if reason == "directory_symlink":
                        errors.append({"path": rel, "error": "Unscanned first-party directory symlink"})
            directories.sort()
            for name in sorted(files):
                p = Path(current) / name
                if p.suffix.lower() not in EXTENSIONS:
                    continue
                rel = p.relative_to(root).as_posix()
                try:
                    text = p.read_text(encoding="utf-8")
                except (OSError, UnicodeError) as error:
                    errors.append({"path": rel, "error": str(error)})
                    continue
                scanned += 1
                rows, lexical_errors = inspect_source(text, rel)
                findings.extend(rows)
                errors.extend({"path": rel, "error": error} for error in lexical_errors)
    if not scanned:
        errors.append({"path": "indra", "error": "No first-party source files scanned"})
    blocking = sum(item["blocking"] for item in findings)
    return {
        "scanned_files": scanned, "finding_count": len(findings),
        "blocking_count": blocking,
        "informational_count": sum(bool(item["informational_kinds"]) for item in findings),
        "findings": findings, "errors": errors, "error_count": len(errors),
        "counting": "Counts are file/line rows; a row with both blocking and informational kinds appears in both category counts.",
        "scope": {"roots": OWNED, "extensions": sorted(EXTENSIONS),
                  "include_tests": include_tests, "excluded_directories": excluded},
        "limits": "Conservative lexical source gate; all preprocessor branches are scanned. "
                  "Literal candidates are retained across unmodeled scopes, assignments and "
                  "branches. Simple literal/object-macro aliases are inspected, not arbitrary "
                  "computed names or interprocedural dataflow. Native compatibility names and "
                  "CPU constants do not prove API use. Source zero is not runtime/context or "
                  "linked-library proof; foreign media components still require separate audit.",
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=["inventory", "strict"], default="inventory")
    ap.add_argument("--strict", action="store_true",
                    help="fail on blocking dependencies or scan errors (alias for --mode strict)")
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
        print(f"Scanned {data['scanned_files']} first-party files: {data['finding_count']} inventory rows, "
              f"{data['blocking_count']} blocking, {data['informational_count']} informational, "
              f"{data['error_count']} scan errors")
        for item in data["findings"][:50]:
            status = "blocking" if item["blocking"] else "informational"
            print(f"{item['path']}:{item['line']} [{status}: {','.join(item['kinds'])}] {item['snippet']}")
        if data["finding_count"] > 50:
            print(f"... {data['finding_count'] - 50} additional findings (pass --output for full report)")
        for error in data["errors"]:
            print(f"ERROR {error['path']}: {error['error']}")
        print(data["limits"])
    mode = "strict" if args.strict else args.mode
    return 1 if mode == "strict" and (data["blocking_count"] or data["error_count"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
