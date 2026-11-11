# Native source tail removal — 2026-11-11

**Source-gate implementation checkpoint:** `5d55f9a202`. Goal remains active. No viewer, menu,
appearance utility, window, screenshot or user-cache operation was executed.
Latest frozen canonical d28b0477a0/26.4.0d68712 includes cache/coverage work;
[exact current receipts](native-pipeline-cache-and-coverage.md) supersede older
build/cache status below. Canonical/appearance build/signature/import/ARM64 checks0.

## Removed ownership and preserved behavior

210fdb8d76 removes unsupported Win32/WGL renderer, DX/WMI, keyboard and drag/drop
backend ownership after the macOS/ARM64-only build gate. Original1eb44 and
configured native compilation confirm no macOS producer was removed. Direct
references, dead factory/message-box/fonts/display branches and ten WGL pointer
pairs are removed. Actual SDL keyboard, IME, AltGr, clipboard, drag/drop,
headless, Cocoa-handle, CEF/IOSurface and voice owners remain. Nine Release syntax
checks0 and independent review pass. Source-body projections/hashes:
`/private/tmp/alchemy-win32-graphics-cleanup-census.json` and syntax receipts.
3620139916 removes separate dead Windows Vulkan loader/telemetry; active native
identity/VRAM/schema code compares byte-identical and Release syntax0.

1b72fd4912/b3b138074e retire displaced generic instanced/indirect/compute/barrier
wrappers, uncalled water bind/draw helpers and exact obsolete fixture sections.
Typed native owners remain; three native water snapshot bodies compare identical.
sCompute and real CPU schema retain live consumers. Eight syntax checks0 and
independent review pass.2ac9a20ed4 removes an inherited empty shader-reload
callback with no XML entry/caller; real native rebuild/cache controls remain.

fe9cf22c68 replaces external GL headers with162 numeric CPU tokens and six
scalar aliases used by actual native source/header/test closure. These describe
pixel formats, logical state and semantic stages, with no GL imports, prototypes,
PFN/APIENTRY or driver feature flags. Remove ll::glext and direct opengl-registry.
Old/new probes produce identical5661bytes, SHA256
`6fa94aa4a58defca2fac913def985fe094dea180430721ec5efaee2ee6088b62`;
values, expression types and scalar size/alignment/types match. Thirteen original
literal checks0;22 Release syntax plus four final checks0. Complete receipts:
`/private/tmp/alchemy-native-cpu-render-vocabulary-validation/`.

6a0831ad88 excludes the orphan GL UI fixture object whose binary was already
excluded. Native UI checks remain. Regenerated Release database:2241rows, zero
LLUI_TEST_GL rows and zero deleted Win32 owner rows. Do not import obsolete
fixture tokens into the native vocabulary.

## Source gate precision and limits

5d55f9a202 preserves inventory markers but distinguishes native class names from
GL dependencies. Strict fails API/context/header/prototype/loader blockers or
incomplete scans. All first-party indra runtime C/C++/Objective-C directories and
all preprocessor branches are scanned, with explicit vendor/submodule/test
exclusions. Only two CPU UI coordinate helpers are exempt by exact name/path;
actual API calls in their bodies or compatibility classes still fail.

The lexer covers comments, raw/escaped/adjacent literals, splices, multiline
calls, pointers/prototypes, macro/header aliases and conservative loader literal
candidates. It retains possible values across unmodeled branches/scopes. It
does not prove arbitrary computed names, interprocedural dataflow, runtime
contexts, linking or foreign media safety. Original grep counts are not directly
comparable with this expanded scope/severity model. Informational rows remain
visible and are not reported as executable API calls.

Integrated root strict audit0:3507files/0blocking/0scanerrors/1188informational.
39tool tests and harness0. Twenty old/fixed CLI expectations and16 independent
adversarial checks pass; initial red receipts retained. Fixed gaps include
char-array aliases, macro imports, parenthesized loader pointers, NUL/wrapper
strings and redefinitions erasing prior candidates. Source-zero is not product
or release completion.

## Integrated verification

Receipts: `artifacts/metal/feature-completion-20261111/`.

| Check | Observed result |
| --- | --- |
| cpu-vocabulary-native-test-build.log | Dependencies reconfigured; almetal_tests,llrender_tests,alrender_tests,llui_test_checks rebuilt0 |
| cpu-vocabulary-llrender-tests.log; API/shader validation | 67793assertions/49cases pass |
| cpu-vocabulary-native-gpu-tests.log; API/shader validation | 93869assertions/22cases pass: reload/profile/benchmark/water/compute/indirect shadows |
| native-build-gates.json | SDL-off/foreign GL-demo rejected1 as expected; normal native configure restored0 |
| native-audit-tools-tests.log; native-audit-harness.log | 39tests0;32requiredfiles validated0 |
| native-source-strict-audit.json/log | Expanded source gate0,3507files/0blockers/0errors/1188informational |

Filters overlap previous receipts; do not sum historical totals. No performance
or live menu/visual parity claim. Product stability/release review remain open.

## Remaining executable work

Native Shader Cache owner dd9372cd69 and root policy53e3599a36 are integrated.
Persistence/retry/purge and actual instrumentation cases now pass. Exact current
receipts in the latest note supersede the historical partial-test status.
Apple documents [Shader Validation's binary-archive incompatibility](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-shader-usage.md).
Keep actual instrumentation and PSO-state checks; report real cache probes/
bypasses and isolate diagnostic namespaces, never fabricate hits.

Continue native target/font invariant admission and reproduced target repairs.
Retire excluded GL fixtures only after native coverage keeps every meaningful
invariant. No viewer launch.
