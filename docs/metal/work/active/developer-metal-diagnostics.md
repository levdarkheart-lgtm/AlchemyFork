# Developer diagnostics: native next-run validation and deliberate driver crash

## Reference mapping before implementation (2026-11-11)

Original revision `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`:
`menu_viewer.xml` exposes a one-shot next-run graphics debugging switch via
`RenderDebugGLSession`; `LLAppViewer::initConfiguration()` enables strict
graphics diagnostics and consumes the saved switch before `initWindow()`.
The native window creates its sole `ALMetalDevice` in its constructor.
The Metal equivalent must request Apple's API validation before that first
device creation, retain one-session consumption, and leave normal launches
unchanged. No original equivalent exists for the native environment mechanism.
A standalone Objective-C++ probe on this M1 Max confirmed setting
`MTL_DEBUG_LAYER=1` in `main()` before `MTLCreateSystemDefaultDevice()` produces
`MTLDebugDevice` and the message `Metal API Validation Enabled` (exit0).
The sandboxed first probe enabled validation but found no device (exit3);
the approved standalone rerun passed. No viewer was started.

The same revision's `LLAppViewer::forceErrorDriverCrash()` deliberately calls
`glDeleteTextures(1, NULL)` to fault inside the graphics API for crash reporting.
Current Darwin has no GL driver; its null GL function pointer instead faults
before entering a driver. The native equivalent is deliberate invalid CPU
client memory in a Metal buffer creation call, followed by process abort if
the runtime unexpectedly returns. It must never submit a hanging GPU workload.
`LLAppViewer` routes this explicit Develop > Force Driver Crash action to the
window-owned device; no ordinary rendering path calls it.

## Validation / handoff

Implemented `RenderDebugMetalSession` and the Develop menu/localized node
renames. Startup consumes both the new one-shot setting and the legacy setting
(migration only); `--debugsession` also selects native API validation.
`MTL_DEBUG_LAYER=1` is requested before `initWindow()`. External shader/capture
environment controls are not changed. Existing viewer-side strict diagnostic
globals remain connected. Full app startup/settings persistence is source
reviewed and awaits root's build; it was not exercised in a viewer process.

The explicit Force Driver Crash action now calls the window-owned native
device's deliberately invalid CPU-memory buffer creation and aborts if it
returns. No OpenGL call remains in that action. The crash action itself was
**not executed**; the integrator explicitly directed source/build checks only.

Commands on M1 Max/macOS26.5.2, source base `bccbb7be48` plus working changes:

- `xcrun clang++ -std=c++17 -fobjc-arc -fblocks -Werror=nonnull -framework Foundation -framework Metal /tmp/alchemy-metal-debug-layer-probe.mm indra/almetal/almetaldevice.mm -o /tmp/alchemy-metal-debug-layer-probe`
  exited0. Probe source directly uses the changed `ALMetalDevice::create()`.
- `/tmp/alchemy-metal-debug-layer-probe enable` outside the sandbox exited0,
  logging `Metal API Validation Enabled` and class `MTLDebugDevice`.
- `env -u MTL_DEBUG_LAYER /tmp/alchemy-metal-debug-layer-probe` exited0 with
  native class `AGXG13XDevice`, confirming opt-in validation in the control.
- `xmllint --noout` on settings.xml, cmd_line.xml and all11 changed menu locale
  XML files exited0; `git diff --check` exited0.

Standalone probe source and executable are temporary local artifacts under
`/tmp/`; no screenshots, login, live driver-crash test or viewer launch occurred.
Broader Debug/Developer feature parity remains open; this bounded change proves
native API validation selection, not live menu interaction or full renderer
debug-view coverage. Root owns app build, integration and canonical evidence.
