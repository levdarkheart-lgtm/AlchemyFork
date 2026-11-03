# Native legacy-bump feature checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer source:** `ec69e977e7` (`Make legacy bump textures Metal-native`)  
**Preceding cleanup:** `0fafef5f97` (`Delete disabled manipulator intersection path`)

## Implemented native feature owner

- The standard-bump loaded callback retains the existing alpha-to-normal CPU
  conversion and publishes its RGBA8 result directly to Metal on Darwin.
- Brightness- and darkness-derived maps retain the existing scale and polarity
  behavior through `ALMetalNormalMapGenerator`. The producer consumes the
  source's native texture, renders into a private Metal texture, generates its
  complete mip chain, commits the command buffer and publishes that result as
  the destination image's sole backing.
- `LLImageGL::adoptMetalRenderTextureAuthoritative()` reuses existing
  descriptor validation, retires an earlier GL name and makes native
  mip/memory/category metadata authoritative. It cannot silently preserve a
  second GL authority.
- Cache retention now checks native backing on Darwin. A fetched source that
  has not published to Metal fails closed and is retried later instead of
  allocating or sampling empty GL storage.
- Metal world-bump capture now tests `getMetalTexture()`, so a valid
  native-only generated map is not incorrectly replaced with the default.
- The former bump `restoreGL()` terminology is now `restoreResources()`.
  The retained `LLRenderTarget`, `gNormalMapGenProgram`, shader lifecycle
  and `normgenV/F.glsl` assets are deleted because their live behavior has a
  native replacement.
- A physical `LLImageGL` regression starts with GL and Metal backing, renders
  RGBA data into a four-level native texture, generates mips, publishes it
  authoritatively, proves the GL name is zero and reads level zero back
  byte-for-byte.

This is feature preservation through Metal, not feature removal for a smaller
OpenGL count.

## Dead-code boundary

Commit `0fafef5f97` separately removes calls, declaration and body for
`LLManipTranslate::highlightIntersection()`. Source inspection proved the
entire shipping body was already inside `#if 0`, so the method was a runtime
no-op. The live manipulator grid remains implemented and sampled through its
previous native authored-mip checkpoint.

The bump GLSL and target deletion is different: they were live before this
checkpoint and are removed only because the equivalent native producer,
publication and consumer path now exists and is tested.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on MacBookPro18,2: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` exited 0 at committed source `ec69e977e7`. Its
  warnings-as-errors Xcode Release build rebuilt 1,091 cacheable tasks, linked,
  signed, validated and staged Alchemy Metal Development `26.4.0d68205`
  (`26.4.0.68205`).
- The first incremental package inspection found two stale copies of the
  deleted `normgen*.glsl` assets. Only those exact regenerable Release outputs
  were removed. A second wrapper build re-signed successfully; source and
  packaged searches then found no copies, and deep strict signing passed.
- `tools/metal/verify_app.sh "build-Darwin-xcode-os-arm64/newview/Release/Alchemy
  Metal Development.app"` exited 0 and checked 349/349 bundled Mach-O files as
  ARM64.
- Viewer executable SHA-256:
  `86fb1fc0426edd0dc60f112ee424e883d23c15ee2ddbed62568140f9935e4645`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- The Ninja Release tree reconfigured at build 68205 and linked
  `llrender_gl_fresh_tests`, `almetal_tests`, `alrender_tests` and
  `newview_tests` warnings-as-errors.
- With `MTL_DEBUG_LAYER=1`, shader validation, load-action validation and
  `MTL_DEBUG_LAYER_ERROR_MODE=nslog`, `almetal_tests` passed 52,414
  assertions across 55 cases. The log explicitly reported Metal API and GPU
  validation enabled.
- Under the same environment, `llrender_gl_fresh_tests` passed 2,612
  assertions across 96 cases, including the new authoritative publication and
  physical readback regression.
- `alrender_tests` passed 557,312 assertions across 137 cases.
- `newview_tests` self-reported build 68205 and passed 98,973 assertions
  across 743 cases: 742 passed and the established CRT case skipped.
- Strict audit exited 1 as required while findings remain: 2,408 files / 3,233
  findings, down 18 from 3,251 with no rule or suppression change. The exact
  report is `artifacts/metal/gl-strict-native-bump-owner-68205.json`, SHA-256
  `5599cf095b7d7d2a6b92c1455327dae18648e57196753703f3ca3c9a799ca33e`.
- Exact packaged symbol inspection finds
  `LLBumpImageList::restoreResources()`, `ALMetalNormalMapGenerator` and
  `LLImageGL::adoptMetalRenderTextureAuthoritative()`; no bump
  `restoreGL()`, retained bump render target or normal-map GLSL program is
  present.
- `otool -L` still finds both OpenGL and Metal. Linked/runtime no-GL and all
  remaining feature/parity gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. The
standard/brightness/darkness bump presentation case is queued in
`docs/metal/work/active/visual-review-queue.md` for an owner-attended daily
review. Physical GPU evidence proves resource behavior, not final visual parity.

## Next executable action

Source-map and port `LLViewerTexLayerSetBuffer`, the live avatar
composite/bake dynamic-texture feature, including rendering, publication,
readback and failure retry. Remove its retained GL target only after equivalent
native behavior and deterministic/physical evidence exist.

