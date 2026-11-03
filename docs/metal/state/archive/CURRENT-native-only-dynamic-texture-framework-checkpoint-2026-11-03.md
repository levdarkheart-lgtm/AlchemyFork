# Native-only dynamic-texture framework checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.

**Branch:** `codex/apple-silicon-metal`

**Renderer source:** `f013ba55de` (`Make dynamic textures native-only`)

## Replacement-first boundary

This commit removes the shared retained-OpenGL dynamic-texture framework only
after every concrete producer had a validated native Metal implementation:

- PBR material previews;
- animation-upload previews;
- avatar and sculpt image-upload previews;
- model-upload previews;
- appearance-editor visual-parameter hints and reset ordering; and
- avatar composite/bake capture, morph readback and publication.

Their feature renderers, retry rules and authoritative publication remain in
place. In particular, this is not a shortcut that deletes previews, avatar
bakes or appearance editing to reduce an OpenGL count.

`LLViewerDynamicTexture` now creates native backing for the supported Darwin
product and fails fast on unsupported platforms. Its producer contract is
purely targetless and can report only `COMPLETED` or `PENDING`. The scheduler
preserves preview, bake and reset order while no longer allocating, binding,
clearing, flushing or copying a retained target. The base GL texture allocator,
camera/viewport transaction, framebuffer publication, retained target pool,
fallback result, backing selector and success/bound-target state are deleted.

Concrete producers no longer carry redundant backing selectors, targetless
capability flags or retained `postRender()` hooks. Direct avatar-composite
updates use the same targetless native transaction as scheduled updates. The
avatar composite keeps its producer-owned native success state so cancellation,
readback and publication behavior remain explicit.

The source change touches 14 files, adds 50 lines and deletes 509. All concrete
subclasses compile against the pure virtual targetless/render contract.
`RETAINED_GL`, `RETAINED_FALLBACK`, the removed base pre/post methods and their
state have no `indra/newview` source matches; the packaged executable exports
none of the removed base methods.

## Exact build and test evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `f013ba55de`. The
  warnings-as-errors Xcode Release build reported `BUILD SUCCEEDED` and staged
  Alchemy Metal Development `26.4.0d68221` (`26.4.0.68221`).
- `tools/metal/verify_app.sh` exited 0 after checking 349/349 bundled Mach-O
  files as ARM64; deep strict code-sign verification exited 0.
- Viewer executable SHA-256:
  `a0c68a0a0cc2bffa7e29551b4ca2d6f77627e7801b4a3e0ac34792d1c15ad009`.
- Source and packaged MSL are byte-identical at
  `6fcd3271ea0fed0a7dde4fec6e66ce71a8ec3fd1b43d4637f850ac8717af8110`;
  packaged metallib SHA-256 is
  `024aba5a70e01ce501c884535ad53f438d626d756c7c2148ee08a1a7a2aacc2d`.
- The exact Ninja warnings-as-errors tree reconfigured to build 68221 and
  revision `f013ba55de`, then linked `almetal_tests`,
  `llrender_gl_fresh_tests`, `llrender_gl_shared_tests`,
  `llappearance_gl_tests`, `alrender_tests`, `newview_tests` and
  `alchemy-bin`. A second exact target invocation exited 0 with no work.
- With Metal API/GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,468 assertions across 55 cases;
  `llrender_gl_fresh_tests` passed 2,612/96;
  `llappearance_gl_tests` passed 13,207/3; and
  `alrender_tests` passed 557,312/137.
- `newview_tests` self-reported build 68221 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- The complete shared-state diagnostic suite remains transparently non-green
  at 8,352/8,357 assertions, 182 passed, 4 failed and 39 skipped across 225
  cases. Its four legacy/headless frame-fence timing assertions and one
  depth-format expectation reproduce the established failure signature.

## Strict inventory result

- `python3 tools/metal/audit_gl.py --mode strict --output
  artifacts/metal/gl-audit-dynamic-texture-native-only-strict.json` exits 1
  while other application-owned GL remains. The exact report is 2,408 files /
  3,241 findings / 2,930 unique path/kinds/snippet records.
- This is one finding and one unique record below the preceding animation
  routing checkpoint; no audit rule or suppression changed. Report SHA-256:
  `2e49780865d5599289a1e4fad57500fff5c5ad5e51ef61d90257a4ca26466a9b`.
- The executable still links both OpenGL and Metal. The static, linked and
  runtime no-OpenGL release gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. Existing
owner-attended daily-queue rows cover material, avatar composite, appearance
hint, image-upload, animation-upload and model-upload previews. This framework
cleanup does not promote any queued row to visual parity.

## Next executable action

Census the next live application-owned Darwin GL producer and its full
user-visible responsibilities. Implement and validate equivalent native Metal
behavior before retiring that producer; do not trade functionality for a lower
inventory count.
