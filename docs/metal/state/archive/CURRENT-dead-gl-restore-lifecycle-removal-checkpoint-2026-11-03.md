# Dead GL restore lifecycle removal checkpoint

**Observed:** 2026-11-03 on the physical M1 Max acceptance host.  
**Branch:** `codex/apple-silicon-metal`  
**Renderer source:** `dc368e4b80` (`Remove obsolete GL restore state`), after
`69d7a19f9a` (`Delete dead GL context restore lifecycle`)

## Implemented cutover

- `LLViewerWindow::restoreGL()` was explicitly deprecated, asserted false and
  had no caller. It and its declaration are deleted.
- `LLPipeline::restoreGL()` and `LLPipeline::destroyGL()` existed only for that
  uncalled recovery path. They and their declarations are deleted.
- `LLViewerWindow::stopGL()` has one process-shutdown caller. It runs after
  `gPipeline.cleanup()`, which makes the pipeline uninitialized, so its guarded
  `gPipeline.destroyGL()` branch was unreachable and is deleted.
- Pipeline cleanup now calls `releaseAllRenderResources()`. The name and
  comments state the actual lifetime: one shutdown release of native and
  retained resources, not a GL-context loss/rebuild contract.
- The only writer of `gRestoreGL` was the deleted viewer restore method.
  `gRestoreGL`, its timer, the five-second progress curtain, world-render
  suppression, cube-face assertion and media-control reshape condition are
  therefore deleted. Media controls still reshape for their live
  `mUpdateScrolls` condition.
- Initial pipeline resource construction is named `first_resources` and
  `initial resources`; live bump-map setup remains. The two source commits total
  23 insertions and 154 deletions.

This is lifecycle deletion, not runtime no-GL proof. The executable still
creates, uses and shuts down a GL context and still links OpenGL.

## Exact validation evidence

- `tools/metal/doctor.sh` exited 0 on `MacBookPro18,2`: Apple M1 Max, 64 GiB
  unified memory, arm64 macOS 26.5.2, Xcode 26.6, Apple7 support and the Metal
  compiler present.
- `tools/metal/build.sh` completed the warnings-as-errors Xcode Release build at
  committed source `dc368e4b80`. It linked, signed, validated and staged Alchemy
  Metal Development `26.4.0d68195` (`26.4.0.68195`).
- `tools/metal/verify_app.sh` checked 349/349 bundled Mach-O files as ARM64.
- Viewer executable SHA-256:
  `54fa795049b81c80007ad2736e851fdc355a67c6d8b3c3273b28d9861d21ba20`.
- Source and packaged `almetal_builtin.metal` are byte-identical at
  `54488f2d43e59f3fcbb72ebc916e03eb9f3883df027b2c18f8a2ca11e058b712`;
  packaged metallib SHA-256 is
  `5192ae1dd49cbc8f20c483ff2e8ecb8fbc8918ff365ea9348c824ccd3e7412e7`.
- With Metal API, GPU, shader and load-action validation enabled, the complete
  physical Metal suite passed 52,414 assertions across 55 cases.
- Full `alrender_tests` passed 557,312 assertions across 137 cases.
- The regenerated and relinked source-exact `newview_tests` binary reported
  build 68195 and passed 98,973 assertions across 743 cases: 742 passed and the
  established CRT case skipped. An initially invoked stale build-68158 binary
  was rejected and is not evidence for this checkpoint.
- Exact packaged-viewer symbol inspection finds no `gRestoreGL`,
  `LLViewerWindow::restoreGL`, `LLPipeline::restoreGL`,
  `LLPipeline::destroyGL` or `releaseAllGLResources`. It finds
  `LLPipeline::releaseAllRenderResources()`.
- `python3 tools/metal/audit_gl.py --mode strict --json --output
  artifacts/metal/gl-strict-lifecycle-cutover.json` exited 1 as expected at
  2,408 scanned files / 3,254 findings. This is one fewer than the retained
  preview checkpoint, without an audit-rule or suppression change. Report
  SHA-256:
  `0cd0dae0e3c15ee36848b3c27695c2699e1f88d2a0a508324d41767de197a8f3`.
- `otool -L` still finds both OpenGL and Metal in the packaged viewer. Strict
  linked and runtime no-GL gates remain open.

## Visual evidence boundary

No viewer executable was launched, driven, photographed or compared. This dead
lifecycle removal creates no new visual question, so no queue row was added.
Existing parity questions remain in the owner-attended daily queue.

## Next executable action

Delete the source-proven unreferenced restore-only sky, spatial-partition,
dynamic-texture, avatar and particle method cluster. Preserve live bump-map and
manipulator initialization, rebuild source-exactly, and use the strict inventory
to confirm the reduction before modifying live GL teardown.
