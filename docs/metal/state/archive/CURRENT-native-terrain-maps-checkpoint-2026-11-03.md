# Native terrain-map checkpoint — 2026-11-03

## Observed source state

Commit `cb59cc05a368b756b86c1b525ea3a53adbc288b5` removes OpenGL
ownership from the Darwin `ALTerrainSurfaceMaps` height and composition
resources. The R32F height map and RG16F composition map now allocate and
upload only through Metal. Full refreshes and dirty rectangles use the native
path; an existing GL-owned image or one missing its Metal texture is rebuilt as
native-only. The retained sampled-binding traversal continues to see the
established fallback, while pipeline terrain and shadow captures consume the
native textures directly.

`LLImageGL` now exposes a Darwin typed native creation overload for production
formats whose byte layout cannot be represented by `LLImageRaw`, plus a
same-coordinate native subimage upload. The existing raw-image creation path
delegates to the typed implementation after establishing geometry. Terrain
refresh/upload reports success and clears dirty state only after both resources
upload successfully; a failure remains dirty and retries rather than silently
publishing incomplete state.

Physical regressions prove exact R32F creation and partial update, exact RG16F
conversion from float input and partial update, unchanged texels outside both
regions, and absence of a GL texture name. No viewer was launched or driven.
The terrain appearance question is recorded in the owner-attended daily review
queue rather than inferred from deterministic validation.

## Validation evidence

- The required `tools/metal/build.sh` command exited 0 at exact source
  `cb59cc05a368b756b86c1b525ea3a53adbc288b5`, using the default Apple
  Silicon Xcode preset and Release configuration with warnings as errors. It
  produced Development build `26.4.0d68133` (`26.4.0.68133`).
- `cmake --build build-Darwin-ninja-os-arm64 --target almetal_tests
  llrender_gl_fresh_tests llrender_gl_shared_tests newview_tests -j 10`
  exited 0 and linked the source-exact RelWithDebInfo physical test binaries.
- With `MTL_DEBUG_LAYER=1`, the complete source-exact `almetal_tests` binary
  passed 52,221 assertions across 52 cases with Metal API Validation enabled.
- With `MTL_DEBUG_LAYER=1`, the source-exact `llrender_gl_fresh_tests` binary
  passed 1,692 assertions across 91 cases with Metal API Validation enabled.
  This includes exact R32F/RG16F typed allocation, conversion, partial-region
  update, outside-region preservation and no-GL-name regressions.
- The source-exact `newview_tests` binary passed 98,973 assertions across 743
  cases: 742 passed and one platform CRT case skipped as expected. Its compiled
  and runtime build numbers both report 68133.
- The source-exact broad `llrender_gl_shared_tests` run passed 8,335/8,340
  assertions and 181/224 cases, with four failures and 39 skips. The failures
  remain the established three frame-fence pacing/budget cases and one
  depth-format case; exit 42 is recorded as failed, not promoted to green.
- `cmake --build build-Darwin-ninja-os-arm64-appearance --target
  appearance-utility-bin -j 10` exited 0 with warnings as errors at source
  build 68133.
- `tools/metal/verify_app.sh` passed all 349 bundled Mach-O slices as ARM64.
- Viewer executable SHA-256:
  `02b826dfacd8a42afe3b579d9a2bd7d1537f4760a1183ceae8a5bd4168150e19`.
- Appearance-utility SHA-256:
  `3cfba52ae7d0fff7b6cb62bab25fcc44057a1b50e05c18cd5a96b143ae451ca5`.
- Source and packaged MSL are byte-identical at
  `fa6821703a980b570305fc2384fcb11a1980fccad43d35e4626e40cc4b323324`;
  packaged metallib SHA-256 is
  `ebd7034235c4f35b13c9874029eff6bbcb61cc3f56c7f3907fd581c112eb96ab`.
- `python3 tools/metal/validate_harness.py` passed all 32 required files, and
  `git diff --check` passed.
- Strict inventory is 2,408 files / 3,257 findings and exits 1 as expected.
  Compared semantically with the prior checkpoint, five heuristic snippets
  were added and two old `void` upload-signature snippets were replaced. The
  net increase of three is entirely native-method/symbol matching; no GL call
  site was added. The ignored exact report is
  `artifacts/metal/gl-strict-native-terrain-maps.json`.
- The packaged viewer has no undefined `CGLTexImageIOSurface2D` dependency.
  `otool -L` still reports both Apple OpenGL and Metal in the packaged viewer
  and source-exact appearance utility. The strict no-GL gate is not met.
- No viewer executable was launched, driven, photographed or compared.

The product remains incomplete. Application-owned GL context, traversal and
resources remain; complete parity, prolonged stability, independent
build/review evidence, strict no-GL and measured performance gates remain open.

## Next executable boundary

Use `ALWaterSurface` as the next bounded resource owner. Its procedural RG32F
cell map and baked RG16F displacement maps already have explicit Metal
consumers, but both still allocate and update through GL. Convert the cell map
to typed native allocation/update, extend typed creation for production mip
chains, and cut the displacement maps over with physical base/mip validation.
Do not launch either viewer; keep visual questions in the daily owner-attended
queue.
