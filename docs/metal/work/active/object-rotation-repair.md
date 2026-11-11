# Edit Rotate: native Debug geometry admission and culling

The owner reported that selecting Rotate appeared to do nothing and no rotation
rings appeared. The viewer must remain closed; no live rotation acceptance or
screenshot comparison was performed for this repair.

## Original contract and confirmed divergence

Reference: exact original Alchemy revision
`1eb44bbd1bdc76d0100c75699bad1fb337e8cb45`. The edit floater radio callback,
tool selection and rotation math match that reference. `LLManipRotate::render`
binds `gDebugProgram`; its washer and circle helpers emit immediate strips and
fans. Original `class1/interface/debugV.glsl` consumes position XYZ with W1;
`debugF.glsl` consumes uniform color, without texture or vertex-color inputs.

Native `LLGLSLShader::configureNative` cannot reflect GLSL attributes and copies
the explicitly declared native mask. The Debug loader omitted that declaration,
leaving mask0. `LLRender::flush` then failed immediate buffer admission because
`MAP_VERTEX` was missing, making the authoritative UI capture incomplete. The
previous published UI can remain visible, explaining why even the mode change
can appear absent. This is a rendering defect; no permission or tool-input gate
was added.

The saved live log corroborates the source path: 120 emitted mask0/Debug Shader
rejections, including 119 between23:48:33 and23:51:10 (106 at202 vertices and13
at102 vertices, matching the production100-step washer/fan). Fourteen emitted
incomplete-capture receipts occur in that interval. These are throttled log
receipts, not frame counts or exact click timestamps. Twenty successful UI
publications are interspersed; this does not prove one continuous158-second
freeze. Two earlier resize warnings are separate evidence.

The immediate bridge also copied cull state only for indexed geometry. Original
culling applies to nonindexed primitives too. The existing native cull mapping
now applies to both; native offscreen/surface winding handling is unchanged.

## Implementation and validation

- `llviewershadermgr.cpp`: declare position-only native Debug attributes before
  shader creation. Uniform color and rigged/debug ownership remain unchanged.
- `llrender.cpp`: copy existing cull state outside the indexed-only block.
- Darwin `llrender_tests`: add a real native configuration/immediate-capture
  fixture using production washer/circle helpers and independent triangle oracles.

Actual M1 Max/64GiB/macOS26.5.2/Xcode26.6 commands and exit codes:

| Command | Result |
| --- | --- |
| `tools/metal/doctor.sh` | exit0 |
| `cmake --build build-Darwin-ninja-os-arm64 --config Release --target llrender_tests --parallel 4` | exit0 |
| `env MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/llrender/Release/llrender_tests '[debug-immediate]'` | exit0;62 assertions/1case |
| Same frozen test rebuilt against previous `LLRender` cull implementation | build0; test42 with4 failures/62 assertions |
| Restore candidate and rebuild same test target | exit0 |
| `env MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build-Darwin-ninja-os-arm64/llrender/Release/llrender_tests '[debug-immediate],[native_ui],[metal-cutover]'` | exit0;419 assertions/14cases |

The omitted-layout control exercises the real CPU rejection and abandoned UI
transaction, followed by a healthy retry. Full64x64 RGBA comparisons cover a
four-step washer/fan with independently authored triangles, visible pixels and
the washer hole. Opposite-winding nonindexed triangles verify front/back culling.
This small oracle does not reproduce the complete100-step live manipulator.

The test target cannot link the newview shader loader: it declares the same
layout and exercises actual `configureNative` through GPU output. Loader wiring
is source-reviewed and compiled by the canonical app build; the test alone does
not prove that callsite or user mouse handling. Independent source/test reviewers
approved the bounded changes. No FPS improvement or complete editing parity is
claimed. Live object rotation, mouse hit testing and simulator updates remain
unverified.

Private evidence: `artifacts/metal/private-object-rotate-audit-20261110-01/`.
`run.py` receipts record exact argv, exits, source SHA and dirty paths. The frozen
test SHA256 is `a196793fceb019e5853a199d08605a6a8c4fe753f5dcde4300db1033b420d978`;
`cull-red-control.json` records baseline/candidate hashes and restoration. Saved
live log SHA256 is
`1a4dd3be28edb2b5ebff908ccedab04b3e4ee61fdec7ff2dec08ebd71f00cd6a`.
Raw logs/captures remain ignored and private. Final canonical build identity and
handoff are recorded in `state/CURRENT.md` after the source commit.

Canonical app follow-through: code checkpoint
`7710ae4f977295d7c3539516a2d3814485a7db45`, Release26.4.0.68648.
`tools/metal/build.sh configure`, `tools/metal/build.sh build`,
`tools/metal/verify_app.sh <canonical app>` and deep strict `codesign --verify`
all exit0;349 Mach-O files ARM64. Exact expanded argv and hashes are in
`final-build-identity.json`. Production loader and LLRender objects were compiled.
No viewer was launched for these checks; final viewer process census was empty.
