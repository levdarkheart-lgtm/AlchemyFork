# Native water-map checkpoint — 2026-11-03

## Observed source state

Commit `6e328d21daa75f63a39e4a45f5dbae287427a363` removes OpenGL
ownership from the Darwin `ALWaterSurface` procedural cell map and baked wave
displacement maps. The cell map now allocates as RG32F through Metal and sends
later full replacements through the native subimage path. The baked height
maps now allocate as RG16F through Metal with a complete generated mip chain.
Neither path allocates or mutates a GL texture name on Darwin.

`LLImageGL` typed native creation now honors `mUseMipMaps`: it allocates the
full level count, uploads level zero and generates the remaining levels in the
same command buffer before publication. Existing non-mip typed creation and
same-coordinate subimage behavior remain unchanged.

Water publication is fail-closed. A failed cell-map upload drops the incomplete
map without advancing the source key, so the next update retries. A failed
baked-map upload retains its CPU texels so `trackMaps` retries instead of
permanently flattening that wave. Existing GL-owned or missing-native cell maps
are rebuilt native-only.

A physical regression creates a 4x4 RG16F checkerboard with three native mip
levels and no GL name. Exact readback proves the base texels and the averaged
2x2 and 1x1 levels, including half-float `0x3800` for 0.5. No viewer was
launched or driven. The water appearance question is recorded in the
owner-attended daily review queue rather than inferred from deterministic
validation.

## Validation evidence

- The required `tools/metal/build.sh` command exited 0 at exact source
  `6e328d21daa75f63a39e4a45f5dbae287427a363`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68135` (`26.4.0.68135`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 1,704 assertions across 92 cases with Metal API Validation enabled.
  This includes exact typed RG16F base and generated-mip readback plus the
  prior native typed-allocation and region-update regressions.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68135.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68135.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `cad32178214983cf4a9905054bf6887df19b422610bba31732e221f91a89692d`.
- Appearance-utility SHA-256:
  `5769d937be848ee0b6edf9c153eefb0b0107898f90bc42fb377399e2ff7b337e`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/validate_harness.py` passed all 32 required files, and
  `git diff --check` passed before this documentation update.
- Strict inventory is 2,408 files / 3,258 findings and exits 1 as expected.
  Relative to the terrain checkpoint, two native `LLImageGL` allocation
  snippets replace one prior one-line allocation snippet. The net increase of
  one is line-wrapping/symbol matching only; no GL call site was added. The
  precommit and exact-source reports are byte-identical at SHA-256
  `87e5bec2875149fdb44878bad14add7bafa1651ecdf53dedcce2cbb63e1ebf44`.
  The ignored exact report is
  `artifacts/metal/gl-strict-native-water-maps.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use `LLViewerParcelOverlay` as the next bounded resource owner. Its RGBA overlay
is consumed by retained terrain and explicit native terrain captures, but its
completed CPU image still allocates and uploads through GL. Preserve the
existing `LLViewerTexture` identity, make Darwin allocation/update native-only,
and retain the rebuild index on failure so idle processing retries rather than
publishing an incomplete overlay. Do not launch either viewer; keep the parcel
overlay visual question in the daily owner-attended queue after implementation.
