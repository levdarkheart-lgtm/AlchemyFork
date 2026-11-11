# Post-launch source corrections — 2026-11-11

Integrated renderer checkpoint `8a14710e80`, following the built/started
`f254feb048` checkpoint in [settings/resource tail](native-settings-and-resource-tail.md).
Original source remains `1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`.
The goal is active. Owner provisionally passed26.4.0d68817: “viewer i tested seems
ok...i would rate it pass...for now”. This is an observation about that build,
not a scenario-specific parity/stability/performance result or these later fixes.

## Preserve the passed build

Its running path is unchanged. A separate local baseline copy contains all
7,840 identical files/symlink entries and passes deep strict signature checking.
Binary SHA256 remains
`285c561302672cb9fab2fe4ab4ef402297796ca2272b579765d57b3abd4ab1eb`.
Receipt: `artifacts/metal/feature-completion-20261111/manual-pass-baseline-receipt.json`.
Do not rebuild over, restart or operate the passed viewer unasked. No new viewer,
window, utility or screenshot action occurred during these corrections.

## Inherited debug-feature mask defect

Integrated9838d1fc5c from child973677de805. Original/current
setRenderDebugFeatureControl used `mask &= !bit`: disabling any nonzero feature
cleared all features. RenderPerformanceTest's live callback disables UI, then
re-enables only UI, leaving dynamic textures, flexible geometry and selection
disabled. This is an inherited bug, not a lost Metal implementation.

Replace only logical negation with bitwise complement. Original setter is
byte-identical before the fix; every other pipeline byte stays unchanged.
Actual setter/enum/getter CPU controls use a named scalar-state facade, not a
viewer object:80checks expose44 baseline failures, then pass. They check the
real consumer flags, off/on/idempotence and zero/single/multiple/high/all bits.
Release textual syntax and independent source peer pass. No GPU behavior claim.
`/private/tmp/alchemy-native-render-feature-mask-validation-20261111/validation-receipt.json`.

The extended menu audit resolves164 callback names and preserves the original
core dispatch bodies; literal-reader presence alone had missed this side effect.
`/private/tmp/alchemy-native-dynamic-developer-completion-20261111/audit-receipt-frozen.json`.

## Avatar preview normal morph refresh

Integrated65b5cf966b from child87a21969de. Original upload-preview render calls
updateLOD then renderAvatars/renderSkinned. The latter updates dirty priority-one
morph geometry for the UI dummy's rank. Native extraction bypassed renderSkinned
and incorrectly assumed updateLOD did the final refresh; updateLOD refreshes
priority>=2/rebuild geometry, not every ordinary dirty morph. A warmed female/
male target change can therefore leave the old vertex geometry.

The narrow actual producer helper now updates LOD, then any remaining dirty or
rebuild geometry before native extraction. A read-only isMeshDirty getter exposes
the existing private predicate without fields/layout/friend changes. A named CPU
avatar/drawable facade compiles the actual helper:20checks expose3 priority-one
baseline failures, then pass. Clean and already-refreshed LOD cases avoid extra
updates; absent drawable stays pending. This does not construct a real avatar.
Production preview/avatar Release syntax and two independent peers pass. The
initial private-member compile failure is preserved; no access hack was used.
All ten original preview modes, camera/light/palette math and prior publication
cases remain. Sculpt is not claimed broken or changed.
`/private/tmp/alchemy-native-image-preview-geometry-validation-20261111/normal-validation-receipt.json`.

## Preview failed-upload admission and retry

Integrated158f60b4f0 from child4a8fdf11c0. Original drawShape retried buffer
publication through setBuffer. A failed rewrite can retain old non-null native
vertex/index handles with dirty current generations. The native preview's raw
pointer checks could render those old stores and clear preview dirty state.

Call actual setBuffer on each needed avatar render, then require independently
published current vertex and index stores before typed extraction. Readiness-only
rejection would lose the required no-new-write retry; both duties are retained.
The real LLVB + actual helper + native indexed LitPreview + actual image-adoption
fixture exposes16 baseline failures(116checks/1case), then passes114/1. Existing
four publication cases, normal CPU case and new generation case pass together:
1,860checks/6cases with Metal API/GPU validation. Previous image/content stamp
and dirty state survive failure; restoring the publisher without a new mesh write
publishes the pending store and changes rendered pixels. Clean opposite store
identity stays. This is not full avatar construction or actual UI invocation.
Production Release syntax and independent peers pass; no viewer/window action.
`/private/tmp/alchemy-native-image-preview-geometry-validation-20261111/generation-validation-receipt.json`.

## Native media RGB8 source layout and alpha

Integrateda380c729ab/8a14710e80 from child7027b60d3b/9068efb342. Original example
plugin declares RGB8 storage with RGBA/U8 depth4; optional build/install and
example:// dispatch remain supported. LLViewerMedia forwards that exact layout.
The native expansion advanced3bytes, making the second pixel start at the prior
alpha. Live CEF software/VLC/GStreamer BGRA/U8 aliases also need opaque stored
alpha, while alpha-bearing RGBA8 CEF storage must retain source alpha.

Private llimagegl_upload_native.inl mechanically shares actual CPU format/staging
code with the existing test TU. Correct U8 source stride and three-channel opaque
alpha without changing the valid plugin declaration, public/class ABI, source
row calculation, native resources, upload command or publication/worker lifetime.
BGRA retains its byte order matching native BGRA storage. One-byte swap is the
original no-op; wider inactive swapped RGB is outside this patch.

Exact final test source runs old-inl baseline599checks/2cases with112 failures,
then831/2pass. The baseline's early failed preparation skips later byte checks;
counts differ without weakening assertions. Independent peer corrected one
preservation oracle from inactive packed RGB8 to valid RGBA8 storage, then the
same final source was rerun red/green. Packed RGB8 opacity remains an inactive
candidate, not asserted correct. Real cpp Release syntax, full existing test TU
compile/link and final source peer pass. No device/GPU/media UI action in this
CPU proof. Root lists the private include as a private CMake source.
`/private/tmp/alchemy-native-image-upload-alias-validation-20261111/validation-receipt.json`.

## Isolated build routing

Integratedd053b16e6a fixes build.sh configure ignoring AL_METAL_BUILD_DIR. The
original wrapper also ignored it and would configure the preset's existing app
even when another directory was requested. Forward quoted -B, preserving the
preset/channel/platform guards and existing build/test routing. Actual-wrapper
argv regression fails before the fix; all50Python tool tests, shell syntax and
source peer pass after. No real CMake action is claimed by that stubbed test.

Actual separate configuration subsequently passes through both direct bootstrap
and the corrected wrapper at build-Darwin-ninja-os-arm64-followup. Existing
third-party installed dependencies are reused with AL_VCPKG_INSTALL=OFF; no
dependency installation or baseline build occurs. Its generated staging path is
the new tree, and the passed binary hash stays unchanged. Receipt:
`artifacts/metal/feature-completion-20261111/followup-isolated-configure-state.json`.
The root's separate appearance source audit also proves original-identical
process/params/skin/avatar and exact five-channel packing/J2C and bake-ID bodies;
actual optional utility execution remains unperformed.
`artifacts/metal/feature-completion-20261111/appearance-source-closure-audit.json`.

## GitHub request and next action

Owner explicitly requests pushing fixed source and triggering the release build.
Configured origin is private levdarkheart-lgtm/AlchemyFork; use the existing
codex/apple-silicon-metal branch, no force push/default merge/permission changes.
Existing dedicated Metal action packages native ARM64 open-source dependencies
and publishes a prerelease in that private fork. Use Tag a Build with Project,
Metal Alpha4 and a fresh exact26.4.0.<revision>-alpha4 override; generic auto tags
or a branch ref cannot enter that job. Never accept an automatic collision suffix.
The Metal release job now also gates the actual strict owned-source GL audit.

Owner configured the CLI; gh auth status outside the sandbox now confirms the
levdarkheart-lgtm account/keyring. Use gh for the dispatch and monitoring. No
remote push/tag/dispatch has occurred for these fixes yet. Do not
request credentials or alter repository security. Validate the combined separate
build/affected tests/static resources/source gate first, push the exact frozen
branch, trigger the existing release action and monitor its real run/artifacts.
Keep manual pass and CI evidence source-pinned. Whole-product runtime/feature/
stability/performance gates remain separate; no product-complete claim.
