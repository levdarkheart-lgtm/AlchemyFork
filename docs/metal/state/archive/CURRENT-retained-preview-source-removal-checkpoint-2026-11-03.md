# Retained material-preview source-removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer source:** `8ba316c6de` (`Delete retained material preview renderer`)

## Implemented cutover

- The Apple-only fork no longer carries the retained GL material-preview sphere
  renderer or its framebuffer post chain. The deletion removes retained shader
  binding, GL batch submission, environment-UBO mutation, transient preview
  targets, exposure/luminance/bloom/color-correction/glow and framebuffer-copy
  code.
- The retained readiness/live-count/resource-dirty machinery is also gone.
  Preview dimensions still come from the established probe-resolution settings,
  but they size only the Metal destination and do not demand a retained pack.
- The targetless scheduler remains fail closed: native readiness and all native
  failure classes stay pending and retry Metal. The non-Darwin stub is pending;
  it is not an alternate renderer in this macOS ARM64-only product.
- Pipeline resource inputs now set `mMaterialPreview` false unconditionally, so
  the retained preview probe/post resources cannot be planned.
- The source change is 18 insertions and 289 deletions across the preview manager
  implementation/header and pipeline resource-input census.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on `MacBookPro18,2`: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` completed the warnings-as-errors Xcode Release build at
  committed source `8ba316c6de`. It linked, signed, validated and staged Alchemy
  Metal Development `26.4.0d68192` (`26.4.0.68192`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O files as ARM64.
- Viewer executable SHA-256:
  `d5f8fb634b0570e67e7b7ccf3a05eccf3fda6b7db7ba7ffb790806f59ff79f66`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API, GPU, shader and load-action validation enabled, focused
  `*preview*` coverage passed 459 assertions across two cases; the complete
  physical Metal suite passed 52,414 assertions across 55 cases.
- Focused resource-plan coverage passed 3,376 assertions across 37 cases; full
  `alrender_tests` passed 557,312 assertions across 137 cases.
- Source-exact `newview_tests` reported build 68192 and passed 98,973 assertions
  across 743 cases: 742 passed and the established CRT case skipped.
- Exact Release-object inspection finds no retained preview shader/post globals,
  bloom/exposure/color-correction calls, GL batch submission, transient target
  acquisition, framebuffer copy, GL-matrix or fallback symbols.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-preview-source-removal.json` exited 1 as expected at
  2,408 scanned files / 3,255 findings. This is seven fewer findings than the
  preceding 3,262 checkpoint without changing the audit rules or suppressions.
  Report SHA-256:
  `6d277de0e174e9df2a2c3d05c0254a69d4077e295557083ddc4ba5c98397be73`.
- The packaged viewer still links both OpenGL and Metal. This checkpoint removes
  one retained renderer; it does not satisfy the linked or runtime no-GL gates.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Queue row
17 now names exact build 68192 and remains open for the owner-attended daily
review. No visual parity inference was made from deterministic tests.

## Next executable action

Inventory and separate broader native pipeline lifetime from full GL teardown.
Preserve manager-owned Metal probes and histories while removing only the next
source-proven retained lifetime dependency; do not globally disable teardown.
