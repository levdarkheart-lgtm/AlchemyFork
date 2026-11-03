# Darwin material-preview GL recovery removal checkpoint

**Observed:** 2026-11-03 on the physical Apple Silicon acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Source:** `e08926889393999c82a5751ec703e3b7d2041a7c`  
**Build:** Alchemy Metal Development `26.4.0d68190` (`26.4.0.68190`)

## Implemented boundary

Commit `e089268893` removes the Darwin material-preview retained recovery
transaction without changing retained-platform behavior:

- `LLGLTFPreviewTexture::renderWithoutTarget()` returns only `PENDING` or
  `COMPLETED` on Darwin. Texture/default-probe readiness, device absence,
  destination/geometry failure, encode failure and publication failure all
  retry through Metal; none returns `RETAINED_FALLBACK`.
- Darwin `resourceInputsFromSettings()` never sets material-preview demand, so
  a preview cannot plan the retained probe/post pack.
- The retained sphere draw, GL state transaction, exposure/bloom/color-correct
  chain and final framebuffer copy compile only for non-Darwin platforms.
- The Darwin direct `postRender()` path is fail-closed: it publishes only an
  existing Metal result and cannot generate GL backing or copy a framebuffer.
- Obsolete fallback counters, flags and one-shot scheduler state were removed.

This completes one owner boundary, not the product. Other dynamic textures,
scene traversal, pipeline lifetime and the packaged OpenGL dependency remain.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on macOS 26.5.2, arm64
  `MacBookPro18,2`, Apple M1 Max, 64 GiB unified memory, Apple7 support and
  `recommendedMaxWorkingSetSize=62277025792`.
- Exact Release and RelWithDebInfo preview-manager, dynamic-texture and pipeline
  objects compiled warnings-as-errors. `tools/metal/build.sh` completed and
  staged source-exact build 68190.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` checked 349/349 Mach-O files as ARM64.
- Viewer executable SHA-256:
  `2ad4fb7ceb0929cfa634e6a8c89a53ae19f1633ad0734e22034459ee8aae45b3`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- Under `MTL_DEBUG_LAYER=1`, shader validation, load-action validation and
  `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, focused `*preview*` tests passed 459
  assertions across two cases. The full physical Metal suite passed 52,414
  assertions across 55 cases.
- Focused resource-plan tests passed 3,376 assertions across 37 cases; full
  `alrender_tests` passed 557,312 assertions across 137 cases.
- Source-exact `newview_tests` reported build 68190 and passed 98,973 assertions
  across 743 cases: 742 passed and the established CRT case skipped.
- `nm` on the exact Release Darwin preview object finds no retained preview
  shader/post globals, bloom/exposure/color-correction calls, GL batch
  submission, transient resource acquisition, retained readiness or fallback
  symbols. It does find the Metal preview renderer, native texture adoption and
  Metal default-probe publication.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-native-preview-recovery-cutover.json` exited 1 as
  expected at 2,408 scanned files / 3,262 findings. Report SHA-256:
  `8bc964c6635d54db8d77a2a82a83e1d7ba99a99605b63f366fc3228c48413968`.
  The packaged viewer still links both OpenGL and Metal.
- `git diff --check` passed.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Queue row
17 now names exact build 68190 and records native-only failure retry. Visual
parity remains open for the owner-attended daily review.

## Next executable action

Inventory and separate broader native pipeline lifetime from full GL teardown,
then remove the next retained lifetime dependency without weakening native
fail-closed behavior.
