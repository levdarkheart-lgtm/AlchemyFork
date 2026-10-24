# Current engineering state (recovery snapshot)

**As recorded:** 2026-10-24 on the physical acceptance Mac.

- Integration branch: `codex/apple-silicon-metal`, based on upstream/fork revision `65c39b7bad928e165f1acdc0cbf39478e017ef1d`; harness commit `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45` is pushed to `origin/codex/apple-silicon-metal`.
- Host: MacBookPro18,2, Apple M1 Max, 68,719,476,736 bytes RAM, macOS 26.5.2, native arm64, Xcode 26.6 (17F113), CMake 4.4.4.
- Metal device: Apple M1 Max, unified memory, registry ID 4294970146, recommended working set 62,277,025,792 bytes; Apple GPU family 7 supported, families 8 and 9 not supported.
- Harness validation: 25 required files present; six Python harness tests pass.
- Open-source dependency configure: documented `ninja-os-arm64` preset succeeds after installing build prerequisites `autoconf`, `autoconf-archive`, `automake`, `libtool`, `nasm`, and `ninja`. All 171 arm64 vcpkg packages install without private registry access.
- Reference Release build: succeeds in `build-Darwin-ninja-os-arm64`. The current Apple linker does not implement `-duplicate_symbols error`; CMake now probes the flag and preserves linker defaults when unavailable.
- Reference bundle: version 26.4.0.67604, executable SHA-256 `6e40c02653b7b4b74bbf43048a159d4b1e7cde390f9f540448faff845ed8236a`. Deep code-sign verification passes. The complete bundle contains 349 Mach-O files, all arm64; the verifier now scans embedded media apps and frameworks as well as the top-level executable.
- Reference launch: a fresh isolated profile at `/tmp/alchemy-metal-isolated.6ErMuQ` reached `STATE_LOGIN_WAIT`, rendered the login UI, honored `--quitafter 8`, exited 0, and shut down normally. Runtime renderer is `GL: 4.1 Metal - 90.5`; GL compute is unavailable because it requires 4.3. No account login was attempted.
- Tests: Catch2 3.16.1 configured 565 entries. The architecture-neutral run (`-LE 'manual|gl'`) passed all 461 enabled tests; 15 Python/LLCoreHTTP entries remain disabled by the upstream configuration. The wider run passed 507 of 510 enabled tests. Its three failures are confined to the macOS GL 4.1 reference: no clip-control for the DoF reverse-Z fixture, driver-normalized `GL_DEPTH_COMPONENT24` storage, and Apple GL fence timing that does not remain busy as the timing test assumes.
- macOS test isolation: `ALCHEMYNEXT_USER_DIR` was Linux-only despite being the documented testing contract. macOS now honors it for user settings, logs, cache, data, and browser profile. The first pre-fix smoke run entered the owner's normal AlchemyNext profile and its normal version migration cleared the re-downloadable texture/object cache; subsequent runs are isolated.
- OpenGL census at harness SHA: 3,080 findings in 135 files out of 2,404 scanned files: 2,304 shader references, 706 GL functions, and 72 SDL GL references. Areas: `newview` 1,731; `llrender` 1,176; `llwindow` 121; `alrender` 41; `llui` 11. Ignored raw artifact: `artifacts/metal/gl-audit-baseline-1eb44bbd1b.json`, SHA-256 `e0bbd2709d006f3fa397e7194bec4e710d0d9e5b70fd86cb4fd2cb7dc50a9dad`.
- Native Metal implementation: not yet a usable renderer. The verified presentation seam is the SDL3 macOS window, `LLWindowSDL::createContext`, and the three `LLWindow::swapBuffers` call sites in `llviewerdisplay.cpp`. SDL 3.4.18 supplies `SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN`, `SDL_Metal_CreateView`, and `SDL_Metal_GetLayer`; user code must attach the `MTLDevice` to the `CAMetalLayer`.
- Feature matrix: login-screen reference bootstrap is observed; authenticated reference and candidate core feature coverage remains unverified.
- Performance: no admissible 300-frame paired samples yet; no speedup is claimed.

## Exact next actions

1. Add the production Metal device/queue and SDL `CAMetalLayer` lifecycle, with real GPU tests for command completion and resource ownership; then move frame presentation off `SDL_GL_SwapWindow`.
2. Continue the resource/pass/shader graph census while implementing Metal buffers, textures, targets, explicit passes, and MSL pipelines. Keep the same-source GL executable only as an external reference.
3. Preserve isolated-profile launch, full-bundle slice verification, Metal validation, feature-matrix evidence, and paired performance capture throughout the conversion.

## Open constraints, not waivers

- Xcode's generator currently cannot load `IDESimulatorFoundation` until the machine completes `xcodebuild -runFirstLaunch` with administrator authorization. Ninja is a documented, fully working arm64 preset, so this does not block engineering.
- The reference bundle identifier is still `org.alchemyviewer.viewer`; the release candidate requires a distinct identity.
- CEF carries its own Vulkan/SwiftShader libraries in its out-of-process framework. This is third-party browser subprocess content, not the viewer render backend; runtime process/API auditing is still required and the distinction must remain explicit.
