# Metal edit-grid authored-mip checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer source:** `db43ee336d` (`Port manipulator grid texture to Metal`)

## Implemented native feature owner

- `LLImageGL::createMetalTexture()` now accepts a complete caller-authored mip
  chain. It validates 2D/mipmapped state, the exact level count and dimensions
  down to 1x1, resolves the Metal format, retires GL/native predecessor storage,
  allocates private Metal storage and uploads every level in one command buffer.
  The published image retains a zero GL texture name.
- `LLManipTranslate` preserves the existing 512x512 through 1x1 RGBA8 grid
  generation algorithm, then publishes those ten levels through the native API.
  Both live draws sample the texture object with `ALSamplers::TrilinearWrap`;
  they no longer allocate, upload or bind a grid GL name.
- `FSManipTranslateJoint` inherits the base constructor and its render path does
  not sample a separate grid. Its duplicate static producer was therefore dead
  allocation/upload work, not a feature; it is deleted rather than replaced.
- A physical regression first rejects an incomplete chain, then uploads every
  level of a representative complete RGBA8 chain, releases all caller CPU
  storage and reads each mip back from the GPU byte-for-byte.

This is a real Metal implementation for the live edit grid. OpenGL elimination
is a release gate, not the product by itself; live rendering behavior is not
deleted as a substitute for native ownership.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on `MacBookPro18,2`: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `db43ee336d`. Its default
  warnings-as-errors Xcode Release build rebuilt 1,091 cacheable tasks, linked,
  signed, validated and staged Alchemy Metal Development `26.4.0d68202`
  (`26.4.0.68202`).
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as ARM64.
- Viewer executable SHA-256:
  `a886120e86917481f7eff068df4a7e028f24595c53c91454075b169cc893b311`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The exact Ninja Release tree was reconfigured at build 68202 and rebuilt the
  four required test targets with warnings-as-errors.
- With Metal API, GPU, shader and load-action validation enabled,
  `almetal_tests` passed 52,414 assertions across 55 cases.
- Under the same Metal validation environment,
  `llrender_gl_fresh_tests` passed 2,595 assertions across 95 cases, including
  the new authored-mip rejection/content/lifetime regression.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- `newview_tests` self-reported build 68202 and passed 98,973 assertions across
  743 cases: 742 passed and the established CRT case skipped.
- `python3 tools/metal/validate_harness.py` validated all 32 required files;
  `python3 -m unittest discover -s tools/metal/tests -v` passed all 27 tests.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-manipulator-grid-metal-68202.json` exited 1 as
  required while findings remain: 2,408 files / 3,251 findings, down two from
  3,253 with no audit-rule or suppression change. Report SHA-256:
  `27b00c663c6a70c570f8ff667e045230dafa5892095d209c8d17824ea3a45299`.
- Exact packaged symbol inspection finds `LLManipTranslate::getGridTexture()`
  and `releaseGridTexture()`, no manipulator `restoreGL()` symbols and no joint
  grid producer. `LLBumpImageList::restoreGL()` remains a separate live owner.
- `otool -L` still finds both OpenGL and Metal in the viewer; linked/runtime
  no-GL and all remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The edit
grid's alpha/color, wrap/tiling and mip-transition presentation question is
queued in `docs/metal/work/active/visual-review-queue.md` for a later
owner-attended daily review. Deterministic GPU evidence does not close that row.

## Next executable action

Delete the permanently compiled-out `highlightIntersection()` no-op contract;
its calls currently have no runtime effect. Then select the next live reachable
GL-backed feature and implement its actual Metal owner with deterministic and
physical validation rather than using deletion as a feature-port substitute.
