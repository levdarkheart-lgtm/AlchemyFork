import csv
import importlib.util
import json
import plistlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class PerfTests(unittest.TestCase):
    def setUp(self):
        self.perf = load("perf_compare")
        self.spec = json.loads((ROOT / "docs/metal/benchmarks/scenarios.json").read_text())

    def test_good_result(self):
        # Synthetic tool test only; MUST NOT be presented as actual Metal performance.
        names = [s["id"] for s in self.spec["scenarios"]]
        ref = {name: [20.0] * 300 for name in names}
        new = {name: [15.0] * 300 for name in names}
        result = self.perf.compare(ref, new, self.spec)
        self.assertTrue(result["passes_target"])
        self.assertEqual(4, result["qualifying_scenes"])

    def test_regression_fails(self):
        names = [s["id"] for s in self.spec["scenarios"]]
        ref = {name: [20.0] * 300 for name in names}
        new = {name: [15.0] * 300 for name in names}
        new[names[0]] = [23.0] * 300
        self.assertFalse(self.perf.compare(ref, new, self.spec)["passes_target"])

    def test_missing_or_short_samples_fail(self):
        names = [s["id"] for s in self.spec["scenarios"]]
        ref = {name: [20.0] * 299 for name in names}
        new = {name: [15.0] * 300 for name in names}
        result = self.perf.compare(ref, new, self.spec)
        self.assertFalse(result["passes_target"])
        self.assertEqual(4, len(result["insufficient_sample_scenes"]))

    def test_invalid_csv_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "input.csv"
            with path.open("w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["scenario", "frame_ms"])
                writer.writerow(["quiet-region", "nan"])
            with self.assertRaisesRegex(ValueError, "nonpositive/nonfinite"):
                self.perf.load_frames(path)

    def write_capture(self, path, variant, exact=1, width=2048,
                      metal_submissions=None, metal_completed=None,
                      metal_timed=None, metal_gpu_ms=None,
                      presentation_state=None, gl_fallback=None):
        headers = ["scenario", "variant", "frame_ms", "frame_index",
                   "source_sha", "build_type", "drawable_width",
                   "drawable_height", "quality_profile", "native_exact"]
        if metal_submissions is not None:
            headers.append("metal_submissions")
        if metal_completed is not None:
            headers.append("metal_completed")
        if metal_timed is not None:
            headers.append("metal_timed")
        if metal_gpu_ms is not None:
            headers.append("metal_gpu_ms")
        if presentation_state is not None:
            headers.extend([
                "native_requested", "native_attempted", "gl_fallback",
                "native_presentation_state",
            ])
        with path.open("w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            frame_index = 100
            for scene in (s["id"] for s in self.spec["scenarios"]):
                for _ in range(300):
                    row = [
                        scene, variant, 16.0, frame_index,
                        "0123456789abcdef0123456789abcdef01234567",
                        "RelWithDebInfo", width, 1280, 5, exact]
                    if metal_submissions is not None:
                        row.append(metal_submissions)
                    if metal_completed is not None:
                        row.append(metal_completed)
                    if metal_timed is not None:
                        row.append(metal_timed)
                    if metal_gpu_ms is not None:
                        row.append(metal_gpu_ms)
                    if presentation_state is not None:
                        requested = presentation_state != "disabled"
                        attempted = presentation_state not in (
                            "disabled", "not_attempted")
                        fallback = requested and not exact \
                            if gl_fallback is None else gl_fallback
                        row.extend([
                            int(requested), int(attempted),
                            int(fallback), presentation_state,
                        ])
                    writer.writerow(row)
                    frame_index += 1

    def test_source_exact_capture_provenance(self):
        with tempfile.TemporaryDirectory() as d:
            baseline_path = Path(d) / "baseline.csv"
            candidate_path = Path(d) / "candidate.csv"
            self.write_capture(baseline_path, "baseline", exact=0)
            self.write_capture(candidate_path, "candidate", exact=1)
            baseline = self.perf.load_capture(
                baseline_path, require_provenance=True)
            candidate = self.perf.load_capture(
                candidate_path, require_provenance=True, candidate=True)
            result = self.perf.compare_provenance(
                baseline, candidate,
                [s["id"] for s in self.spec["scenarios"]])
            self.assertTrue(result["verified"])
            self.assertEqual(4, len(result["scenes"]))

    def test_nonexact_candidate_and_mismatched_display_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            baseline_path = Path(d) / "baseline.csv"
            candidate_path = Path(d) / "candidate.csv"
            self.write_capture(baseline_path, "baseline", exact=0)
            self.write_capture(candidate_path, "candidate", exact=0)
            with self.assertRaisesRegex(ValueError, "not an exact native"):
                self.perf.load_capture(
                    candidate_path, require_provenance=True, candidate=True)

            self.write_capture(candidate_path, "candidate", exact=1, width=1920)
            baseline = self.perf.load_capture(
                baseline_path, require_provenance=True)
            candidate = self.perf.load_capture(
                candidate_path, require_provenance=True, candidate=True)
            with self.assertRaisesRegex(ValueError, "drawable_width"):
                self.perf.compare_provenance(
                    baseline, candidate,
                    [s["id"] for s in self.spec["scenarios"]])

    def test_secondary_metal_submission_counts_are_reported_not_gated(self):
        with tempfile.TemporaryDirectory() as d:
            baseline_path = Path(d) / "baseline.csv"
            candidate_path = Path(d) / "candidate.csv"
            self.write_capture(
                baseline_path, "baseline", exact=0,
                metal_submissions=12, metal_completed=11,
                metal_timed=10, metal_gpu_ms=5.5)
            self.write_capture(
                candidate_path, "candidate", exact=1,
                metal_submissions=5, metal_completed=4,
                metal_timed=4, metal_gpu_ms=2.25)
            baseline = self.perf.load_capture(baseline_path)
            candidate = self.perf.load_capture(candidate_path, candidate=True)
            report = self.perf.compare_secondary_metrics(
                baseline["secondary_metrics"], candidate["secondary_metrics"],
                [s["id"] for s in self.spec["scenarios"]])
            self.assertEqual("count_per_frame",
                             report["metal_submissions"]["unit"])
            self.assertEqual(12,
                             report["metal_submissions"]["scenes"][0]
                             ["baseline"]["p50"])
            self.assertEqual(5,
                             report["metal_submissions"]["scenes"][0]
                             ["candidate"]["p50"])
            self.assertEqual("count_per_frame",
                             report["metal_completed"]["unit"])
            self.assertEqual(4,
                             report["metal_completed"]["scenes"][0]
                             ["candidate"]["p50"])
            self.assertEqual(10,
                             report["metal_timed"]["scenes"][0]
                             ["baseline"]["p50"])
            self.assertEqual("ms", report["metal_gpu_ms"]["unit"])
            self.assertEqual(2.25,
                             report["metal_gpu_ms"]["scenes"][0]
                             ["candidate"]["p50"])

    def test_native_presentation_census_reports_fallback_causes(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "capture.csv"
            self.write_capture(
                path, "candidate", exact=0,
                presentation_state="capture_failed")
            capture = self.perf.load_capture(path)
            report = self.perf.summarize_presentation_census(
                capture, [s["id"] for s in self.spec["scenarios"]])
            self.assertTrue(report["available"])
            self.assertEqual(4, len(report["scenes"]))
            scene = report["scenes"][0]
            self.assertEqual(300, scene["attempted"])
            self.assertEqual(300, scene["gl_fallback"])
            self.assertEqual(300, scene["states"]["capture_failed"])
            self.assertEqual(1.0, scene["gl_fallback_rate"])

    def test_native_only_failure_does_not_report_gl_fallback(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "capture.csv"
            self.write_capture(
                path, "candidate", exact=0,
                presentation_state="capture_failed", gl_fallback=0)
            capture = self.perf.load_capture(path)
            report = self.perf.summarize_presentation_census(
                capture, [s["id"] for s in self.spec["scenarios"]])
            self.assertTrue(report["available"])
            scene = report["scenes"][0]
            self.assertEqual(300, scene["attempted"])
            self.assertEqual(0, scene["gl_fallback"])
            self.assertEqual(300, scene["states"]["capture_failed"])
            self.assertEqual(0.0, scene["gl_fallback_rate"])

    def test_inconsistent_native_presentation_census_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "capture.csv"
            self.write_capture(
                path, "candidate", exact=1,
                presentation_state="capture_failed")
            with self.assertRaisesRegex(ValueError, "inconsistent native"):
                self.perf.load_capture(path)


class AuditTests(unittest.TestCase):
    def audit_source(self, source, path="indra/llrender/renderer.cpp"):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / path
            target.parent.mkdir(parents=True)
            target.write_text(source)
            return load("audit_gl").audit(root)

    def test_gl_detected_and_absent_after_rewrite(self):
        module = load("audit_gl")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "indra/llrender"
            src.mkdir(parents=True)
            p = src / "renderer.cpp"
            p.write_text("void render(){ glBindFramebuffer(GL_FRAMEBUFFER, 0); }\n")
            self.assertGreater(module.audit(root)["finding_count"], 0)
            p.write_text("void render(){ encodeMetalCommands(); }\n")
            self.assertEqual(module.audit(root)["finding_count"], 0)

    def test_documented_strict_alias_fails_when_gl_remains(self):
        module = load("audit_gl")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            src = root / "indra/llrender"
            src.mkdir(parents=True)
            (src / "renderer.cpp").write_text(
                "void render(){ glBindFramebuffer(GL_FRAMEBUFFER, 0); }\n")
            with mock.patch.object(
                    sys, "argv",
                    ["audit_gl.py", "--strict", "--root", str(root)]):
                self.assertEqual(module.main(), 1)

    def test_native_class_inventory_does_not_weaken_actual_api_gate(self):
        module = load("audit_gl")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            src = root / "indra/llrender/llglslshader.cpp"
            src.parent.mkdir(parents=True)
            src.write_text("void LLGLSLShader::bind() { configureNative(); }\n")
            report = module.audit(root)
            self.assertEqual(1, report["finding_count"])
            self.assertEqual(1, report["informational_count"])
            self.assertEqual(0, report["blocking_count"])
            with mock.patch.object(sys, "argv", ["audit_gl.py", "--strict", "--root", str(root)]):
                self.assertEqual(0, module.main())
            src.write_text("void LLGLSLShader::bind() { glUseProgram(1); }\n")
            report = module.audit(root)
            self.assertEqual(1, report["blocking_count"])
            self.assertEqual(["gl_function"], report["findings"][0]["blocking_kinds"])
            self.assertEqual(["gl_shader"], report["findings"][0]["informational_kinds"])
            with mock.patch.object(sys, "argv", ["audit_gl.py", "--mode", "strict", "--root", str(root)]):
                self.assertEqual(1, module.main())
            with mock.patch.object(sys, "argv", ["audit_gl.py", "--root", str(root)]):
                self.assertEqual(0, module.main())

    def test_comments_strings_raw_literals_and_digit_separators_are_not_calls(self):
        report = self.audit_source(r'''
/* A multiline comment.
 glClear(0); SDL_GL_CreateContext(window);
*/
// glBindBuffer(0, 0); \
glFinish(); // C++ line splicing keeps this in the comment.
const char* message = "glClear(0); /* not a comment */";
const char* label = "glTF (*.gltf; *.glb)";
const char* shader = u8R"shader(
    glBindBuffer(0, 0); // source text, not a C++ API invocation
    " /* #include <OpenGL/gl.h> */
)shader";
const char slash = '/'; const char quote = '\'';
const auto size = 1'024; const auto mask = 0xffff'ffff;
void render(){ encodeMetalCommands(); }
''')
        self.assertEqual(0, report["finding_count"])
        self.assertEqual([], report["errors"])

    def test_real_calls_survive_urls_comments_multiline_and_spliced_tokens(self):
        report = self.audit_source(
            'const char* url = "https://host/path"; glClear /* gap */\n(0);\n'
            'glCl\\\near(0);\n'
            '#define CALL_GL glFinish\nCALL_GL();\n'
            'void draw() { (*glDrawArrays)(0, 0, 3); }\n')
        self.assertEqual(4, report["blocking_count"])
        self.assertEqual([1, 3, 5, 7], [row["line"] for row in report["findings"]])
        self.assertTrue(all("gl_function" in row["blocking_kinds"] for row in report["findings"]))

    def test_prototypes_contexts_macros_and_imports_block_without_a_call(self):
        samples = [
            "extern void glBindBuffer(unsigned, unsigned);",
            "extern PFNGLBINDBUFFERPROC dispatch;",
            "extern PFN_wglDXOpenDeviceNV dispatch;",
            "auto function = &glClear;",
            "GLXContext context; EGLContext other; HGLRC win; CGLContextObj mac;",
            "NSOpenGLContext* context; EAGLContext* mobile;",
            "auto attributes = WGL_CONTEXT_MAJOR_VERSION_ARB; auto pixel = kCGLPFAAccelerated;",
            "auto loader = &glfwGetProcAddress; int ready = glewInit();",
            "auto flag = SDL_WINDOW_OPENGL; auto prop = SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN;",
            "auto context = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_OPENGL_CONTEXT_POINTER, nullptr);",
            "void render() { eglSwapBuffers(display, surface); glXSwapBuffers(display, window); }",
            "#define CALL(name) gl ## name\nCALL(Clear)(0);",
            "#define FINISH g ## lFinish\nFINISH();",
            "#define CALL(name) g ## l ## name\nCALL(Clear)(0);",
            "#include /* between tokens */ <OpenGL/gl.h>",
            '#import "EGL/egl.h"',
            "#include <SDL3/SDL_opengl.h>",
            '#define GL_HEADER "GL/glcorearb.h"\n#include GL_HEADER',
            '#define API_HEADER <GL/glcorearb.h>\n#include API_HEADER',
            '#define H1 H2\n#define H2 <GL/gl.h>\n#include H1',
            '#define H <GL/gl.h>\n#if 0\n#define H "ordinary.h"\n#endif\n#include H',
            "@import OpenGL;",
            '#pragma comment(lib, "opengl32.lib")',
        ]
        for source in samples:
            with self.subTest(source=source):
                report = self.audit_source(source)
                self.assertGreater(report["blocking_count"], 0)
                self.assertEqual([], report["errors"])

    def test_dynamic_api_names_and_libraries_resolve_literals_and_local_aliases(self):
        samples = [
            'auto p = dlsym(handle, "gl" /* gap */ "Clear");',
            r'auto p = dlsym(handle, "\x67lClear");',
            r'auto p = dlsym(handle, "glClear\0ignored");',
            'auto p = dlsym(handle, R"name(glClear)name");',
            'const auto symbol = "glClear"; auto p = dlsym(handle, symbol);',
            'const char symbol[] = "glClear"; auto p = dlsym(handle, symbol);',
            'const char symbol[8] = "glClear"; auto p = dlsym(handle, symbol);',
            'const char symbol[] = "glClear"; if(symbol[0] == 0) return; auto p = dlsym(handle, symbol);',
            'const auto first = "glClear"; const auto copy = first; first = "ordinary"; auto p = dlsym(handle, copy);',
            'const auto symbol = "glClear"; auto p = dlsym(handle, symbol); symbol = "ordinary";',
            'const char* symbol = "glClear"; if(change){symbol = "ordinary";} auto p = dlsym(handle, symbol);',
            'const char* symbol = "glClear"; void unrelated(){const char* symbol = "ordinary";} auto p = dlsym(handle, symbol);',
            '#define SYMBOL "glClear"\n#if 0\n#define SYMBOL "ordinary"\n#endif\nauto p = dlsym(handle, SYMBOL);',
            '#define SYMBOL BASE\n#define BASE "gl" "Clear"\nauto p = dlsym(handle, SYMBOL);',
            '#define LOOKUP(symbol) dlsym(handle, symbol)\nauto p = LOOKUP("glClear");',
            '#define LOOKUP(name) dlsym(handle, "g" "l" #name)\nauto p = LOOKUP(Clear);',
            'auto lookup = dlsym; auto p = lookup(handle, "glClear");',
            'auto lookup = (&dlsym); auto p = lookup(handle, "glClear");',
            'auto p = dlsym(handle, std::string("glClear").c_str());',
            'std::string symbol = "glClear"s; auto p = dlsym(handle, symbol.c_str());',
            'auto p = GetProcAddress(handle, "wglCreateContext");',
            'auto p = CFBundleGetFunctionPointerForName(bundle, CFSTR("CGLCreateContext"));',
            'auto p = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));',
            'auto p = SDL_GL_GetProcAddress("glClear");',
            'auto library = dlopen("lib" "GL.so.1", flags);',
            r'auto library = dlopen("libGL.so.1\0ignored", flags);',
            'auto library = dlopen(std::string("libGL.so.1").c_str(), flags);',
            '#define LIB TEXT("opengl32.dll")\nauto library = LoadLibraryW(LIB);',
            'auto library = LoadLibraryEx(L"opengl32.dll", nullptr, 0);',
            'auto library = LoadLibraryW(L"opengl32");',
            'auto library = SDL_LoadObject("/System/Library/Frameworks/OpenGL.framework/OpenGL");',
        ]
        for source in samples:
            with self.subTest(source=source):
                report = self.audit_source(source)
                self.assertGreater(report["blocking_count"], 0)
                self.assertEqual([], report["errors"])
        report = self.audit_source(
            'auto p = dlsym(handle, "SDL_GetVersion");\n'
            'auto library = dlopen("libpulse.so.0", flags);\n'
            'const char* diagnostic = "glClear"; const char* format = "glTF";\n'
            '#include "native/MyGL/diagnostics.h"\n')
        self.assertEqual(0, report["blocking_count"])

    def test_cpu_helpers_are_exempt_by_exact_name_and_path_only(self):
        helpers = "void glPointToScreen(); void glRectToScreen();\n"
        for path in ("indra/llui/llui.h", "indra/llui/llui.cpp"):
            with self.subTest(path=path):
                report = self.audit_source(helpers, path)
                self.assertEqual(0, report["blocking_count"])
                self.assertIn("cpu_coordinate_helper", report["findings"][0]["informational_kinds"])
                report = self.audit_source(
                    "void glPointToScreen() { glClear(0); }\n", path)
                self.assertEqual(1, report["blocking_count"])
        self.assertEqual(1, self.audit_source(helpers)["blocking_count"])
        self.assertEqual(1, self.audit_source("void glPointToScreenExtra();", "indra/llui/llui.cpp")["blocking_count"])

    def test_all_first_party_runtime_directories_and_explicit_exclusions(self):
        module = load("audit_gl")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runtime = ["almetal", "llappearance", "llappearanceutility", "llcommon",
                       "llplugin", "media_plugins/cef", "llmath", "llrender"]
            excluded = ["llrender/tests", "llrender/test", "integration_tests",
                        "externals", "almetal/third_party", "newview/vendor", "dullahan/examples"]
            for path in runtime + excluded:
                target = root / "indra" / path / ("source.C" if path == "llmath" else "source.mm")
                target.parent.mkdir(parents=True)
                target.write_text("void render(){ glClear(0); }\n")
            (root / ".gitmodules").write_text('[submodule "dullahan"]\npath = indra/dullahan\n')
            report = module.audit(root)
            self.assertEqual(len(runtime), report["scanned_files"])
            self.assertEqual(len(runtime), report["blocking_count"])
            self.assertEqual([], report["errors"])
            reasons = {item["reason"] for item in report["scope"]["excluded_directories"]}
            self.assertEqual({"tests", "vendored", "submodule"}, reasons)
            report = module.audit(root, include_tests=True)
            self.assertEqual(len(runtime) + 3, report["blocking_count"])

    def test_scan_errors_cannot_be_reported_as_strict_zero(self):
        module = load("audit_gl")
        for source in ('/* glClear(0);', 'const char* text = R"x(unclosed;'):
            with self.subTest(source=source):
                self.assertGreater(self.audit_source(source)["error_count"], 0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(sys, "argv", ["audit_gl.py", "--strict", "--root", str(root)]):
                self.assertEqual(1, module.main())
            with mock.patch.object(sys, "argv", ["audit_gl.py", "--root", str(root)]):
                self.assertEqual(0, module.main())
            source = root / "indra/almetal/renderer.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("void render(){encodeMetalCommands();}\n")
            (root / ".gitmodules").write_text("[broken submodule metadata\n")
            report = module.audit(root)
            self.assertEqual(0, report["blocking_count"])
            self.assertGreater(report["error_count"], 0)
            with mock.patch.object(sys, "argv", ["audit_gl.py", "--strict", "--root", str(root)]):
                self.assertEqual(1, module.main())


class TraceSubmissionTests(unittest.TestCase):
    def test_resolves_compact_xctrace_references_and_sums_durations(self):
        module = load("trace_submissions")
        xml = b'''<?xml version="1.0"?>
<trace-query-result><node>
  <row><duration id="1" fmt="1 us">1000</duration>
    <duration id="2" fmt="0.75 us">750</duration>
    <narrative id="3" fmt='Committed &quot; Alpha &quot; with 1 encoders'/></row>
  <row><duration ref="1"/><duration ref="2"/><narrative ref="3"/></row>
  <row><duration id="4" fmt="2 us">2000</duration>
    <duration id="5" fmt="1.5 us">1500</duration>
    <narrative id="6" fmt='Committed &quot; Beta &quot; with 2 encoders'/></row>
  <row><duration id="7" fmt="1 us">1000</duration>
    <narrative id="8" fmt='Committed &quot; No Encoder &quot; with 0 encoders'/></row>
</node></trace-query-result>'''
        report = module.summarize_xml(xml, "fixture.xml")
        self.assertEqual(4, report["submission_count"])
        self.assertEqual(0.005, report["event_ms"])
        self.assertEqual(0.003, report["encoder_ms"])
        self.assertEqual("Alpha", report["labels"][0]["label"])
        self.assertEqual(2, report["labels"][0]["count"])
        self.assertEqual("Beta", report["labels"][1]["label"])
        self.assertEqual("No Encoder", report["labels"][2]["label"])
        self.assertEqual(1, report["labels"][2]["encoder_unreported"])
        self.assertIsNone(report["labels"][2]["mean_encoder_ms"])

    def test_rejects_xml_without_submission_rows(self):
        module = load("trace_submissions")
        with self.assertRaisesRegex(ValueError, "no committed"):
            module.summarize_xml(b"<trace-query-result/>")


class CUAPreflightTests(unittest.TestCase):
    def make_app(self, root):
        app = root / "Alchemy Metal Development.app"
        macos = app / "Contents/MacOS"
        macos.mkdir(parents=True)
        executable = macos / "Alchemy Metal Development"
        executable.write_bytes(b"test viewer\n")
        executable.chmod(0o755)
        with (app / "Contents/Info.plist").open("wb") as stream:
            plistlib.dump(
                {
                    "CFBundleExecutable": "Alchemy Metal Development",
                    "CFBundleShortVersionString": "26.4.0.99999",
                },
                stream,
            )
        return app, executable

    def valid_snapshot(self, module, root):
        app, executable = self.make_app(root)
        identity, identity_errors = module.app_identity(app)
        self.assertEqual([], identity_errors)
        profile = root / "isolated-profile"
        log_path = profile / "logs/Alchemy.log"
        process = module.Process(
            42, 1, str(identity["executable"]) + " --multiple --noaudio"
        )
        environment = f"{process.command} ALCHEMYNEXT_USER_DIR={profile} TERM=xterm"
        open_files = [str(log_path)]
        log_text = (
            f"Started logging to {log_path}\n"
            "Version: Alchemy Metal Development 26.4.0.99999\n"
        )
        return identity, profile, process, environment, open_files, log_text

    def test_exact_isolated_snapshot_passes(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            self.assertEqual(
                [],
                module.validate_runtime_snapshot(
                    identity, profile, [process], environment, open_files, log_text
                ),
            )

    def test_kernel_resolved_relative_executable_passes(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            relative = module.Process(
                process.pid,
                process.ppid,
                "build/newview/Release/Alchemy Metal Development.app/Contents/"
                "MacOS/Alchemy Metal Development --multiple --noaudio",
                str(identity["executable"]),
            )
            relative_environment = environment.replace(
                process.command, relative.command
            )
            self.assertEqual(
                [],
                module.validate_runtime_snapshot(
                    identity,
                    profile,
                    [relative],
                    relative_environment,
                    open_files,
                    log_text,
                ),
            )

    def test_kernel_resolved_wrong_executable_is_rejected(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            relative = module.Process(
                process.pid,
                process.ppid,
                "other/Alchemy Release.app/Contents/MacOS/Alchemy Release",
                str(Path(directory) / "other/Alchemy Release"),
            )
            errors = module.validate_runtime_snapshot(
                identity,
                profile,
                [relative],
                environment,
                open_files,
                log_text,
            )
            self.assertTrue(any("Expected one exact" in error for error in errors))

    def test_stale_sibling_viewer_is_rejected(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            sibling = module.Process(
                43, 1,
                "/Applications/Alchemy Release.app/Contents/MacOS/Alchemy Release",
            )
            errors = module.validate_runtime_snapshot(
                identity,
                profile,
                [process, sibling],
                environment,
                open_files,
                log_text,
            )
            self.assertTrue(any("census" in error for error in errors))

    def test_firestorm_sibling_viewer_is_rejected(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, environment, open_files, log_text = values
            sibling = module.Process(
                43,
                1,
                "/Applications/Firestorm-Releasex64.app/Contents/MacOS/Firestorm",
            )
            errors = module.validate_runtime_snapshot(
                identity,
                profile,
                [process, sibling],
                environment,
                open_files,
                log_text,
            )
            self.assertTrue(any("census" in error for error in errors))

    def test_ordinary_profile_or_missing_environment_is_rejected(self):
        module = load("cua_preflight")
        with tempfile.TemporaryDirectory() as directory:
            values = self.valid_snapshot(module, Path(directory))
            identity, profile, process, _, open_files, log_text = values
            normal = Path.home() / "Library/Application Support/AlchemyNext/logs/Alchemy.log"
            errors = module.validate_runtime_snapshot(
                identity,
                profile,
                [process],
                process.command,
                open_files + [str(normal)],
                log_text,
            )
            self.assertTrue(any("isolated-profile root" in error for error in errors))
            self.assertTrue(any("ordinary Alchemy profile" in error for error in errors))

    def test_profile_root_inside_ordinary_tree_is_rejected(self):
        module = load("cua_preflight")
        identity = {
            "executable": Path("/tmp/Alchemy Metal Development.app/Contents/MacOS/Alchemy Metal Development"),
            "channel": "Alchemy Metal Development",
            "version": "26.4.0.99999",
        }
        profile = Path.home() / "Library/Application Support/AlchemyNext/validation"
        process = module.Process(42, 1, str(identity["executable"]))
        errors = module.validate_runtime_snapshot(
            identity,
            profile,
            [process],
            f"{process.command} ALCHEMYNEXT_USER_DIR={profile}",
            [str(profile / "logs/Alchemy.log")],
            (
                f"Started logging to {profile / 'logs/Alchemy.log'}\n"
                "Version: Alchemy Metal Development 26.4.0.99999\n"
            ),
        )
        self.assertTrue(any("profile root" in error for error in errors))


class GraphicsSettingsTests(unittest.TestCase):
    def setUp(self):
        self.module = load("graphics_settings")

    @staticmethod
    def write_settings(path, values):
        records = []
        for name, value in values.items():
            if isinstance(value, bool):
                encoded = f"<boolean>{str(value).lower()}</boolean>"
                value_type = "Boolean"
            elif isinstance(value, int):
                encoded = f"<integer>{value}</integer>"
                value_type = "S32"
            elif isinstance(value, float):
                encoded = f"<real>{value}</real>"
                value_type = "F32"
            else:
                encoded = f"<string>{value}</string>"
                value_type = "String"
            records.append(
                f"<key>{name}</key><map>"
                f"<key>Type</key><string>{value_type}</string>"
                f"<key>Value</key>{encoded}</map>"
            )
        path.write_text("<llsd><map>" + "".join(records) + "</map></llsd>")

    def complete_values(self):
        values = {name: 1 for name in self.module.REQUIRED_SETTINGS}
        values["RenderMirrors"] = True
        values["RenderTransparentWater"] = False
        return values

    def test_profile_overrides_defaults_and_ignores_backend_setting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            defaults = root / "defaults.xml"
            profile = root / "profile.xml"
            values = self.complete_values()
            values["RenderHeroProbeResolution"] = 256
            values["AlchemyExposureCompensation"] = -1.0
            values["RenderShaderCacheVersion"] = "reference-cache"
            values["RenderMetalNativeOnly"] = True
            self.write_settings(defaults, values)
            self.write_settings(
                profile,
                {
                    "RenderHeroProbeResolution": 512,
                    "AlchemyExposureCompensation": 0.5,
                    "RenderShaderCacheVersion": "candidate-cache",
                },
            )
            manifest = self.module.build_manifest([defaults], profile, "fixture")
            self.assertEqual(512, manifest["settings"]["RenderHeroProbeResolution"])
            self.assertEqual(0.5, manifest["settings"]["AlchemyExposureCompensation"])
            self.assertNotIn("RenderShaderCacheVersion", manifest["settings"])
            self.assertNotIn("RenderMetalNativeOnly", manifest["settings"])

    def test_visual_mismatch_fails_comparison(self):
        settings = self.complete_values()
        reference = {
            "schema_version": 1,
            "label": "reference",
            "settings": dict(settings),
        }
        candidate = {
            "schema_version": 1,
            "label": "candidate",
            "settings": dict(settings),
        }
        candidate["settings"]["RenderQualityPerformance"] = 0
        report = self.module.compare_manifests(reference, candidate)
        self.assertFalse(report["comparable"])
        self.assertEqual("RenderQualityPerformance", report["mismatches"][0]["setting"])

    def test_identical_visual_settings_pass(self):
        settings = self.complete_values()
        reference = {"label": "reference", "settings": dict(settings)}
        candidate = {"label": "candidate", "settings": dict(settings)}
        report = self.module.compare_manifests(reference, candidate)
        self.assertTrue(report["comparable"])
        candidate["settings"]["RenderFutureQualitySwitch"] = 7
        report = self.module.compare_manifests(reference, candidate)
        self.assertFalse(report["comparable"])
        self.assertEqual(
            ["RenderFutureQualitySwitch"], report["candidate_only"]
        )


class HarnessTests(unittest.TestCase):
    def test_initial_harness_contract(self):
        module = load("validate_harness")
        self.assertEqual([], module.validate(ROOT))
        # Release must not pass when nothing is validated.
        self.assertTrue(module.validate(ROOT, release=True))

    def test_stale_repository_command_path_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "AGENTS.md").write_text(
                "Run `tools/metal/verify-app.sh app`.\n", encoding="utf-8"
            )
            errors = module.validate_live_references(root, ["AGENTS.md"])
            self.assertTrue(any("verify-app.sh" in error for error in errors))

    def test_stale_document_relative_repository_path_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            skill = root / ".agents/skills/example/SKILL.md"
            skill.parent.mkdir(parents=True)
            skill.write_text(
                "Read `benchmarks/scenarios.json`.\n", encoding="utf-8"
            )
            errors = module.validate_live_references(
                root, [".agents/skills/example/SKILL.md"]
            )
            self.assertTrue(any(
                "benchmarks/scenarios.json" in error for error in errors
            ))

    def test_feature_evidence_path_and_anchor_are_validated(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            evidence = root / "docs/evidence.md"
            evidence.parent.mkdir(parents=True)
            evidence.write_text("# Exact checkpoint\n", encoding="utf-8")
            self.assertEqual([], module.validate_evidence_reference(
                root, "docs/evidence.md#exact-checkpoint", "fixture"
            ))
            errors = module.validate_evidence_reference(
                root, "docs/evidence.md#old-checkpoint", "fixture"
            )
            self.assertTrue(any("Stale evidence anchor" in error
                                for error in errors))
            errors = module.validate_evidence_reference(
                root, "docs/missing.md#exact-checkpoint", "fixture"
            )
            self.assertTrue(any("Missing evidence path" in error
                                for error in errors))

    def test_referenced_command_must_be_executable(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            script = root / "tools/metal/check.sh"
            script.parent.mkdir(parents=True)
            script.write_text("#!/bin/sh\n", encoding="utf-8")
            script.chmod(0o644)
            (root / "AGENTS.md").write_text(
                "Run `tools/metal/check.sh`.\n", encoding="utf-8"
            )
            errors = module.validate_live_references(root, ["AGENTS.md"])
            self.assertIn(
                "Referenced command is not executable: tools/metal/check.sh",
                errors,
            )

    def test_stale_markdown_link_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            guide = root / "docs/guide.md"
            guide.parent.mkdir(parents=True)
            guide.write_text("[missing](absent.md)\n", encoding="utf-8")
            errors = module.validate_live_references(root, ["docs/guide.md"])
            self.assertEqual(
                ["Stale Markdown link in docs/guide.md: absent.md"], errors
            )

    def test_release_invariant_pattern_loss_is_rejected(self):
        module = load("validate_harness")
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            metal = root / "docs/metal"
            metal.mkdir(parents=True)
            (metal / "PRODUCT.md").write_text("immutable alpha\n", encoding="utf-8")
            (metal / "VALIDATION.md").write_text("coverage beta\n", encoding="utf-8")
            (metal / "release-invariants.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "source": "docs/metal/PRODUCT.md",
                        "invariants": [
                            {
                                "id": "fixture",
                                "source_patterns": ["alpha"],
                                "coverage": {
                                    "docs/metal/VALIDATION.md": ["gamma"]
                                },
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            errors = module.validate_release_invariants(root, required_ids=set())
            self.assertTrue(any("coverage missing" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
